# The client's title screen, in the shared host layer so the Player and the publisher open the
# same way, ahead of a game's own splash -- that one ships with the game. Start plays with
# whatever key is already loaded: the environment, user://player.key, or none, which only reads.
extends CanvasLayer

const Keyfile = preload("res://host/Keyfile.gd")

signal chosen

# Shipped with the app at full size: nothing about the title screen is published or paid for, so
# it skips the asset-by-hash path the game side uses and there is nothing to gain by downscaling.
const TITLE := "res://host/title.png"
## Outside res:// on purpose: never part of a game's own files, and out of reach of a creator's
## script even if the sandbox were bypassed.
const KEY_FILE := "user://player.key"
const WATCH_FILE := "user://watch.address"

# Click bands as fractions of the artwork. The picture draws the buttons themselves, so these
# carry no chrome or text.
const BUTTONS := [
	{"id": "start", "x0": 0.187, "x1": 0.388},
	{"id": "connect", "x0": 0.395, "x1": 0.598},
	{"id": "new", "x0": 0.607, "x1": 0.810},
]
const BAND_TOP := 0.795
const BAND_BOTTOM := 0.884

var _art: TextureRect
var _row: Control
var _note: Label
var _panel: Control
var _wallet: Node

func _init() -> void:
	layer = 200

## Design size the words on top are laid out at; the picture fills the window either way.
const BASE := Vector2(1280, 720)
const SCALE_RANGE := Vector2(1.0, 3.0)

## Window stand-in inside the scaled layer: sized window/f against the layer's scale f, so a
## full-rect child still lands on the window edges.
var _screen: Control

func _fit() -> void:
	var window := Vector2(get_viewport().get_visible_rect().size)
	var f := clampf(minf(window.x / BASE.x, window.y / BASE.y), SCALE_RANGE.x, SCALE_RANGE.y)
	scale = Vector2(f, f)
	if _screen != null and is_instance_valid(_screen):
		_screen.size = window / f
	_layout()

func _ready() -> void:
	_wallet = get_parent().get_node_or_null("Wallet")
	_screen = Control.new()
	_screen.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_screen)
	var back := ColorRect.new()
	back.color = Color(0, 0, 0)
	back.set_anchors_preset(Control.PRESET_FULL_RECT)
	back.mouse_filter = Control.MOUSE_FILTER_STOP
	_screen.add_child(back)

	_art = TextureRect.new()
	# Letterboxed, not cropped: a crop slides the drawn buttons out from under the click bands on
	# any window that is not sixteen by nine.
	_art.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_art.set_anchors_preset(Control.PRESET_FULL_RECT)
	_art.mouse_filter = Control.MOUSE_FILTER_IGNORE
	# In an exported build the png is a remap to Godot's .ctex, and reading it as a png gives a
	# black screen; the raw file is only the fallback for a source run before import.
	if ResourceLoader.exists(TITLE):
		var loaded = load(TITLE)
		if loaded is Texture2D:
			_art.texture = loaded
	if _art.texture == null and FileAccess.file_exists(TITLE):
		var img := Image.new()
		if img.load(TITLE) == OK:
			_art.texture = ImageTexture.create_from_image(img)
	_screen.add_child(_art)

	_row = Control.new()
	_row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_screen.add_child(_row)
	for spec in BUTTONS:
		_row.add_child(_make_button(spec))

	_note = Label.new()
	_note.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_note.add_theme_font_size_override("font_size", 15)
	_note.add_theme_color_override("font_color", Color(0.86, 0.80, 1.0))
	_note.add_theme_constant_override("outline_size", 4)
	_note.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.85))
	_note.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_screen.add_child(_note)
	_say(_greeting())

	get_viewport().size_changed.connect(_fit)
	_fit()
	_layout()
	# A key is asked for once a launch, here, and lives in memory for the session only. Nothing
	# signs until it is: play as a guest and every read still works.
	if not _unlocked():
		match Keyfile.kind(KEY_FILE):
			Keyfile.KEYSTORE:
				_panel = _unlock_panel()
				_screen.add_child(_panel)
			Keyfile.BARE:
				_panel = _protect_panel()
				_screen.add_child(_panel)

## Whether a key is already in hand -- PBLOCKZ_PLAYER_KEY, or a wallet that has been given one.
func _unlocked() -> bool:
	var w = preload("res://host/Wallet.gd")
	if w.setting("PLAYER_KEY") != "" or w.unlocked != "":
		return true
	return _wallet != null and String(_wallet.wallet_address) != ""

## The shell every panel here shares: a heading, a paragraph, and whatever is put in `box`.
func _panel_shell(heading: String, paragraph: String) -> Array:
	var wrap := PanelContainer.new()
	wrap.set_anchors_preset(Control.PRESET_CENTER)
	wrap.position = Vector2(-260, -120)
	wrap.custom_minimum_size = Vector2(520, 0)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 18)
	wrap.add_child(margin)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 10)
	margin.add_child(box)
	var head := Label.new()
	head.text = heading
	head.add_theme_font_size_override("font_size", 20)
	box.add_child(head)
	var body := Label.new()
	body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	body.add_theme_font_size_override("font_size", 13)
	body.modulate = Color(0.78, 0.80, 0.88)
	body.text = paragraph
	box.add_child(body)
	return [wrap, box]

## A passphrase field that submits on Enter.
func _secret_field(placeholder: String) -> LineEdit:
	var f := LineEdit.new()
	f.secret = true
	f.placeholder_text = placeholder
	f.custom_minimum_size = Vector2(0, 34)
	return f

## Asked once a launch when this machine holds a keystore.
func _unlock_panel() -> Control:
	var shell := _panel_shell("Unlock your wallet",
		("%s is kept on this machine, locked with your passphrase. It is read here and held for"
		+ " this session only -- nothing is sent anywhere and nothing is written.\n\nPlay as a guest"
		+ " and the game still reads the chain and shows you what is in it; only signing needs the"
		+ " key.") % _address())
	var wrap: Control = shell[0]
	var box: VBoxContainer = shell[1]
	var field := _secret_field("passphrase")
	box.add_child(field)
	var said := Label.new()
	said.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	said.add_theme_font_size_override("font_size", 13)
	box.add_child(said)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 10)
	box.add_child(row)
	var ok := Button.new()
	ok.text = "Unlock"
	ok.custom_minimum_size = Vector2(120, 34)
	row.add_child(ok)
	var guest := Button.new()
	guest.text = "Play as a guest"
	guest.custom_minimum_size = Vector2(150, 34)
	row.add_child(guest)
	guest.pressed.connect(func():
		wrap.queue_free()
		_panel = null
		_say(_greeting()))
	ok.pressed.connect(func():
		var key := Keyfile.unlock(KEY_FILE, field.text)
		if key == "":
			said.text = "That is not the passphrase for this wallet."
			field.clear()
			field.grab_focus()
			return
		# The Player has no wallet yet; the town and the Publisher do. Both are served.
		preload("res://host/Wallet.gd").unlocked = key
		if _wallet != null and _wallet.has_method("adopt_key"):
			_wallet.adopt_key(key)
		wrap.queue_free()
		_panel = null
		_say(_greeting()))
	field.text_submitted.connect(func(_t): ok.pressed.emit())
	field.grab_focus.call_deferred()
	return wrap

## Asked when a key from before the keystore is found: it is put away properly, once.
func _protect_panel() -> Control:
	var shell := _panel_shell("Give this wallet a passphrase",
		("%s is kept on this machine as a bare key, which anything running as you can read. Set a"
		+ " passphrase and it is written as a keystore -- the same file any Ethereum wallet reads,"
		+ " which you can copy to another machine and open there with the same words.\n\nThere is no"
		+ " way to recover it. Write it down.") % _address())
	var wrap: Control = shell[0]
	var box: VBoxContainer = shell[1]
	var first := _secret_field("a passphrase")
	var again := _secret_field("the same passphrase")
	box.add_child(first)
	box.add_child(again)
	var said := Label.new()
	said.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	said.add_theme_font_size_override("font_size", 13)
	box.add_child(said)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 10)
	box.add_child(row)
	var ok := Button.new()
	ok.text = "Set it"
	ok.custom_minimum_size = Vector2(120, 34)
	row.add_child(ok)
	var later := Button.new()
	later.text = "Play as a guest"
	later.custom_minimum_size = Vector2(150, 34)
	row.add_child(later)
	later.pressed.connect(func():
		wrap.queue_free()
		_panel = null
		_say("The key is still bare, and nothing will sign with it. You will be asked again."))
	ok.pressed.connect(func():
		if first.text.strip_edges() == "":
			said.text = "A passphrase, not nothing."
			return
		if first.text != again.text:
			said.text = "Those two are not the same."
			again.clear()
			again.grab_focus()
			return
		var key := Keyfile.unlock(KEY_FILE, "")
		if key == "" or not Keyfile.write(KEY_FILE, key, first.text):
			said.text = "Could not write %s, so the key was left as it was." % KEY_FILE
			return
		# The Player has no wallet yet; the town and the Publisher do. Both are served.
		preload("res://host/Wallet.gd").unlocked = key
		if _wallet != null and _wallet.has_method("adopt_key"):
			_wallet.adopt_key(key)
		wrap.queue_free()
		_panel = null
		_say(_greeting()))
	again.text_submitted.connect(func(_t): ok.pressed.emit())
	first.grab_focus.call_deferred()
	return wrap

func _greeting() -> String:
	var address := _address()
	if address != "" and _unlocked():
		return "Signed in as %s." % address
	if address != "":
		return "%s is on this machine, locked." % address
	if FileAccess.file_exists(WATCH_FILE):
		return "Watching %s. Reads work; nothing here can sign." % FileAccess.get_file_as_string(WATCH_FILE).strip_edges()
	return "No wallet."

## The address this machine plays as. The town has a Wallet node beside this screen; the Player
## has none while the title is up, since a wallet belongs to a session -- so this repeats
## Wallet.gd's own lookup order, PLAYER_KEY then the key file, and has to track changes to it or
## the greeting names a different identity than the next screen.
func _address() -> String:
	if _wallet != null and _wallet.wallet_address != "":
		return String(_wallet.wallet_address)
	var key := preload("res://host/Wallet.gd").setting("PLAYER_KEY")
	if key != "":
		return PulseBlockzCrypto.address_from_key(key)
	return Keyfile.address(KEY_FILE)

func _say(text: String) -> void:
	_note.text = text

func _make_button(spec: Dictionary) -> Button:
	var b := Button.new()
	b.name = String(spec.id)
	b.focus_mode = Control.FOCUS_NONE
	# The picture has already drawn the button; this is a click target that lights up under the
	# pointer.
	for state in ["normal", "hover", "pressed", "focus", "disabled"]:
		var box := StyleBoxFlat.new()
		box.bg_color = Color(0.55, 0.25, 0.95, 0.0 if state == "normal" else 0.28)
		box.set_corner_radius_all(6)
		box.set_border_width_all(0)
		b.add_theme_stylebox_override(state, box)
	b.pressed.connect(func(): _pressed(String(spec.id)))
	return b

## The letterboxed picture's rect inside the window; every position here is measured against it.
func _art_rect() -> Rect2:
	var view := _screen.size if _screen != null and is_instance_valid(_screen) else get_viewport().get_visible_rect().size
	if _art.texture == null:
		return Rect2(Vector2.ZERO, view)
	var art := _art.texture.get_size()
	var scale: float = min(view.x / art.x, view.y / art.y)
	var size := art * scale
	return Rect2((view - size) / 2.0, size)

func _layout() -> void:
	var r := _art_rect()
	var top := r.position.y + r.size.y * BAND_TOP
	var height := r.size.y * (BAND_BOTTOM - BAND_TOP)
	for i in BUTTONS.size():
		var spec: Dictionary = BUTTONS[i]
		var b: Button = _row.get_child(i)
		b.position = Vector2(r.position.x + r.size.x * float(spec.x0), top)
		b.size = Vector2(r.size.x * (float(spec.x1) - float(spec.x0)), height)
	# Below the button band: inside it the note reads as a caption on one button.
	_note.position = Vector2(r.position.x + r.size.x * 0.12, r.position.y + r.size.y * 0.905)
	_note.size = Vector2(r.size.x * 0.76, r.size.y * 0.085)

func _pressed(id: String) -> void:
	match id:
		"start": _start()
		"new": _new_wallet()
		"connect": _connect()

func _start() -> void:
	chosen.emit()
	queue_free()

## Thirty-two random bytes are a secp256k1 secret key unless they land on zero or past the
## curve's order, so the address is derived first: an empty one means the draw is repeated.
func _new_wallet() -> void:
	if FileAccess.file_exists(KEY_FILE):
		_say("There is already a wallet on this machine. %s is the backup -- it opens anywhere with your passphrase -- so copy it somewhere safe before you delete it to make another." % KEY_FILE)
		return
	var crypto := Crypto.new()
	var key := ""
	var address := ""
	for _try in 8:
		key = "0x" + crypto.generate_random_bytes(32).hex_encode()
		address = PulseBlockzCrypto.address_from_key(key)
		if address != "":
			break
	if address == "":
		_say("Could not make a key. That should be impossible; try again.")
		return
	# Made here, locked here: the panel takes the passphrase before the key is written, so a key
	# never touches the disk bare.
	_panel = _new_passphrase_panel(key, address)
	_screen.add_child(_panel)
	return

## Opened from a place that is closing its own panel, so it waits a frame for _panel to be free.
func _open_new_passphrase(key: String, address: String) -> void:
	_panel = _new_passphrase_panel(key, address)
	_screen.add_child(_panel)

## The passphrase for a wallet just made or just pasted. The key is in hand and goes nowhere
## until this is done.
func _new_passphrase_panel(key: String, address: String) -> Control:
	var shell := _panel_shell("A passphrase for your new wallet",
		("%s is yours. It is not written down anywhere yet: set a passphrase and it is kept as a"
		+ " keystore, the file any Ethereum wallet reads.

Nobody can send this key to you again"
		+ " and there is no way to recover the passphrase. Write it down.") % address)
	var wrap: Control = shell[0]
	var box: VBoxContainer = shell[1]
	var first := _secret_field("a passphrase")
	var again := _secret_field("the same passphrase")
	box.add_child(first)
	box.add_child(again)
	var said := Label.new()
	said.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	said.add_theme_font_size_override("font_size", 13)
	box.add_child(said)
	var ok := Button.new()
	ok.text = "Keep it"
	ok.custom_minimum_size = Vector2(120, 34)
	box.add_child(ok)
	ok.pressed.connect(func():
		if first.text.strip_edges() == "":
			said.text = "A passphrase, not nothing."
			return
		if first.text != again.text:
			said.text = "Those two are not the same."
			again.clear()
			again.grab_focus()
			return
		if not Keyfile.write(KEY_FILE, key, first.text):
			said.text = "Could not write %s, so the key was thrown away rather than half kept." % KEY_FILE
			return
		# The Player has no wallet yet; the town and the Publisher do. Both are served.
		preload("res://host/Wallet.gd").unlocked = key
		if _wallet != null and _wallet.has_method("adopt_key"):
			_wallet.adopt_key(key)
		wrap.queue_free()
		_panel = null
		_say(("Made one: %s
It is kept in %s, locked with your passphrase. That file is the backup:"
			+ " copy it anywhere, and it opens with the same words.")
			% [address, ProjectSettings.globalize_path(KEY_FILE)]))
	again.text_submitted.connect(func(_t): ok.pressed.emit())
	first.grab_focus.call_deferred()
	return wrap

# No wallet library fits: RainbowKit is React over an injected EIP-1193 extension and this is a
# native client with neither. WalletConnect v2 is the right QR pairing to keep the key on the
# phone, but it is a relay protocol with its own key exchange -- work to write, not a dependency.
func _connect() -> void:
	if _panel != null and is_instance_valid(_panel):
		_panel.queue_free()
		_panel = null
		return
	_panel = _ask_panel()
	_screen.add_child(_panel)

func _ask_panel() -> Control:
	var wrap := PanelContainer.new()
	wrap.set_anchors_preset(Control.PRESET_CENTER)
	wrap.position = Vector2(-260, -140)
	wrap.custom_minimum_size = Vector2(520, 0)

	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 18)
	wrap.add_child(margin)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 10)
	margin.add_child(box)

	var head := Label.new()
	head.text = "Connect a wallet"
	head.add_theme_font_size_override("font_size", 20)
	box.add_child(head)

	var body := Label.new()
	body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	body.add_theme_font_size_override("font_size", 13)
	body.modulate = Color(0.78, 0.80, 0.88)
	body.text = ("An ADDRESS (0x and forty hex) is the safe answer: the game reads what you"
		+ " hold, dresses you in it, and cannot sign a thing. That covers everything except"
		+ " taking something off Sal's shelf.\n\nA PRIVATE KEY (0x and sixty-four hex) can"
		+ " sign, and is the one thing you should never paste anywhere. If you do, it is"
		+ " written to a file on this machine and nowhere else -- but a key that has been"
		+ " typed into a game is a key to stop keeping money on.\n\nConnecting a phone wallet"
		+ " by QR, so the key never comes near this machine, is WalletConnect and is not"
		+ " built yet.")
	box.add_child(body)

	var said := Label.new()
	said.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	said.add_theme_font_size_override("font_size", 13)
	box.add_child(said)

	var field := LineEdit.new()
	field.placeholder_text = "0x..."
	field.custom_minimum_size = Vector2(0, 34)
	box.add_child(field)

	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 10)
	box.add_child(row)
	var ok := Button.new()
	ok.text = "Use it"
	ok.custom_minimum_size = Vector2(120, 34)
	row.add_child(ok)
	var cancel := Button.new()
	cancel.text = "Never mind"
	cancel.custom_minimum_size = Vector2(120, 34)
	row.add_child(cancel)

	cancel.pressed.connect(func():
		wrap.queue_free()
		_panel = null)
	ok.pressed.connect(func():
		var typed := field.text.strip_edges()
		var outcome := _adopt(typed)
		if outcome == "":
			wrap.queue_free()
			_panel = null
		else:
			said.text = outcome)
	field.text_submitted.connect(func(_t): ok.pressed.emit())
	return wrap

## Forty hex is a watch-only address, sixty-four a signing key. Returns "" when the input was
## kept, otherwise the message to show.
func _adopt(typed: String) -> String:
	var text := typed.to_lower()
	if not text.begins_with("0x"):
		return "That isn't an address or a key. Both start 0x."
	if text.length() == 42:
		if not text.substr(2).is_valid_hex_number():
			return "That is the right length for an address but has something in it that is not hex."
		var f := FileAccess.open(WATCH_FILE, FileAccess.WRITE)
		if f != null:
			f.store_string(text)
			f.close()
		if _wallet != null and _wallet.has_method("watch_address"):
			_wallet.watch_address(text)
		_say("Watching %s. Reads work; nothing here can sign." % text)
		return ""
	if text.length() == 66:
		var address: String = PulseBlockzCrypto.address_from_key(text)
		if address == "":
			return "That is the right length for a key but is not one this curve accepts."
		# Pasted keys are kept the same way as made ones: the passphrase first, then the file.
		_open_new_passphrase.call_deferred(text, address)
		return ""
	return "An address is 0x and forty hex characters; a key is 0x and sixty-four."
