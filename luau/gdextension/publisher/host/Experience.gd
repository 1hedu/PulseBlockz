# COPY -- do not edit. The original is luau/gdextension/host/Experience.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# Loads an experience from the chain: scripts/publish-experience.js names every file by content
# hash and ChainAssets checks each. Source is NoReplicate, as on Roblox, so a server cannot send a
# client scripts -- it runs its own copy. The host draws the join screen, before any of that runs.
extends Node

signal mounted(name: String, files: int)
signal fetching(done: int, total: int)

@export var sync_path: NodePath = ^"../ScriptSync"
## Show the join screen before anything runs. Off only for headless tests.
@export var confirm := true

var assets: Node          # ChainAssets
var curation: Node        # Curation, optional
var _seen_path := "user://experiences.json"

func _ready() -> void:
	if assets == null:
		assets = get_node_or_null("/root/ChainAssets")
	if curation == null:
		curation = get_node_or_null("/root/Curation")

## Reads the manifest alone: no file body is fetched and nothing runs. `weigh` asks the store how
## big each named asset is, which is a call per asset: worth it for a page somebody is reading,
## not for a card on the way into a place.
func preview(uri: String, weigh: bool = true) -> Dictionary:
	var raw: PackedByteArray = await assets.fetch(uri)
	if raw.is_empty():
		return {"ok": false, "error": "the manifest could not be fetched or did not match its hash"}
	var parsed = JSON.parse_string(raw.get_string_from_utf8())
	if typeof(parsed) != TYPE_DICTIONARY or String(parsed.get("kind", "")) != "experience":
		return {"ok": false, "error": "that asset is not an experience manifest"}
	var files: Array = parsed.get("files", [])
	var bytes := 0
	for f in files:
		bytes += int(f.get("bytes", 0))

	# The store weighs an asset without sending it, so download_bytes covers code and assets.
	var place_assets := _assets_of(parsed.get("assets", {}))
	var asset_bytes := 0
	if weigh and not place_assets.is_empty() and assets.has_method("sizes"):
		var weighed: Dictionary = await assets.sizes(place_assets.values())
		for one in weighed.values():
			asset_bytes += int(one)
	var hash: String = PulseBlockzChain.parse_asset_uri(uri).get("content_hash", "")
	var out := {
		"ok": true, "uri": uri, "hash": hash,
		"name": String(parsed.get("name", "(unnamed)")),
		# Either may be "". splash is the card shown while the place is still arriving.
		"thumbnail": _asset(parsed.get("thumbnail", "")),
		"splash": _asset(parsed.get("splash", "")),
		# The ceiling Uses.gd holds it to. Nothing named, nothing allowed.
		"uses": parsed.get("uses", []) if typeof(parsed.get("uses", [])) == TYPE_ARRAY else [],
		"publisher": String(parsed.get("publisher", "")),
		# host:port the creator says the place is played at. Joining it runs what that server
		# sends, which is never checked against this hash.
		"server": String(parsed.get("server", "")),
		# Chain contracts by name and address, as the place's own Contracts module declares them.
		"contracts": parsed.get("contracts", {}) if typeof(parsed.get("contracts", {})) == TYPE_DICTIONARY else {},
		"published": int(parsed.get("published", 0)),
		"files": files, "count": files.size(), "bytes": bytes,
		# Named assets (a font, sounds, meshes); scripts find them in ReplicatedStorage.PlaceAssets.
		"assets": place_assets, "asset_bytes": asset_bytes, "download_bytes": bytes + asset_bytes,
		"seen_before": false, "changed": false, "previous": "",
	}

	var seen := _seen()
	if seen.has(out.name):
		out.seen_before = true
		out.previous = String(seen[out.name])
		out.changed = out.previous != hash

	# An experience is put to the curation lists as an asset like any other.
	if curation != null:
		var verdict: Dictionary = curation.decide({"asset": {"id": hash, "creator": out.publisher, "uri": uri}})
		out["blocked"] = verdict.get("hidden", false)
		out["curation"] = verdict.get("by", [])
	else:
		out["blocked"] = false
		out["curation"] = []
	return out

## Fetches every file, checks each against its own hash, and mounts them. Returns
## {ok, mounted, error}. Nothing is mounted until every file has been verified.
##
## `code_only` is for joining a server, which sends every instance but no script source: only the
## code is taken from the chain, laid over the tree already standing by ScriptSync.load_place,
## whose doc has the ordering that call has to keep. Mounting a model file on a joined client
## replaces rather than fills -- the server's instance is destroyed and a client-made one stands
## at a negative id in its place.
func mount(uri: String, code_only: bool = false) -> Dictionary:
	var info := await preview(uri)
	if not info.ok:
		return {"ok": false, "error": info.error}
	if info.blocked:
		return {"ok": false, "error": "a curation list you follow blocks this experience"}
	if confirm and not await _ask(info):
		return {"ok": false, "error": "you decided not to"}

	# The ceiling is set before any of its code is in the tree.
	preload("res://host/Uses.gd").declare(info.uses)

	# Warmed in batches: a place runs to a hundred files and more, and fetched one at a time
	# each is its own round trip to a public node. What the batch misses the loop below fetches.
	var wanted := []
	for f in info.files:
		# On a joined client the server supplies these instances.
		if code_only and not _is_code(String(f.get("path", ""))):
			continue
		wanted.append(f)
	var uris := []
	for f in wanted:
		uris.append(String(f.get("uri", "")))
	fetching.emit(0, uris.size())
	await assets.prefetch(uris)
	await assets.prefetch_calldata(uris)
	var sources := {}
	var done := 0
	for f in wanted:
		var p := String(f.get("path", ""))
		var data: PackedByteArray = await assets.fetch(String(f.get("uri", "")))
		done += 1
		fetching.emit(done, uris.size())
		if data.is_empty():
			return {"ok": false, "error": "%s could not be fetched, or did not match its hash" % p}
		# ScriptSync takes a .rbxm as base64; everything else is text.
		sources[p] = Marshalls.raw_to_base64(data) if p.ends_with(".rbxm") else data.get_string_from_utf8()

	var sync := get_node_or_null(sync_path)
	if sync == null:
		return {"ok": false, "error": "no ScriptSync to mount into"}
	var names := sources.keys()
	names.sort()
	# Every source is in the tree before any script starts, so a require() at startup resolves.
	for name in names:
		await sync.load_source(name, sources[name])

	var seen := _seen()
	seen[info.name] = info.hash
	_write_seen(seen)
	print("[experience] %s %s: %d file(s), %s" % ["took the code of" if code_only else "mounted", info.name, names.size(), info.hash.substr(0, 18)])
	mounted.emit(info.name, info.count)
	return {"ok": true, "mounted": info.count, "name": info.name,
		"published": info.published, "hash": info.hash, "splash": info.splash, "thumbnail": info.thumbnail,
		"uses": info.uses, "assets": info.assets, "server": info.server}

## Luau only: a model or a picture is an instance, and on a joined client instances are the
## server's.
static func _is_code(path: String) -> bool:
	return path.ends_with(".luau") or path.ends_with(".lua")

## Puts `pblockz://` back on a manifest's bare content hash: an ImageLabel is handed a string and
## wants a scheme in it. A reference that already carries one passes through. The hash alone is
## name enough because ChainAssets resolves it through AssetStore.blobOf, and the blob itself
## says what kind of file it holds.
static func _asset(ref) -> String:
	var text := String(ref)
	if text == "" or text.contains("://"):
		return text
	return "pblockz://" + text.trim_prefix("0x")

static func _assets_of(raw) -> Dictionary:
	var out := {}
	if typeof(raw) != TYPE_DICTIONARY:
		return out
	for k in raw:
		if not String(k).begins_with("_") and (String(raw[k]).begins_with("pblockz://")):
			out[String(k)] = String(raw[k])
	return out

# ---- what has been run before -----------------------------------------------------
func _seen() -> Dictionary:
	if not FileAccess.file_exists(_seen_path):
		return {}
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(_seen_path))
	return parsed if typeof(parsed) == TYPE_DICTIONARY else {}

func _write_seen(seen: Dictionary) -> void:
	var f := FileAccess.open(_seen_path, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(seen))
		f.close()

# ---- the join screen ---------------------------------------------------------------
func _ask(info: Dictionary) -> bool:
	var layer := CanvasLayer.new()
	layer.layer = 100
	add_child(layer)
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.6)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	layer.add_child(dim)

	var panel := PanelContainer.new()
	panel.set_anchors_preset(Control.PRESET_CENTER)
	panel.position = Vector2(-280, -190)
	panel.custom_minimum_size = Vector2(560, 0)
	layer.add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 20)
	panel.add_child(margin)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 10)
	margin.add_child(box)

	var heading := Label.new()
	heading.text = "Run “%s”?" % info.name
	heading.add_theme_font_size_override("font_size", 20)
	box.add_child(heading)

	var facts := Label.new()
	facts.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	facts.add_theme_font_size_override("font_size", 13)
	facts.text = "%d scripts and models, %s.\nPublished by %s.\n\n%s" % [
		info.count, String.humanize_size(info.bytes),
		info.publisher if info.publisher != "" else "someone who did not say",
		info.hash]
	box.add_child(facts)

	var verdict := Label.new()
	verdict.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	verdict.add_theme_font_size_override("font_size", 13)
	var lines := PackedStringArray()
	if info.curation.is_empty():
		lines.append("• No list you follow has anything to say about this one.")
	else:
		for entry in info.curation:
			lines.append("• %s: %s" % [String(entry.get("maintainer", "?")).substr(0, 10), entry.get("mode", "?")])
	if not info.seen_before:
		lines.append("• You have not run this before.")
	elif info.changed:
		lines.append("• CHANGED since you last ran it. The code is not what it was.")
		verdict.modulate = Color(1.0, 0.75, 0.4)
	else:
		lines.append("• Byte for byte the same code you ran last time.")
	var uses_script := preload("res://host/Uses.gd")
	if not uses_script.touches_wallet(info.uses):
		lines.append("• It cannot ask your wallet for anything.")
	else:
		lines.append("• It can ask your wallet for things. Read every prompt: approving or signing the wrong one can lose you your tokens.")
	for one in uses_script.describe(info.uses):
		lines.append("     %s" % one)
	verdict.text = "\n".join(lines)
	box.add_child(verdict)

	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_END
	row.add_theme_constant_override("separation", 8)
	box.add_child(row)
	var no := Button.new()
	no.text = "Not now"
	no.custom_minimum_size = Vector2(110, 34)
	row.add_child(no)
	var yes := Button.new()
	yes.text = "Run it"
	yes.custom_minimum_size = Vector2(130, 34)
	row.add_child(yes)
	yes.grab_focus()

	var answered := [false]
	yes.pressed.connect(func(): answered[0] = true; layer.queue_free())
	no.pressed.connect(func(): layer.queue_free())
	await layer.tree_exited
	return answered[0]
