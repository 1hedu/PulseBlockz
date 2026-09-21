# Fetches pblockz:// assets (ASSETS.md) through AssetStore or the URI's mirrors, verifying the
# content hash and caching by it. Autoload as `ChainAssets`; needs PulseBlockzChain.
extends Node

signal fetched(uri: String, ok: bool)

var rpc_url: String = "http://127.0.0.1:8545"   # a PulseChain RPC in production
var chain_id: int = 31337
var cache_dir: String = "user://assets"
var curation: Node = null                        # optional; defaults to the Curation autoload

var _inflight := {}   # unused: the cache handles repeat fetches

func _ready() -> void:
	DirAccess.make_dir_recursive_absolute(cache_dir)
	if curation == null and has_node("/root/Curation"):
		curation = get_node("/root/Curation")

## Resolve and verify. Returns the bytes, or empty on any failure.
func fetch(uri: String) -> PackedByteArray:
	var p: Dictionary = PulseBlockzChain.parse_asset_uri(uri)
	if not p.ok:
		push_warning("ChainAssets: %s: %s" % [uri, p.error]); fetched.emit(uri, false); return PackedByteArray()
	if curation != null and curation.hidden({"asset": {"id": "", "creator": "", "uri": uri}}):
		push_warning("ChainAssets: hidden by a curation list: %s" % uri); fetched.emit(uri, false); return PackedByteArray()
	var cached := _read_cache(p.content_hash)
	if cached.size() > 0:
		fetched.emit(uri, true); return cached
	var data := PackedByteArray()
	if p.has_chain and int(p.chain_id) == chain_id:
		data = await _fetch_chain(p)
	if data.is_empty() and p.get("has_tx", false) and int(p.get("tx_chain_id", 0)) == chain_id:
		data = await _fetch_calldata(p)
	if data.is_empty():
		for src in p.srcs:
			data = await _fetch_url(src)
			if not data.is_empty() and PulseBlockzChain.verify(data, p.content_hash): break
			data = PackedByteArray()
	if data.is_empty() or not PulseBlockzChain.verify(data, p.content_hash):
		push_warning("ChainAssets: no source produced bytes matching %s" % p.content_hash)
		fetched.emit(uri, false); return PackedByteArray()
	_write_cache(p.content_hash, data)
	fetched.emit(uri, true)
	return data

# ---- chain ------------------------------------------------------------------------------
func _fetch_chain(p: Dictionary) -> PackedByteArray:
	# One eth_call for the whole blob; chunked reads are the fallback when the RPC refuses the size.
	var res := await _eth_call(p.store, PulseBlockzChain.read_calldata(int(p.blob_id)))
	if res != "":
		var data := PulseBlockzChain.decode_bytes(res)
		if PulseBlockzChain.verify(data, p.content_hash): return data
	var chunks_res := await _eth_call(p.store, PulseBlockzChain.chunks_of_calldata(int(p.blob_id)))
	if chunks_res == "": return PackedByteArray()
	var codes := PackedStringArray()
	for addr in PulseBlockzChain.decode_addresses(chunks_res):
		var code := await _rpc("eth_getCode", [addr, "latest"])
		if code == "": return PackedByteArray()
		codes.push_back(code)
	var data := PulseBlockzChain.assemble_chunks(codes)
	return data if PulseBlockzChain.verify(data, p.content_hash) else PackedByteArray()

## An asset published as calldata: a manifest transaction listing its chunk transactions. Cheaper
## to publish than contract code, at the cost of living in block history, not contract storage.
func _fetch_calldata(p: Dictionary) -> PackedByteArray:
	var manifest_raw := await _rpc("eth_getTransactionByHash", [String(p.manifest_tx)])
	if manifest_raw == "":
		return PackedByteArray()
	var manifest: Dictionary = PulseBlockzChain.parse_tx_manifest(manifest_raw)
	if not manifest.get("ok", false):
		return PackedByteArray()
	var out := PackedByteArray()
	for tx in manifest.chunks:
		var raw := await _rpc("eth_getTransactionByHash", [String(tx)])
		if raw == "":
			return PackedByteArray()
		var part := PulseBlockzChain.parse_tx_chunk(raw)
		if part.is_empty():
			return PackedByteArray()
		out.append_array(part)
	return out if PulseBlockzChain.verify(out, p.content_hash) else PackedByteArray()

func _eth_call(to: String, data: String) -> String:
	return await _rpc("eth_call", [{"to": to, "data": data}, "latest"])

## The JSON-RPC result as a String, "" on error.
func _rpc(method: String, params: Array) -> String:
	var req := HTTPRequest.new(); add_child(req)
	var body := JSON.stringify({"jsonrpc": "2.0", "id": 1, "method": method, "params": params})
	var err := req.request(rpc_url, ["content-type: application/json"], HTTPClient.METHOD_POST, body)
	if err != OK:
		req.queue_free(); push_warning("ChainAssets: rpc request failed (%d)" % err); return ""
	var res: Array = await req.request_completed
	req.queue_free()
	if res[1] != 200: push_warning("ChainAssets: rpc HTTP %d" % res[1]); return ""
	var parsed = JSON.parse_string((res[3] as PackedByteArray).get_string_from_utf8())
	if typeof(parsed) != TYPE_DICTIONARY or not parsed.has("result"):
		push_warning("ChainAssets: rpc error: %s" % str(parsed.get("error", parsed) if typeof(parsed) == TYPE_DICTIONARY else parsed)); return ""
	if typeof(parsed.result) == TYPE_DICTIONARY:
		return String((parsed.result as Dictionary).get("input", ""))   # a transaction: its calldata
	return String(parsed.result)

# ---- mirrors ------------------------------------------------------------------------------
func _fetch_url(url: String) -> PackedByteArray:
	if url.begins_with("ipfs://"):
		url = "https://ipfs.io/ipfs/" + url.substr(7)   # any gateway works: the hash is checked
	var req := HTTPRequest.new(); add_child(req)
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
