# The form around host/Publish.gd, which does the work of putting a place on chain. Check reads a
# place folder (the one with default.project.json) and shows what would go up -- files, which are
# already on chain and kept, transactions, cost -- and nothing is sent until Publish is confirmed.
# What comes back is the experience's uri, the link a Player opens.
#
#   godot --path publisher
#   godot --path publisher -- --dir <place folder> [--name <name>]      how the Studio opens it
extends Node

signal checked(ok: bool)
signal published(result: Dictionary)

const PublishScript = preload("res://host/Publish.gd")
const Uses = preload("res://host/Uses.gd")

## Show the title card and its wallet doors before the form. Off for tests.
@export var show_title := true
## Overrides listing_path(), so a test never writes into the repo's listing.
@export var listing_override := ""

@onready var wallet: Node = $Wallet
var publisher: Node
var place := {}                  # what read_place + plan found; empty until Check
var last_result := {}
var busy := false

var _ui: CanvasLayer
var _dir: LineEdit
var _name: LineEdit
var _description: TextEdit
var _uses := {}                  # capability -> CheckBox
var _pictures := {}              # "thumbnail" / "splash" / "assets" -> LineEdit
var _who: Label
var _summary: Label
var _files: Tree
var _check_button: Button
var _publish_button: Button
var _progress: ProgressBar
var _result: LineEdit
var _log: RichTextLabel
var _dev_key_button: Button

const INK := Color(0.93, 0.91, 0.96)
const MUTED := Color(0.66, 0.62, 0.72)
const ACCENT := Color(0.94, 0.42, 0.66)
const GROUND := Color(0.075, 0.063, 0.094)
const WARN := Color(1.0, 0.72, 0.38)
const GOOD := Color(0.56, 0.84, 0.58)

func _ready() -> void:
	publisher = PublishScript.new()
	publisher.name = "Publish"
	publisher.wallet = wallet
	add_child(publisher)
	publisher.progress.connect(_on_progress)
	var assets := get_node_or_null("/root/ChainAssets")
	if assets != null:
		assets.rpc_url = wallet.rpc_url
		assets.chain_id = int(wallet.ADDRESSES.chain_id)
		assets.asset_store = String(wallet.ADDRESSES.AssetStore)
	_build_ui()
	var args := _args()
	if args.has("dir"):
		_dir.text = String(args.dir)
		_fill_from_folder()
	if args.has("name"):
		_name.text = String(args.name)
	if show_title:
		_ui.visible = false
		var title := preload("res://host/Title.gd").new()
		title.name = "Title"
		title.chosen.connect(func(): _ui.visible = true; _refresh_wallet())
		add_child(title)
	_refresh_wallet()

func _args() -> Dictionary:
	var out := {}
	var a := OS.get_cmdline_user_args()
	for i in a.size():
		for flag in ["dir", "name"]:
			if a[i] == "--" + flag and i + 1 < a.size():
				out[flag] = a[i + 1]
			elif a[i].begins_with("--%s=" % flag):
				out[flag] = a[i].substr(flag.length() + 3)
	return out

# ---- the work -------------------------------------------------------------------------------------
## Reads the place off disk and asks the chain which of its bytes are already there. Sends nothing.
func check() -> bool:
	if busy:
		return false
	busy = true
	_set_buttons()
	place = {}
	_publish_button.disabled = true
	_result.text = ""
	_log.clear()
	_summary.text = "Reading the place…"
	_files.clear()
	var read := PublishScript.read_place(_dir.text.strip_edges(), _options())
	if not read.ok:
		_summary.text = String(read.error)
		_summary.add_theme_color_override("font_color", WARN)
		busy = false
		_set_buttons()
		checked.emit(false)
		return false
	_summary.text = "Asking the chain what is already there…"
	var planned: Dictionary = await publisher.plan(read)
	busy = false
	if not planned.ok:
		_summary.text = String(planned.error)
		_summary.add_theme_color_override("font_color", WARN)
		_set_buttons()
		checked.emit(false)
		return false
	place = read
	await _show_plan()
	_set_buttons()
	checked.emit(true)
	return true

## Publishes what check() found. The confirmation lives in the Publish button, not here.
func publish_now() -> Dictionary:
	if busy or place.is_empty():
		return {"ok": false, "error": "check the place first"}
	if not wallet.can_buy():
		return {"ok": false, "error": "no key loaded to sign with"}
	busy = true
	_set_buttons()
	_progress.visible = true
	_progress.value = 0
	_say("Publishing %s as %s" % [place.name, wallet.wallet_address])
	var result: Dictionary = await publisher.publish(place)
	busy = false
	_progress.visible = false
	last_result = result
	if not result.ok:
		_say("[color=#ffb861]Stopped: %s[/color]" % result.error)
		_say("Whatever landed is kept: Check again and it will not be sent twice.")
		place = {}
		_set_buttons()
		published.emit(result)
		return result
	_result.text = String(result.uri)
	var listing := listing_path()
	PublishScript.record(listing, result)
	_say("[color=#8fd694]Published.[/color] %d stored, %d kept, %d transactions." % [result.stored, result.kept, (result.txs as Array).size()])
	_say("Recorded in %s" % ProjectSettings.globalize_path(listing))
	place = {}
	_set_buttons()
	published.emit(result)
	return result

## The repo's experiences.943.json from a checkout, a record of what was published; user:// from an app.
func listing_path() -> String:
	if listing_override != "":
		return listing_override
	var repo := ProjectSettings.globalize_path("res://").path_join("../../../experiences.943.json").simplify_path()
	if not OS.has_feature("template") and FileAccess.file_exists(repo):
		return repo
	return "user://experiences.943.json"

func _options() -> Dictionary:
	var uses := []
	for cap in _uses:
		if (_uses[cap] as CheckBox).button_pressed:
			uses.append(cap)
	return {"name": _name.text.strip_edges(), "description": _description.text.strip_edges(), "uses": uses,
		"thumbnail": _pictures.thumbnail.text.strip_edges(), "splash": _pictures.splash.text.strip_edges(),
		"assets": _pictures.assets.text.strip_edges()}

func _show_plan() -> void:
	var kept: Dictionary = place.kept
	var new_bytes := 0
	for key in place.to_store:
		new_bytes += (place.contents[key] as PackedByteArray).size() if place.contents.has(key) else (place.pictures[String(key).trim_prefix("<").trim_suffix(">")].bytes as PackedByteArray).size()
	var cost := ""
	var quote: Dictionary = await wallet._fee_quote("", "", "", 0)
	var per_gas := int(quote.get("base_wei", 0)) + int(quote.get("tip_wei", 0))
	if per_gas > 0:
		cost = ", about %s PLS at today's base fee" % String.num(float(place.gas) * float(per_gas) / 1e18, 2)
	var lines := PackedStringArray()
	lines.append("%s: %d files, %s. %d already on chain and kept; %d to store (%s)." % [
		place.name, place.paths.size(), String.humanize_size(place.bytes), kept.size(),
		place.to_store.size(), String.humanize_size(new_bytes)])
	lines.append("About %d transactions and %s gas%s. Public and permanent once sent." % [place.transactions, _thousands(place.gas), cost])
	if (place.undeclared as PackedStringArray).size() > 0:
		lines.append("The scripts look like they use %s, which is not ticked: a Player will refuse it." % ", ".join(place.undeclared))
	if not (place.assets as Dictionary).is_empty():
		lines.append("%d place assets travel in the manifest." % (place.assets as Dictionary).size())
	_summary.text = "\n".join(lines)
	_summary.add_theme_color_override("font_color", WARN if (place.undeclared as PackedStringArray).size() > 0 else INK)
	_files.clear()
	var root := _files.create_item()
	for key in place.paths + (place.pictures.keys().map(func(w): return "<%s>" % w)):
		var item := _files.create_item(root)
		var size := (place.contents[key] as PackedByteArray).size() if place.contents.has(key) else (place.pictures[String(key).trim_prefix("<").trim_suffix(">")].bytes as PackedByteArray).size()
		item.set_text(0, String(key))
		item.set_text(1, String.humanize_size(size))
		item.set_text(2, "kept" if kept.has(key) else "store")
		item.set_custom_color(2, MUTED if kept.has(key) else ACCENT)

func _on_progress(done: int, total: int, what: String) -> void:
	_progress.max_value = total
	_progress.value = done
	if what != "done":
		_say("storing %s" % what)

func _ask_and_publish() -> void:
	if place.is_empty():
		return
	var dialog := ConfirmationDialog.new()
	dialog.title = "Publish %s?" % place.name
	dialog.dialog_text = "%d files to store and %d kept, in about %d transactions signed by\n%s.\n\nEverything published is public and permanent: nobody, you included, can take it down.\nA later publish makes a new version with a new link; the old one stays." % [
		place.to_store.size(), (place.kept as Dictionary).size(), place.transactions, wallet.wallet_address]
	dialog.ok_button_text = "Publish"
	add_child(dialog)
	dialog.confirmed.connect(func(): dialog.queue_free(); publish_now())
	dialog.canceled.connect(func(): dialog.queue_free())
	dialog.popup_centered()

## Name from default.project.json, thumbnail and splash from the folder or the one above it.
## place-assets.json only from the one above.
func _fill_from_folder() -> void:
	var dir := _dir.text.strip_edges()
	if dir == "":
		return
	if _name.text.strip_edges() == "":
		var project := dir.path_join("default.project.json")
		var parsed = JSON.parse_string(FileAccess.get_file_as_string(project)) if FileAccess.file_exists(project) else null
		if typeof(parsed) == TYPE_DICTIONARY and String(parsed.get("name", "")) != "":
			_name.text = String(parsed.name)
	for want in [["thumbnail", ["thumbnail.png", "thumbnail.jpg", "../thumbnail.png", "../thumbnail.jpg"]],
			["splash", ["splash.png", "splash.jpg", "../splash.png", "../splash.jpg"]],
			["assets", ["../place-assets.json"]]]:
		var edit: LineEdit = _pictures[want[0]]
		if edit.text.strip_edges() != "":
			continue
		for rel in want[1]:
			var at := dir.path_join(rel).simplify_path()
			if FileAccess.file_exists(at) and (want[0] != "assets" or _is_uri_map(at)):
				edit.text = at
				break

static func _is_uri_map(path: String) -> bool:
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(path))
	if typeof(parsed) != TYPE_DICTIONARY:
		return false
	for k in parsed:
		if not String(k).begins_with("_"):
			return (String(parsed[k]).begins_with("pblockz://"))
	return false

## Adopts CREATOR_KEY from the repo's .env.testnet, the key the Node scripts publish with. Only
## from a checkout: an exported build has the "template" feature and is refused.
func use_repo_creator_key() -> bool:
	var env := ProjectSettings.globalize_path("res://").path_join("../../../.env.testnet").simplify_path()
	if OS.has_feature("template") or not FileAccess.file_exists(env):
		return false
	for line in FileAccess.get_file_as_string(env).split("\n"):
		var kv := line.strip_edges().split("=", true, 1)
		if kv.size() == 2 and kv[0] == "CREATOR_KEY" and kv[1].strip_edges() != "":
			var ok: bool = wallet.adopt_key(kv[1].strip_edges())
			_refresh_wallet()
			return ok
	return false

func _refresh_wallet() -> void:
	if _who == null:
		return
	if wallet.can_buy():
		_who.text = "Signing as %s" % wallet.wallet_address
		_who.add_theme_color_override("font_color", INK)
		_show_balance()
	elif wallet.wallet_address != "":
		_who.text = "Watching %s: a watched address cannot sign. Connect a key to publish." % wallet.wallet_address
		_who.add_theme_color_override("font_color", WARN)
	else:
		_who.text = "No wallet. Connect one, or make one, to publish."
		_who.add_theme_color_override("font_color", WARN)
	_dev_key_button.visible = not OS.has_feature("template") and not wallet.can_buy() \
		and FileAccess.file_exists(ProjectSettings.globalize_path("res://").path_join("../../../.env.testnet").simplify_path())
	_set_buttons()

func _show_balance() -> void:
	var hex: String = await wallet._rpc("eth_getBalance", [wallet.wallet_address, "latest"], wallet._tx_http)
	if hex.begins_with("0x") and wallet.can_buy():
		# Through a decimal string: from 9.2 PLS up a balance in wei no longer fits a 64-bit int.
		_who.text = "Signing as %s, %s PLS" % [wallet.wallet_address, String.num(float(String(wallet._hex_to_dec(hex))) / 1e18, 2)]

func _set_buttons() -> void:
	if _check_button == null:
		return
	_check_button.disabled = busy
	_publish_button.disabled = busy or place.is_empty() or not wallet.can_buy()

func _say(bb: String) -> void:
	_log.append_text(bb + "\n")

static func _thousands(n: int) -> String:
	var s := str(n)
	var out := ""
	while s.length() > 3:
		out = "," + s.right(3) + out
		s = s.left(s.length() - 3)
	return s + out

# ---- the form -------------------------------------------------------------------------------------
func _build_ui() -> void:
	_ui = CanvasLayer.new()
	_ui.layer = 50
	add_child(_ui)
	var back := ColorRect.new()
	back.color = GROUND
	back.set_anchors_preset(Control.PRESET_FULL_RECT)
	_ui.add_child(back)
	var margin := MarginContainer.new()
	margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 24)
	_ui.add_child(margin)
	var split := HBoxContainer.new()
	split.add_theme_constant_override("separation", 24)
	margin.add_child(split)

	var form := VBoxContainer.new()
	form.custom_minimum_size = Vector2(460, 0)
	form.add_theme_constant_override("separation", 10)
	split.add_child(form)
	form.add_child(_label("Publish a place", 28, INK))
	_who = _label("", 13, MUTED)
	_who.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	form.add_child(_who)
	_dev_key_button = Button.new()
	_dev_key_button.text = "Sign with the repo's CREATOR_KEY"
	_dev_key_button.pressed.connect(use_repo_creator_key)
	form.add_child(_dev_key_button)

	_dir = _path_row(form, "Place folder", "the folder with default.project.json", true, "")
	_dir.text_submitted.connect(func(_t): _fill_from_folder())
	_dir.focus_exited.connect(_fill_from_folder)
	form.add_child(_label("Name", 13, MUTED))
	_name = LineEdit.new()
	_name.placeholder_text = "what a Player lists it as"
	form.add_child(_name)
	form.add_child(_label("Description", 13, MUTED))
	_description = TextEdit.new()
	_description.custom_minimum_size = Vector2(0, 64)
	_description.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	form.add_child(_description)

	form.add_child(_label("What it uses outside itself. A Player refuses anything not ticked.", 13, MUTED))
	for cap in Uses.ALL:
		var box := CheckBox.new()
		box.text = "%s: %s" % [cap, Uses.ALL[cap].says]
		box.tooltip_text = Uses.ALL[cap].says
		box.clip_text = true
		box.custom_minimum_size = Vector2(440, 0)
		form.add_child(box)
		_uses[cap] = box

	_pictures["thumbnail"] = _path_row(form, "Thumbnail", "a PNG, JPEG or WebP a Player lists it with", false, "*.png, *.jpg, *.jpeg, *.webp")
	_pictures["splash"] = _path_row(form, "Splash", "a PNG or JPEG shown while it loads", false, "*.png, *.jpg, *.jpeg")
	_pictures["assets"] = _path_row(form, "Place assets", "a name -> pblockz:// uri json (optional)", false, "*.json")

	var buttons := HBoxContainer.new()
	buttons.add_theme_constant_override("separation", 10)
	form.add_child(buttons)
	_check_button = Button.new()
	_check_button.text = "Check"
	_check_button.custom_minimum_size = Vector2(110, 36)
	_check_button.pressed.connect(check)
	buttons.add_child(_check_button)
	_publish_button = Button.new()
	_publish_button.text = "Publish…"
	_publish_button.custom_minimum_size = Vector2(130, 36)
	_publish_button.disabled = true
	_publish_button.pressed.connect(_ask_and_publish)
	buttons.add_child(_publish_button)

	var right := VBoxContainer.new()
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right.add_theme_constant_override("separation", 10)
	split.add_child(right)
	_summary = _label("Choose a place folder and press Check. Nothing is sent until you publish.", 15, MUTED)
	_summary.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	right.add_child(_summary)
	_files = Tree.new()
	_files.columns = 3
	_files.hide_root = true
	_files.column_titles_visible = true
	_files.set_column_title(0, "File")
	_files.set_column_title(1, "Size")
	_files.set_column_title(2, "")
	_files.set_column_expand(1, false)
	_files.set_column_custom_minimum_width(1, 90)
	_files.set_column_expand(2, false)
	_files.set_column_custom_minimum_width(2, 60)
	_files.size_flags_vertical = Control.SIZE_EXPAND_FILL
	right.add_child(_files)
	_progress = ProgressBar.new()
	_progress.visible = false
	right.add_child(_progress)
	var uri_row := HBoxContainer.new()
	uri_row.add_theme_constant_override("separation", 8)
	right.add_child(uri_row)
	_result = LineEdit.new()
	_result.editable = false
	_result.placeholder_text = "the experience's link appears here"
	_result.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	uri_row.add_child(_result)
	var copy := Button.new()
	copy.text = "Copy"
	copy.pressed.connect(func(): if _result.text != "": DisplayServer.clipboard_set(_result.text))
	uri_row.add_child(copy)
	_log = RichTextLabel.new()
	_log.bbcode_enabled = true
	_log.scroll_following = true
	_log.custom_minimum_size = Vector2(0, 140)
	right.add_child(_log)

func _path_row(parent: Control, title: String, hint: String, folder: bool, filters: String) -> LineEdit:
	parent.add_child(_label(title, 13, MUTED))
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 6)
	parent.add_child(row)
	var edit := LineEdit.new()
	edit.placeholder_text = hint
	edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(edit)
	var browse := Button.new()
	browse.text = "Browse…"
	browse.pressed.connect(func():
		var dialog := FileDialog.new()
		dialog.access = FileDialog.ACCESS_FILESYSTEM
		dialog.file_mode = FileDialog.FILE_MODE_OPEN_DIR if folder else FileDialog.FILE_MODE_OPEN_FILE
		if filters != "":
			dialog.filters = PackedStringArray([filters])
		dialog.use_native_dialog = true
		add_child(dialog)
		dialog.dir_selected.connect(func(p): edit.text = p; if folder: _fill_from_folder())
		dialog.file_selected.connect(func(p): edit.text = p)
		dialog.popup_centered_ratio(0.6))
	row.add_child(browse)
	return edit

func _label(text: String, size: int, color: Color) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	return l
