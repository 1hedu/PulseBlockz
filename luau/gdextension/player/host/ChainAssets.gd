# COPY -- do not edit. The original is luau/gdextension/host/ChainAssets.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# Fetches pblockz:// assets (ASSETS.md) from the chain through AssetStore or from a uri's
# mirrors, verifying the content hash and caching by hash. Autoload as `ChainAssets`; needs
# PulseBlockzChain from the GDExtension. JSON-RPC goes to `rpc_url` over HTTPRequest.
extends Node

signal fetched(uri: String, ok: bool)

var rpc_url: String = "http://127.0.0.1:8545"   # a PulseChain RPC in production
var chain_id: int = 31337
var cache_dir: String = "user://assets"
var curation: Node = null                        # the Curation autoload, if present; a hidden uri is refused
## The store asked where bare content lives. `pblockz://<hash>` with nothing after it names
## content and no location, so the client supplies the where: `blobOf(hash)` gives the blob
## holding those bytes. A link that names its own chain and blob is read as given.
var asset_store: String = ""
## Hosts a `?src=<url>` mirror may be fetched from; empty, the default, means none. The hash
## check keeps bad bytes out, but the GET goes out from the player's machine to a url the uri's
## author wrote. The place gates this too, with `Uses.permits("mirror")`: see _mirror_allowed.
## An empty list breaks nothing shipped: no asset in the catalogue, in place-assets.json or in
## the on-chain manifest carries a `src=` -- each is content-addressed and comes from the
## AssetStore. SAFETY.md is the promise this gate keeps: a place reaches no undeclared network.
var mirrors_allowed: PackedStringArray = PackedStringArray()
var _located := {}                               # content hash -> {blob, mime, size}, one lookup per hash

var _inflight := {}   # content_hash -> true while a fetch is in flight
# Reused: a fresh node per call costs a fresh TLS handshake. Take one through _lanes_for -- two
# callers finding the same idle node in a frame get ERR_BUSY (44) on the second request().
const LANES := 6
var _lanes: Array[HTTPRequest] = []
var _busy := {}       # instance id -> true while a lane is taken

func _ready() -> void:
	DirAccess.make_dir_recursive_absolute(cache_dir)
	for i in LANES:
		var lane := HTTPRequest.new()
		lane.use_threads = true   # handshake and download off the frame (see Rpc.gd)
		add_child(lane)
		_lanes.append(lane)
	if curation == null and has_node("/root/Curation"):
		curation = get_node("/root/Curation")

## Up to `want` free lanes, never fewer than one: the first is waited for, the rest taken only
## if idle now. Waiting for a full set deadlocks two batches each holding half the pool.
func _lanes_for(want: int) -> Array[HTTPRequest]:
	var out: Array[HTTPRequest] = []
	while out.is_empty():
		for lane in _lanes:
			if not _busy.has(lane.get_instance_id()):
				_busy[lane.get_instance_id()] = true
				out.append(lane)
				break
		if out.is_empty():
			await get_tree().process_frame
	for lane in _lanes:
		if out.size() >= want:
			break
		if not _busy.has(lane.get_instance_id()):
			_busy[lane.get_instance_id()] = true
			out.append(lane)
	return out

func _free(lanes: Array) -> void:
	for lane in lanes:
		_busy.erase(lane.get_instance_id())

## Resolve and verify. Returns the bytes, or an empty array on any failure.
func fetch(uri: String) -> PackedByteArray:
	var p: Dictionary = PulseBlockzChain.parse_asset_uri(uri)
	if not p.ok:
		push_warning("ChainAssets: %s: %s" % [uri, p.error]); fetched.emit(uri, false); return PackedByteArray()
	if curation != null and curation.hidden({"asset": {"id": "", "creator": "", "uri": uri}}):
		push_warning("ChainAssets: hidden by a curation list: %s" % uri); fetched.emit(uri, false); return PackedByteArray()
	var cached := _read_cache(p.content_hash)
	if cached.size() > 0:
		fetched.emit(uri, true); return cached
	if _inflight.has(p.content_hash):
		while _inflight.has(p.content_hash) and is_inside_tree():
			await get_tree().process_frame
		cached = _read_cache(p.content_hash)
		fetched.emit(uri, cached.size() > 0)
		return cached
	_inflight[p.content_hash] = true
	var data := PackedByteArray()
	if _needs_locating(p):
		await _locate([String(p.content_hash)])
		p = _located_into(p)
	if p.has_chain and int(p.chain_id) == chain_id:
		data = await _fetch_chain(p)
	if data.is_empty() and p.get("has_tx", false) and int(p.get("tx_chain_id", 0)) == chain_id:
		data = await _fetch_calldata(p)
	if data.is_empty():
		for src in p.srcs:
			if not _mirror_allowed(String(src)):
				push_warning("ChainAssets: not fetching the mirror %s -- the place must declare `mirror` and its host must be in mirrors_allowed" % String(src).left(60))
				continue
			data = await _fetch_url(src)
			if not data.is_empty() and PulseBlockzChain.verify(data, p.content_hash): break
			data = PackedByteArray()
	_inflight.erase(p.content_hash)
	if data.is_empty() or not PulseBlockzChain.verify(data, p.content_hash):
		push_warning("ChainAssets: no source produced bytes matching %s" % p.content_hash)
		fetched.emit(uri, false); return PackedByteArray()
	_write_cache(p.content_hash, data)
	fetched.emit(uri, true)
	return data

## True when this link names content and nothing else, and there is a store to ask.
func _needs_locating(p: Dictionary) -> bool:
	return (asset_store != "" and not p.get("has_chain", false)
		and not p.get("has_tx", false) and String(p.get("content_hash", "")) != "")

## The same link with what the store knows filled in. Unchanged if the store had nothing.
func _located_into(p: Dictionary) -> Dictionary:
	var found: Dictionary = _located.get(String(p.content_hash), {})
	if int(found.get("blob", 0)) <= 0:
		return p
	p["has_chain"] = true
	p["chain_id"] = chain_id
	p["store"] = asset_store
	p["blob_id"] = int(found.blob)
	if String(p.get("mime", "")) == "":
		p["mime"] = String(found.get("mime", ""))
	return p

## Looks up every hash not looked up already: one batched round of `blobOf`, then one of `blob`
## for the ones that exist. Two round trips for a whole shelf, not two per item. A hit is kept
## for the life of the process: bytes are stored under the hash of those bytes, so which blob
## holds them is settled when they land and cannot move.
func _locate(hashes: Array) -> void:
	if asset_store == "":
		return
	var asking := []
	for h in hashes:
		var one := String(h)
		if one != "" and not _located.has(one) and not asking.has(one):
			asking.append(one)
	if asking.is_empty():
		return
	var calls := []
	for one in asking:
		calls.append({"method": "eth_call", "params": [
			{"to": asset_store, "data": PulseBlockzChain.blob_of_calldata(one)}, "latest"]})
	var answers := await _rpc_many(calls)

	var found := []      # the hashes that resolved, parallel to `blobs`
	var blobs := []
	for i in asking.size():
		var id := _word_to_int(String(answers[i]) if i < answers.size() else "")
		if id > 0:
			found.append(asking[i])
			blobs.append(id)
		else:
			# Zero means "not in this store", which a later publish can change: not remembered.
			push_warning("ChainAssets: %s names no blob at %s" % [asking[i], asset_store])
	if found.is_empty():
		return

	# Second round: the blob carries the mime, which a bare link has no other way to learn.
	var describe := []
	for id in blobs:
		describe.append({"method": "eth_call", "params": [
			{"to": asset_store, "data": PulseBlockzChain.blob_calldata(int(id))}, "latest"]})
	var told := await _rpc_many(describe)
	for i in found.size():
		var b: Dictionary = PulseBlockzChain.decode_blob(String(told[i]) if i < told.size() else "")
		_located[found[i]] = {"blob": int(blobs[i]),
			"mime": String(b.get("mime", "")), "size": int(b.get("size", 0))}

## The last 32-byte word of an eth_call result, as an int. 0 for anything unreadable.
func _word_to_int(hex: String) -> int:
	if hex.length() < 66:
		return 0
	var digits := hex.substr(hex.length() - 64).lstrip("0")
	return ("0x" + digits).hex_to_int() if digits != "" else 0

## The mime the link carries, or the one the store keeps beside the bytes; "" if neither knows.
## Callers writing the bytes to a file need it: Godot picks its loader from the extension.
func mime_of(uri: String) -> String:
	var p: Dictionary = PulseBlockzChain.parse_asset_uri(uri)
	if not p.get("ok", false):
		return ""
	var mime := String(p.get("mime", ""))
	if mime != "":
		return mime
	if _needs_locating(p):
		await _locate([String(p.content_hash)])
		return String(_located.get(String(p.content_hash), {}).get("mime", ""))
	return ""


## Byte size per uri without downloading: the store keeps a blob's size beside its bytes, so one
## batched read answers for a whole shelf. Uris with no answer are left out of the result.
func sizes(uris: Array) -> Dictionary:
	var out := {}
	var calls := []
	var asked := []
	var seen := {}
	for uri in uris:
		var one := String(uri)
		if one == "" or seen.has(one):
			continue
		seen[one] = true
		var p: Dictionary = PulseBlockzChain.parse_asset_uri(one)
		if not p.get("ok", false):
			continue
		var cached := _read_cache(p.content_hash)
		if cached.size() > 0:
			out[one] = cached.size()
			continue
		if not p.has_chain or int(p.chain_id) != chain_id:
			continue
		calls.append({"method": "eth_call", "params": [{"to": p.store, "data": PulseBlockzChain.blob_calldata(int(p.blob_id))}, "latest"]})
		asked.append(one)
	if calls.is_empty():
		return out
	var answers := await _rpc_many(calls)
	for i in asked.size():
		var hex := String(answers[i]) if i < answers.size() else ""
		if hex == "":
			continue
		var blob: Dictionary = PulseBlockzChain.decode_blob(hex)
		if int(blob.get("size", 0)) > 0:
			out[asked[i]] = int(blob.get("size", 0))
	return out

## Warms the cache for many URIs at once: everything in an AssetStore on this chain goes out as
## one batched JSON-RPC call. Anything the batch cannot serve is left to `fetch`.
func prefetch(uris: Array) -> void:
	var pending := []
	var seen := {}
	for uri in uris:
		if uri == "" or seen.has(uri):
			continue
		seen[uri] = true
		var p: Dictionary = PulseBlockzChain.parse_asset_uri(uri)
		if not p.get("ok", false) or _read_cache(p.content_hash).size() > 0:
			continue
		pending.append(p)
	# Bare links are located together before the read batch is built, so a shelf of them costs
	# the same round trips as a shelf of full links.
	var bare := []
	for p in pending:
		if _needs_locating(p):
			bare.append(String(p.content_hash))
	if not bare.is_empty():
		await _locate(bare)
		for i in pending.size():
			if _needs_locating(pending[i]):
				pending[i] = _located_into(pending[i])
	pending = pending.filter(func(p): return p.get("has_chain", false) and int(p.chain_id) == chain_id)
	if pending.is_empty():
		return
	# Split across lanes, all started before any is awaited: a node answers one batch strictly in
	# order, so a single request costs the sum of its parts. One-shot callbacks rather than
	# awaiting request_completed: a lane finishing before its await is reached hangs forever.
	var lanes: Array[HTTPRequest] = await _lanes_for(pending.size())
	var per := int(ceil(float(pending.size()) / lanes.size()))
	var bodies := {}
	var left := [0]
	for l in lanes.size():
		var from := l * per
		if from >= pending.size():
			break
		var to: int = min(from + per, pending.size())
		var payload := []
		for i in range(from, to):
			payload.append({"jsonrpc": "2.0", "id": i, "method": "eth_call",
				"params": [{"to": pending[i].store, "data": PulseBlockzChain.read_calldata(int(pending[i].blob_id))}, "latest"]})
		var slot := l
		left[0] += 1
		lanes[l].request_completed.connect(
			func(_result, code, _headers, body):
				bodies[slot] = [code, body]
				left[0] -= 1,
			CONNECT_ONE_SHOT)
		if lanes[l].request(rpc_url, ["content-type: application/json"], HTTPClient.METHOD_POST, JSON.stringify(payload)) != OK:
			bodies[slot] = [0, PackedByteArray()]
			left[0] -= 1
	while left[0] > 0:
		await get_tree().process_frame
	_free(lanes)

	var kept := 0
	for slot in bodies:
		if int(bodies[slot][0]) != 200:
			continue
		var parsed = JSON.parse_string((bodies[slot][1] as PackedByteArray).get_string_from_utf8())
		if typeof(parsed) != TYPE_ARRAY:
			continue
		for entry in parsed:
			if typeof(entry) != TYPE_DICTIONARY or not entry.has("id") or not entry.has("result"):
				continue
			var i := int(entry.id)
			if i < 0 or i >= pending.size():
				continue
			var data := PulseBlockzChain.decode_bytes(String(entry.result))
			if data.size() > 0 and PulseBlockzChain.verify(data, pending[i].content_hash):
				_write_cache(pending[i].content_hash, data)
				kept += 1
	if kept > 0:
		print("[assets] prefetched %d of %d in one round trip" % [kept, pending.size()])

## Warms the cache for assets published as transaction calldata: every manifest transaction in
## one batched request, then every chunk transaction they name in a second.
func prefetch_calldata(uris: Array) -> void:
	var pending := []
	var seen := {}
	for uri in uris:
		if uri == "" or seen.has(uri):
			continue
		seen[uri] = true
		var p: Dictionary = PulseBlockzChain.parse_asset_uri(uri)
		if not p.get("ok", false) or _read_cache(p.content_hash).size() > 0:
			continue
		if p.get("has_tx", false) and int(p.get("tx_chain_id", 0)) == chain_id:
			pending.append(p)
	if pending.is_empty():
		return

	var manifests := await _txs([] + pending.map(func(p): return String(p.manifest_tx)))
	# `wanted` is deduped across assets; a chunk's position in it is the index its body returns under.
	var chunks_of := {}
	var wanted := []
	for i in pending.size():
		var m: Dictionary = PulseBlockzChain.parse_tx_manifest(String(manifests.get(i, "")))
		if not m.get("ok", false):
			continue
		chunks_of[i] = m.chunks
		for tx in m.chunks:
			if not wanted.has(String(tx)):
				wanted.append(String(tx))
	if wanted.is_empty():
		return
	var bodies := await _txs(wanted)

	var kept := 0
	for i in pending.size():
		if not chunks_of.has(i):
			continue
		var data := PackedByteArray()
		var whole := true
		for tx in chunks_of[i]:
			var part := PulseBlockzChain.parse_tx_chunk(String(bodies.get(wanted.find(String(tx)), "")))
			if part.is_empty():
				whole = false
				break
			data.append_array(part)
		if whole and PulseBlockzChain.verify(data, pending[i].content_hash):
			_write_cache(pending[i].content_hash, data)
			kept += 1
	if kept > 0:
		print("[assets] prefetched %d calldata asset(s) in two round trips" % kept)

## eth_getTransactionByHash for many hashes at once. Returns index -> calldata.
func _txs(hashes: Array) -> Dictionary:
	var out := {}
	if hashes.is_empty():
		return out
	var lanes: Array[HTTPRequest] = await _lanes_for(hashes.size())
	var per := int(ceil(float(hashes.size()) / lanes.size()))
	var bodies := {}
	var left := [0]
	for l in lanes.size():
		var from := l * per
		if from >= hashes.size():
			break
		var to: int = min(from + per, hashes.size())
		var payload := []
		for i in range(from, to):
			payload.append({"jsonrpc": "2.0", "id": i, "method": "eth_getTransactionByHash", "params": [hashes[i]]})
		var slot := l
		left[0] += 1
		lanes[l].request_completed.connect(
			func(_result, code, _headers, body):
				bodies[slot] = [code, body]
				left[0] -= 1,
			CONNECT_ONE_SHOT)
		if lanes[l].request(rpc_url, ["content-type: application/json"], HTTPClient.METHOD_POST, JSON.stringify(payload)) != OK:
			bodies[slot] = [0, PackedByteArray()]
			left[0] -= 1
	while left[0] > 0:
		await get_tree().process_frame
	_free(lanes)
	for slot in bodies:
		if int(bodies[slot][0]) != 200:
			continue
		var parsed = JSON.parse_string((bodies[slot][1] as PackedByteArray).get_string_from_utf8())
		if typeof(parsed) != TYPE_ARRAY:
			continue
		for entry in parsed:
			if typeof(entry) == TYPE_DICTIONARY and entry.has("id") and typeof(entry.get("result")) == TYPE_DICTIONARY:
				out[int(entry.id)] = String((entry.result as Dictionary).get("input", ""))
	return out

# ---- chain ------------------------------------------------------------------------------
func _fetch_chain(p: Dictionary) -> PackedByteArray:
	# One eth_call for the whole blob, then chunk by chunk past roughly half a megabyte, where a
	# node refuses to return that much at once. The first call failing is not an error.
	var res := await _eth_call(p.store, PulseBlockzChain.read_calldata(int(p.blob_id)))
	if res != "":
		var data := PulseBlockzChain.decode_bytes(res)
		if PulseBlockzChain.verify(data, p.content_hash): return data
	var chunks_res := await _eth_call(p.store, PulseBlockzChain.chunks_of_calldata(int(p.blob_id)))
	if chunks_res == "": return PackedByteArray()
	# Every chunk in one batch: one at a time costs a round trip per chunk.
	var calls := []
	for addr in PulseBlockzChain.decode_addresses(chunks_res):
		calls.append({"method": "eth_getCode", "params": [addr, "latest"]})
	var codes := PackedStringArray()
	for code in await _rpc_many(calls):
		if String(code) == "": return PackedByteArray()
		codes.push_back(String(code))
	var data := PulseBlockzChain.assemble_chunks(codes)
	return data if PulseBlockzChain.verify(data, p.content_hash) else PackedByteArray()

## Several JSON-RPC calls at once, answered by index, split across the lanes as prefetch does.
func _rpc_many(calls: Array) -> Array:
	var out := []
	out.resize(calls.size())
	for i in out.size():
		out[i] = ""
	if calls.is_empty():
		return out
	var lanes: Array[HTTPRequest] = await _lanes_for(calls.size())
	var per := int(ceil(float(calls.size()) / lanes.size()))
	var bodies := {}
	var left := [0]
	for l in lanes.size():
		var from := l * per
		if from >= calls.size():
			break
		var to: int = min(from + per, calls.size())
		var payload := []
		for i in range(from, to):
			payload.append({"jsonrpc": "2.0", "id": i, "method": calls[i].method, "params": calls[i].params})
		var slot := l
		left[0] += 1
		lanes[l].request_completed.connect(
			func(_result, code, _headers, body):
				bodies[slot] = [code, body]
				left[0] -= 1,
			CONNECT_ONE_SHOT)
		if lanes[l].request(rpc_url, ["content-type: application/json"], HTTPClient.METHOD_POST, JSON.stringify(payload)) != OK:
			bodies[slot] = [0, PackedByteArray()]
			left[0] -= 1
	while left[0] > 0:
		await get_tree().process_frame
	_free(lanes)
	for slot in bodies:
		if int(bodies[slot][0]) != 200:
			continue
		var parsed = JSON.parse_string((bodies[slot][1] as PackedByteArray).get_string_from_utf8())
		if typeof(parsed) != TYPE_ARRAY:
			continue
		for entry in parsed:
			if typeof(entry) != TYPE_DICTIONARY or not entry.has("id") or not entry.has("result"):
				continue
			var i := int(entry.id)
			if i < 0 or i >= out.size():
				continue
			# eth_getTransactionByHash answers with an object; the calldata wanted is its "input"
			# field, as in _rpc. Stringifying the whole transaction hands the chunk reader a
			# dictionary instead of hex.
			if typeof(entry.result) == TYPE_DICTIONARY:
				out[i] = String((entry.result as Dictionary).get("input", ""))
			else:
				out[i] = String(entry.result)
	return out

## An asset published as transaction calldata: a manifest transaction listing chunk
## transactions. Cheaper to write than contract code, but it lives in block history, readable
## only from a node that kept it -- which is why it runs after _fetch_chain, whose bytes sit in
## contract code any node can still read.
func _fetch_calldata(p: Dictionary) -> PackedByteArray:
	var manifest_raw := await _rpc("eth_getTransactionByHash", [String(p.manifest_tx)])
	if manifest_raw == "":
		return PackedByteArray()
	var manifest: Dictionary = PulseBlockzChain.parse_tx_manifest(manifest_raw)
	if not manifest.get("ok", false):
		return PackedByteArray()
	var calls := []
	for tx in manifest.chunks:
		calls.append({"method": "eth_getTransactionByHash", "params": [String(tx)]})
	var out := PackedByteArray()
	for raw in await _rpc_many(calls):
		if String(raw) == "":
			return PackedByteArray()
		var part := PulseBlockzChain.parse_tx_chunk(String(raw))
		if part.is_empty():
			return PackedByteArray()
		out.append_array(part)
	return out if PulseBlockzChain.verify(out, p.content_hash) else PackedByteArray()

func _eth_call(to: String, data: String) -> String:
	return await _rpc("eth_call", [{"to": to, "data": data}, "latest"])

## Returns the JSON-RPC result as a String ("" on error).
func _rpc(method: String, params: Array) -> String:
	var lanes := await _lanes_for(1)
	var req: HTTPRequest = lanes[0]
	var body := JSON.stringify({"jsonrpc": "2.0", "id": 1, "method": method, "params": params})
	var err := req.request(rpc_url, ["content-type: application/json"], HTTPClient.METHOD_POST, body)
	if err != OK:
		_free(lanes)
		push_warning("ChainAssets: rpc request failed (%d)" % err); return ""
	var res: Array = await req.request_completed
	_free(lanes)
	if res[1] != 200: push_warning("ChainAssets: rpc HTTP %d" % res[1]); return ""
	var parsed = JSON.parse_string((res[3] as PackedByteArray).get_string_from_utf8())
	if typeof(parsed) != TYPE_DICTIONARY or not parsed.has("result"):
		var why := str(parsed.get("error", parsed) if typeof(parsed) == TYPE_DICTIONARY else parsed)
		# Too big for one call is not a failure: _fetch_chain falls back to chunks.
		if why.contains("returndata") or why.contains("exceeding"):
			return ""
		push_warning("ChainAssets: rpc error: %s" % why); return ""
	if typeof(parsed.result) == TYPE_DICTIONARY:
		return String((parsed.result as Dictionary).get("input", ""))   # a transaction: its calldata
	return String(parsed.result)

# ---- mirrors ------------------------------------------------------------------------------
## Matched on host, so a permitted name in someone else's path or query does not pass.
func _mirror_allowed(url: String) -> bool:
	if mirrors_allowed.is_empty():
		return false
	if not preload("res://host/Uses.gd").permits("mirror"):
		return false
	var host := url
	for scheme in ["https://", "http://", "ipfs://"]:
		host = host.trim_prefix(scheme)
	host = host.split("/")[0].split("?")[0].split("@")[-1]
	if host.contains(":"):
		host = host.substr(0, host.rfind(":"))     # the port is not part of the host
	for allowed in mirrors_allowed:
		if host == String(allowed) or host.ends_with("." + String(allowed)):
			return true
	return false

func _fetch_url(url: String) -> PackedByteArray:
	if url.begins_with("ipfs://"):
		url = "https://ipfs.io/ipfs/" + url.substr(7)   # any gateway works: the hash is checked
	var req := HTTPRequest.new(); req.use_threads = true; add_child(req)
	if req.request(url) != OK:
		req.queue_free(); return PackedByteArray()
	var res: Array = await req.request_completed
	req.queue_free()
	return res[3] if res[1] == 200 else PackedByteArray()

# ---- cache ----------------------------------------------------------------------------------
func _cache_path(h: String) -> String:
	return cache_dir.path_join(h.trim_prefix("0x"))

func _read_cache(h: String) -> PackedByteArray:
	var path := _cache_path(h)
	if not FileAccess.file_exists(path): return PackedByteArray()
	var data := FileAccess.get_file_as_bytes(path)
	return data if PulseBlockzChain.verify(data, h) else PackedByteArray()

func _write_cache(h: String, data: PackedByteArray) -> void:
	var f := FileAccess.open(_cache_path(h), FileAccess.WRITE)
	if f: f.store_buffer(data)
