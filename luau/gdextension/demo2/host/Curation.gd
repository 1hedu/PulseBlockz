# COPY -- do not edit. The original is luau/gdextension/host/Curation.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# Client-side curation (blocklists / allowlists) — see ../../../curation/SPEC.md.
# Autoload named `Curation`; mirrors curation/src/curation.js. The chain has no delist
# switch: hidden means this client won't render it, not that the token is gone.
extends Node

signal lists_changed
signal report_sent(maintainer: String, ok: bool)

const REASONS := ["abuse", "copyright", "scam", "malware", "spam", "other"]

var chain_id: int = 369
var registry: String = ""                 # UGC1155 address (lowercase)
var verifier: Callable                    # EIP-712 check; defaults to the GDExtension's
var trusted_urls: PackedStringArray = []  # accepted without a verifier (dev / first-party lists)
var subscriptions: Array[Dictionary] = [] # [{maintainer, url, enabled}]

var _lists := {}     # maintainer(lower) -> {list, enabled, verified}
var _http: HTTPRequest

func _ready() -> void:
	_http = HTTPRequest.new()
	add_child(_http)
	if not verifier.is_valid() and ClassDB.class_exists("PulseBlockzCrypto"):
		verifier = Callable(PulseBlockzCrypto, "verify_curation_list")

# ---- subscriptions --------------------------------------------------------------
func subscribe(url: String, maintainer: String = "", enabled: bool = true) -> void:
	subscriptions.append({"maintainer": maintainer.to_lower(), "url": url, "enabled": enabled})
	refresh()

func set_enabled(maintainer: String, enabled: bool) -> void:
	var k := maintainer.to_lower()
	if _lists.has(k):
		_lists[k].enabled = enabled
	for s in subscriptions:
		if s.maintainer == k:
			s.enabled = enabled
	lists_changed.emit()

func refresh() -> void:
	for s in subscriptions:
		await _fetch(s)

func _fetch(sub: Dictionary) -> void:
	var err := _http.request(sub.url, ["accept: application/json"])
	if err != OK:
		push_warning("curation: request failed for %s (%d)" % [sub.url, err]); return
	var res: Array = await _http.request_completed
	if res[1] != 200:
		push_warning("curation: HTTP %d for %s" % [res[1], sub.url]); return
	var list = JSON.parse_string((res[3] as PackedByteArray).get_string_from_utf8())
	if typeof(list) != TYPE_DICTIONARY:
		push_warning("curation: not JSON: %s" % sub.url); return
	add_list(list, sub.url in trusted_urls, sub.enabled)

# ---- lists ------------------------------------------------------------------------
## Accepts a list Dictionary. Returns true if it was stored (newest per maintainer wins).
func add_list(list: Dictionary, trusted_source: bool = false, enabled: bool = true) -> bool:
	if not _valid_shape(list):
		push_warning("curation: rejected malformed list"); return false
	if int(list.chainId) != chain_id or String(list.registry).to_lower() != registry.to_lower():
		return false  # another deployment: never applies
	var verified := false
	if verifier.is_valid():
		verified = bool(verifier.call(list))
		if not verified:
			push_warning("curation: bad signature from %s" % list.maintainer); return false
	elif not trusted_source:
		push_warning("curation: no verifier set; refusing list from untrusted source"); return false
	var k := String(list.maintainer).to_lower()
	if _lists.has(k) and int(_lists[k].list.updated) >= int(list.updated):
		return false
	var was_enabled: bool = _lists[k].enabled if _lists.has(k) else enabled
	_lists[k] = {"list": list, "enabled": was_enabled, "verified": verified}
	lists_changed.emit()
	return true

func remove_list(maintainer: String) -> void:
	_lists.erase(maintainer.to_lower()); lists_changed.emit()

func status() -> Array:
	var out := []
	for k in _lists:
		var e: Dictionary = _lists[k]
		out.append({"maintainer": e.list.maintainer, "name": e.list.name, "mode": e.list.mode,
			"entries": e.list.entries.size(), "enabled": e.enabled, "verified": e.verified,
			"stale": Time.get_unix_time_from_system() > int(e.list.updated) + int(e.list.ttl)})
	return out

func _valid_shape(l: Dictionary) -> bool:
	for key in ["version", "mode", "chainId", "registry", "maintainer", "name", "updated", "ttl", "entries", "signature"]:
		if not l.has(key): return false
	if int(l.version) != 1 or not (l.mode in ["block", "allow"]): return false
	if typeof(l.entries) != TYPE_ARRAY: return false
	for e in l.entries:
		if typeof(e) != TYPE_DICTIONARY or not e.has("kind") or not (e.get("reason", "") in REASONS): return false
	return true

# ---- decisions --------------------------------------------------------------------
## subject: {asset: {id, creator, uri}} | {creator: address} | {server: endpoint}
## Returns {hidden: bool, by: [{maintainer, mode, entry}], allow_required: bool}
func decide(subject: Dictionary) -> Dictionary:
	var by := []
	var allow_required := false
	var allowed := false
	for k in _lists:
		var e: Dictionary = _lists[k]
		if not e.enabled: continue
		var m := _matches(e.list, subject)
		if e.list.mode == "block":
			for entry in m: by.append({"maintainer": e.list.maintainer, "mode": "block", "entry": entry})
		else:
			allow_required = true
			if m.size() > 0:
				allowed = true
				for entry in m: by.append({"maintainer": e.list.maintainer, "mode": "allow", "entry": entry})
	var blocked := by.any(func(b): return b.mode == "block")
	return {"hidden": blocked or (allow_required and not allowed), "by": by, "allow_required": allow_required}

func hidden(subject: Dictionary) -> bool:
	return decide(subject).hidden

func is_asset_hidden(id: String, creator: String, uri: String) -> bool:
	return hidden({"asset": {"id": id, "creator": creator, "uri": uri}})

func is_server_hidden(endpoint: String) -> bool:
	return hidden({"server": endpoint})

## Filter the `owned` array from GET /inventory before equipping anything.
func filter_assets(assets: Array) -> Array:
	return assets.filter(func(a): return not is_asset_hidden(String(a.id), String(a.get("creator", "")), String(a.get("uri", ""))))

func _matches(list: Dictionary, subject: Dictionary) -> Array:
	var out := []
	var a: Dictionary = subject.get("asset", {})
	var creator := String(subject.get("creator", a.get("creator", ""))).to_lower()
	var server := String(subject.get("server", "")).to_lower()
	for e in list.entries:
		match e.kind:
			"asset":
				if not a.is_empty() and String(e.id) == String(a.id):
					out.append(e)
			"creator":
				if creator != "" and String(e.address).to_lower() == creator:
					out.append(e)
			"server":
				if server != "" and String(e.endpoint).to_lower() == server:
					out.append(e)
			"uri":
				if a.has("uri") and String(a.uri).begins_with(String(e.prefix)):
					out.append(e)
	return out

# ---- reporting --------------------------------------------------------------------
## POSTs to every enabled list that names a report endpoint; outcome per list on report_sent.
func report(subject: Dictionary, reason: String, note: String = "", reporter: String = "") -> void:
	assert(reason in REASONS)
	var body := {"version": 1, "reason": reason, "note": note, "at": int(Time.get_unix_time_from_system())}
	if subject.has("asset"):
		body.merge({"kind": "asset", "id": String(subject.asset.id)})
	elif subject.has("creator"):
		body.merge({"kind": "creator", "address": subject.creator})
	else:
		body.merge({"kind": "server", "endpoint": subject.server})
	if reporter != "":
		body["reporter"] = reporter
	for k in _lists:
		var e: Dictionary = _lists[k]
		if not e.enabled or not e.list.has("report") or String(e.list.report) == "": continue
		var req := HTTPRequest.new(); add_child(req)
		var err := req.request(e.list.report, ["content-type: application/json"], HTTPClient.METHOD_POST, JSON.stringify(body))
		if err != OK:
			report_sent.emit(e.list.maintainer, false); req.queue_free(); continue
		var res: Array = await req.request_completed
		report_sent.emit(e.list.maintainer, res[1] >= 200 and res[1] < 300)
		req.queue_free()
