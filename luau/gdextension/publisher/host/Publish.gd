# COPY -- do not edit. The original is luau/gdextension/host/Publish.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# Publishes a place: files flattened as ScriptSync mounts them, stored in AssetStore by content
# hash, then a manifest of uris -- its uri is the experience the Player mounts. A deliberate twin
# of scripts/publish-experience.js: same flattening, same uri format, but no Node, no checkout.
extends Node

signal progress(done: int, total: int, what: String)

const Abi = preload("res://host/Abi.gd")
const Uses = preload("res://host/Uses.gd")

## The suffixes ScriptSync keeps on an instance's name, longest first.
const SUFFIXES := [".server.luau", ".server.lua", ".client.luau", ".client.lua",
	".luau", ".lua", ".model.json", ".meta.json", ".rbxmx", ".rbxm"]
const MAX_CHUNK := 24575          # AssetStore.MAX_CHUNK
const CHUNK_BYTES := 24000        # two to a transaction
const MAX_TX_DATA := 49152        # PulseChain refuses more calldata than EIP-3860's initcode limit
const STORE_GAS_PER_BYTE := 260   # code deposit is 200 a byte; calldata and copying the rest

## The key holder (host/Wallet.gd). A publish is dozens of transactions with no prompt each: the
## question is asked once over plan(). No place reaches this file -- Wallet's allowlist omits it.
var wallet: Node
## Which AssetStore to write to. Empty: the wallet's own (Wallet.ADDRESSES.AssetStore).
var store := ""

func _store_address() -> String:
	return store if store != "" else String(wallet.ADDRESSES.AssetStore)

# ---- what a place is --------------------------------------------------------------------------------
static func suffix_of(file: String) -> String:
	for s in SUFFIXES:
		if file.ends_with(s):
			return s
	return ""

static func _walk(base: String, rel: String = "") -> Array:
	var out := []
	var here := base.path_join(rel) if rel != "" else base
	var d := DirAccess.open(here)
	if d == null:
		return out
	d.include_hidden = false
	d.list_dir_begin()
	var n := d.get_next()
	while n != "":
		var child := rel.path_join(n) if rel != "" else n
		if d.current_is_dir():
			if not n.begins_with("."):
				out.append_array(_walk(base, child))
		elif suffix_of(n) != "":
			out.append(child)
		n = d.get_next()
	out.sort()
	return out

static func _node_meta(node: Dictionary) -> String:
	var meta := {}
	if node.has("$className"): meta["className"] = node["$className"]
	if node.has("$properties"): meta["properties"] = node["$properties"]
	if node.has("$attributes"): meta["attributes"] = node["$attributes"]
	return JSON.stringify(meta, "", false) if not meta.is_empty() else ""

static func _mount(node, inst: String, dir: String, files: Dictionary) -> void:
	if typeof(node) != TYPE_DICTIONARY:
		return
	var is_file := false
	if node.has("$path") and typeof(node["$path"]) == TYPE_STRING:
		var disk := dir.path_join(String(node["$path"]))
		if FileAccess.file_exists(disk):
			files[inst + suffix_of(disk)] = {"file": disk}
			is_file = true
		else:
			for rel in _walk(disk):
				files[inst.path_join(rel) if inst != "" else rel] = {"file": disk.path_join(rel)}
	if inst != "":
		var meta := _node_meta(node)
		if meta != "":
			var name := inst + ".meta.json" if is_file else inst.path_join("init.meta.json")
			if not files.has(name):
				files[name] = {"inline": meta}
	for k in node.keys():
		if not String(k).begins_with("$"):
			_mount(node[k], inst.path_join(String(k)) if inst != "" else String(k), dir, files)

## A place folder as instance path -> {file} or {inline}: with a default.project.json, its tree read
## the way Rojo and ScriptSync read it; without one, the folder is the DataModel.
static func resolve(dir: String) -> Dictionary:
	var files := {}
	var project := dir.path_join("default.project.json")
	if FileAccess.file_exists(project):
		var tree = JSON.parse_string(FileAccess.get_file_as_string(project))
		if typeof(tree) != TYPE_DICTIONARY or not tree.has("tree"):
			return {"error": "%s has no tree" % project}
		_mount(tree.tree, "", dir, files)
	else:
		for rel in _walk(dir):
			files[rel] = {"file": dir.path_join(rel)}
	return files

static func mime_for(p: String) -> String:
	if p.ends_with(".json"): return "application/json"
	if p.ends_with(".rbxm") or p.ends_with(".rbxmx"): return "application/octet-stream"
	return "text/x-lua"

static func image_mime(path: String, allow_webp: bool = true) -> String:
	var ext := path.get_extension().to_lower()
	if ext == "jpg" or ext == "jpeg": return "image/jpeg"
	if ext == "webp" and allow_webp: return "image/webp"
	return "image/png"

## Capabilities the scripts look like they ask for: a hint for the creator, not the declaration.
static func uses_hint(contents: Dictionary) -> PackedStringArray:
	var text := ""
	for p in contents:
		if String(p).ends_with(".luau") or String(p).ends_with(".lua"):
			text += (contents[p] as PackedByteArray).get_string_from_utf8() + "\n"
	var found := PackedStringArray()
	for cap in Uses.ALL:
		for action in Uses.ALL[cap].actions:
			var re := RegEx.create_from_string("action\\s*=\\s*\"%s\"" % action)
			if re.search(text) != null and not found.has(cap):
				found.append(cap)
	if text.contains("AskScan") and not found.has("scan"): found.append("scan")
	if text.contains("AskMarket") and not found.has("market"): found.append("market")
	return found

## Everything a publish would put on chain, read off disk, with nothing sent and nothing asked.
## opts: {name, description, uses: Array, thumbnail: path, splash: path, assets: path to a
## name -> pblockz:// uri json}
static func read_place(dir: String, opts: Dictionary) -> Dictionary:
	var name := String(opts.get("name", "")).strip_edges()
	if name == "":
		return {"ok": false, "error": "a place needs a name"}
	var uses := PackedStringArray()
	for u in opts.get("uses", []):
		if not Uses.ALL.has(String(u)):
			return {"ok": false, "error": "\"%s\" is not a capability (%s)" % [u, ", ".join(Uses.ALL.keys())]}
		if not uses.has(String(u)):
			uses.append(String(u))
	var files := resolve(dir)
	if files.has("error"):
		return {"ok": false, "error": files.error}
	for p in files:
		if RegEx.create_from_string("(^|/)ContractsDev\\.luau?$").search(String(p)) != null:
			return {"ok": false, "error": "%s is a dev chain's override; stop the dev chain before publishing" % p}
	var paths: Array = files.keys()
	paths.sort()
	if paths.is_empty():
		return {"ok": false, "error": "nothing to publish under %s" % dir}
	var contents := {}
	var total := 0
	for p in paths:
		var e: Dictionary = files[p]
		var bytes: PackedByteArray = String(e.inline).to_utf8_buffer() if e.has("inline") else FileAccess.get_file_as_bytes(String(e.file))
		contents[p] = bytes
		total += bytes.size()
	var out := {"ok": true, "name": name, "description": String(opts.get("description", "")), "uses": uses,
		"paths": paths, "contents": contents, "bytes": total, "pictures": {}, "assets": {}}
	for which in ["thumbnail", "splash"]:
		var at := String(opts.get(which, ""))
		if at != "":
			if not FileAccess.file_exists(at):
				return {"ok": false, "error": "no %s at %s" % [which, at]}
			out.pictures[which] = {"bytes": FileAccess.get_file_as_bytes(at), "mime": image_mime(at, which == "thumbnail")}
	var assets_file := String(opts.get("assets", ""))
	if assets_file != "":
		var parsed = JSON.parse_string(FileAccess.get_file_as_string(assets_file)) if FileAccess.file_exists(assets_file) else null
		if typeof(parsed) != TYPE_DICTIONARY:
			return {"ok": false, "error": "%s is not a name -> uri object" % assets_file}
		for k in parsed:
			if String(k).begins_with("_"):
				continue
			if not (String(parsed[k]).begins_with("pblockz://")):
				return {"ok": false, "error": "place asset %s is not a pblockz:// uri" % k}
			out.assets[String(k)] = String(parsed[k])
	var looks := uses_hint(contents)
	var missing := PackedStringArray()
	for u in looks:
		if not uses.has(u):
			missing.append(u)
	out["undeclared"] = missing
	return out

# ---- the chain ------------------------------------------------------------------------------------
## Which of the place's bytes are already in the store. Adds to `place`: kept {path -> uri},
## to_store [paths], transactions (an estimate) and gas (an estimate). Reads only.
func plan(place: Dictionary) -> Dictionary:
	if wallet == null:
		return {"ok": false, "error": "no wallet to read the chain with"}
	var target := _store_address()
	var items := []          # [key, bytes, mime]
	for p in place.paths:
		items.append([p, place.contents[p], mime_for(p)])
	for which in place.pictures:
		items.append(["<%s>" % which, place.pictures[which].bytes, place.pictures[which].mime])
	var calls := []
	for it in items:
		calls.append({"to": target, "fn": "blobOf(bytes32)", "args": [String(PulseBlockzChain.content_hash_of(it[1]))], "returns": ["uint256"]})
	var found: Dictionary = await wallet.chain_read_many(calls)
	if not found.get("ok", false):
		return {"ok": false, "error": "the chain did not answer: %s" % found.get("message", "")}
	var ids := []
	var infos := []
	for i in items.size():
		var r: Dictionary = found.results[i]
		var id := int(String(r.words[0]) if r.get("ok", false) and r.words.size() > 0 else "0")
		ids.append(id)
		if id > 0:
			infos.append({"to": target, "fn": "blob(uint256)", "args": [id], "returns": ["address", "bytes32", "uint256", "string"]})
	var blobs := {}
	if not infos.is_empty():
		var got: Dictionary = await wallet.chain_read_many(infos)
		if not got.get("ok", false):
			return {"ok": false, "error": "the chain did not answer: %s" % got.get("message", "")}
		var j := 0
		for i in items.size():
			if ids[i] > 0:
				blobs[i] = got.results[j]
				j += 1
	var kept := {}
	var to_store := []
	var txs := 1            # the manifest
	var gas := 0
	for i in items.size():
		var bytes: PackedByteArray = items[i][1]
		var mime: String = items[i][2]
		var b = blobs.get(i)
		if b != null and b.get("ok", false) and b.words.size() >= 4 and int(String(b.words[2])) == bytes.size() and String(b.words[3]) == mime:
			kept[items[i][0]] = PulseBlockzChain.format_asset_uri({"content_hash": PulseBlockzChain.content_hash_of(bytes),
				"has_chain": true, "chain_id": int(wallet.ADDRESSES.chain_id), "store": target, "blob_id": ids[i], "mime": mime})
			continue
		to_store.append(items[i][0])
		var parts := _chunks(bytes)
		txs += 1 if _fits_one(parts, mime) else _batches(parts, 0).size() + 1
		gas += bytes.size() * STORE_GAS_PER_BYTE + parts.size() * 45000 + 60000
	gas += 400000     # the manifest, roughly
	place["kept"] = kept
	place["to_store"] = to_store
	place["transactions"] = txs
	place["gas"] = gas
	place["store"] = target
	place["planned"] = true
	return {"ok": true}

## Stores what plan() found missing, then the manifest. Returns {ok, uri, stored, kept, txs, error}.
## Safe to run again after a failure: whatever landed is found by plan() and kept.
func publish(place: Dictionary) -> Dictionary:
	if wallet == null or not wallet.can_buy():
		return {"ok": false, "error": "no key loaded to sign with"}
	if not place.get("planned", false):
		var p := await plan(place)
		if not p.ok:
			return p
	var uris: Dictionary = (place.kept as Dictionary).duplicate()
	var total: int = place.to_store.size() + 1
	var done := 0
	var txs := []
	for key in place.to_store:
		var bytes: PackedByteArray
		var mime: String
		if String(key).begins_with("<"):
			var which := String(key).trim_prefix("<").trim_suffix(">")
			bytes = place.pictures[which].bytes
			mime = place.pictures[which].mime
		else:
			bytes = place.contents[key]
			mime = mime_for(key)
		progress.emit(done, total, String(key))
		var stored := await store_bytes(bytes, mime)
		if not stored.ok:
			return {"ok": false, "error": "%s: %s" % [key, stored.error], "txs": txs}
		txs.append_array(stored.txs)
		uris[key] = stored.uri
		done += 1
	var files := []
	for p in place.paths:
		files.append({"path": p, "uri": uris[p], "bytes": (place.contents[p] as PackedByteArray).size()})
	var published := int(Time.get_unix_time_from_system())
	var manifest := {
		"kind": "experience",
		"name": place.name,
		"description": place.description,
		# Bare content hash, not a full uri: a link naming a store would make a moved store a
		# reason to republish. The files below keep full uris -- a fetch plan for one
		# deployment, read once at join and cheaper with the blob id in hand.
		"thumbnail": _content_hash(uris.get("<thumbnail>", "")),
		"splash": _content_hash(uris.get("<splash>", "")),
		"uses": Array(place.uses),
		"published": published,
		"publisher": wallet.wallet_address,
		"files": files,
	}
	if not (place.assets as Dictionary).is_empty():
		manifest["assets"] = place.assets
	progress.emit(done, total, "the manifest")
	var body := JSON.stringify(manifest, "", false).to_utf8_buffer()
	var m := await store_bytes(body, "application/json")
	if not m.ok:
		return {"ok": false, "error": "the manifest: %s" % m.error, "txs": txs}
	txs.append_array(m.txs)
	progress.emit(total, total, "done")
	return {"ok": true, "uri": m.uri, "name": place.name, "files": files.size(), "bytes": place.bytes,
		"published": published, "stored": place.to_store.size(), "kept": (place.kept as Dictionary).size(), "txs": txs}

## The content hash out of a uri, or "" for no uri.
static func _content_hash(uri: String) -> String:
	if uri == "":
		return ""
	return String(PulseBlockzChain.parse_asset_uri(uri).get("content_hash", ""))

## Bytes into the store, in as many transactions as they need. {ok, uri, blob_id, txs, error}
func store_bytes(bytes: PackedByteArray, mime: String) -> Dictionary:
	if bytes.is_empty():
		return {"ok": false, "error": "nothing to store"}
	var target := _store_address()
	var hash: String = PulseBlockzChain.content_hash_of(bytes)
	var parts := _chunks(bytes)
	var txs := []
	var blob_id := 0
	if _fits_one(parts, mime):
		var data := Abi.selector("storeAndPublish(bytes32,uint32,string,bytes[])") + Abi.encode(
			["bytes32", "uint32", "string", "bytes[]"], [hash, bytes.size(), mime, parts])
		var sent := await _send(target, data, bytes.size() * STORE_GAS_PER_BYTE + parts.size() * 45000 + 200000, "Storing a file")
		if not sent.ok:
			return {"ok": false, "error": sent.error, "txs": txs}
		txs.append(sent.hash)
		blob_id = _blob_id(sent.receipt, target)
	else:
		var chunks := []
		for batch in _batches(parts, 0):
			var data := Abi.selector("storeMany(bytes[])") + Abi.encode(["bytes[]"], [batch])
			var size := 0
			for b in batch:
				size += (b as PackedByteArray).size()
			var sent := await _send(target, data, size * STORE_GAS_PER_BYTE + batch.size() * 45000 + 60000, "Storing part of a file")
			if not sent.ok:
				return {"ok": false, "error": sent.error, "txs": txs}
			txs.append(sent.hash)
			var landed := _chunk_addresses(sent.receipt, target)
			if landed.size() != batch.size():
				return {"ok": false, "error": "stored %d chunks but the chain reported %d" % [batch.size(), landed.size()], "txs": txs}
			chunks.append_array(landed)
		var data := Abi.selector("publish(bytes32,uint32,string,address[])") + Abi.encode(
			["bytes32", "uint32", "string", "address[]"], [hash, bytes.size(), mime, chunks])
		var sent := await _send(target, data, 150000 + chunks.size() * 30000 + bytes.size() * 8, "Naming a stored file")
		if not sent.ok:
			return {"ok": false, "error": sent.error, "txs": txs}
		txs.append(sent.hash)
		blob_id = _blob_id(sent.receipt, target)
	if blob_id <= 0:
		return {"ok": false, "error": "stored, but the chain did not say under what id", "txs": txs}
	return {"ok": true, "blob_id": blob_id, "txs": txs, "uri": PulseBlockzChain.format_asset_uri({
		"content_hash": hash, "has_chain": true, "chain_id": int(wallet.ADDRESSES.chain_id),
		"store": target, "blob_id": blob_id, "mime": mime})}

func _send(to: String, data: String, fallback_gas: int, what: String = "Publishing") -> Dictionary:
	var sent: Dictionary = await wallet.send_prepared(to, data, fallback_gas, what)
	if not sent.get("ok", false):
		return {"ok": false, "error": String(sent.get("message", "not sent"))}
	return {"ok": true, "hash": String(sent.hash), "receipt": sent.get("receipt", {})}

static func _chunks(bytes: PackedByteArray) -> Array:
	var out := []
	for i in range(0, bytes.size(), CHUNK_BYTES):
		out.append(bytes.slice(i, min(i + CHUNK_BYTES, bytes.size())))
	return out

static func _pad32(n: int) -> int:
	return int(ceil(n / 32.0)) * 32

## The exact calldata size of a call whose last argument is these chunks as bytes[].
static func _calldata_size(sizes: Array, head: int) -> int:
	var total := 4 + head + 32 + 32
	for n in sizes:
		total += 32 + 32 + _pad32(int(n))
	return total

static func _fits_one(parts: Array, mime: String) -> bool:
	var sizes := parts.map(func(p): return (p as PackedByteArray).size())
	return _calldata_size(sizes, 32 * 4 + _pad32(mime.to_utf8_buffer().size())) <= MAX_TX_DATA

static func _batches(parts: Array, head: int) -> Array:
	var out := []
	var cur := []
	for part in parts:
		var sizes := (cur + [part]).map(func(p): return (p as PackedByteArray).size())
		if not cur.is_empty() and _calldata_size(sizes, head) > MAX_TX_DATA:
			out.append(cur)
			cur = []
		cur.append(part)
	if not cur.is_empty():
		out.append(cur)
	return out

static func _blob_id(receipt: Dictionary, store_address: String) -> int:
	var topics := Abi.find_log(receipt, store_address, "BlobPublished(uint256,address,bytes32,uint32,string,uint256)")
	return String(topics[1]).trim_prefix("0x").hex_to_int() if topics.size() >= 2 else 0

static func _chunk_addresses(receipt: Dictionary, store_address: String) -> Array:
	var want := Abi.topic("ChunkStored(address,uint32)").to_lower()
	var out := []
	for log in receipt.get("logs", []):
		if typeof(log) != TYPE_DICTIONARY or String(log.get("address", "")).to_lower() != store_address.to_lower():
			continue
		var topics: Array = log.get("topics", [])
		if topics.size() >= 2 and String(topics[0]).to_lower() == want:
			out.append("0x" + String(topics[1]).right(40))
	return out

# ---- the listing ------------------------------------------------------------------------------------
## Adds a published place to an experiences listing file, the shape publish-experience.js writes.
static func record(listing_path: String, result: Dictionary) -> void:
	var all := {}
	if FileAccess.file_exists(listing_path):
		var parsed = JSON.parse_string(FileAccess.get_file_as_string(listing_path))
		if typeof(parsed) == TYPE_DICTIONARY:
			all = parsed
	all[String(result.name)] = {"uri": result.uri, "files": int(result.files), "bytes": int(result.bytes), "published": int(result.published)}
	var f := FileAccess.open(listing_path, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(all, "  ", false) + "\n")
		f.close()
