# The icon set, as SVG source rasterised by Godot at runtime -- no image files,
# and an exported template can do it too. Everything is white on a 24 unit box
# with a 2 unit stroke inside a 20 unit live area, so one texture serves every
# theme and button state by tint alone.
class_name Icons

const BOX := 24.0

const HEAD := '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">'
const STROKE := ' fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>'
const THIN := ' fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>'
# SOLID when the filled mass is what the icon means -- play, stop, a lock body; STROKE otherwise
const SOLID := ' fill="#fff" stroke="none"/>'

# ---- the ribbon -------------------------------------------------------------------
const SRC := {
	# an arrow cursor
	"select": '<path d="M6 3 6 19 10.2 15 13 21 16 19.6 13.2 13.8 19 13Z"' + SOLID,
	# four arrows from a centre
	"move": '<path d="M12 3v18M3 12h18M12 3 9 6M12 3l3 3M12 21l-3-3M12 21l3-3M3 12l3-3M3 12l3 3M21 12l-3-3M21 12l-3 3"' + STROKE,
	# a box with a corner handle pulled out
	"scale": '<path d="M4 20V9h11v11Z"' + THIN + '<path d="M9 4h11v11M20 4 13 11"' + STROKE,
	# a circular arrow
	"rotate": '<path d="M20 12a8 8 0 1 1-3-6.2"' + STROKE + '<path d="M20 4v5h-5"' + STROKE,
	# the isometric brick
	"part": '<path d="M12 3 21 7.5v9L12 21 3 16.5v-9Z"' + THIN + '<path d="M12 12 12 21M12 12 3 7.5M12 12l9-4.5"' + THIN,
	# a brick with a plus
	"insert": '<path d="M12 3 21 7.5v9L12 21 3 16.5v-9Z"' + THIN + '<path d="M12 9v6M9 12h6"' + STROKE,
	# corner brackets round a core
	"group": '<path d="M9 4H4v5M20 9V4h-5M4 15v5h5M15 20h5v-5"' + STROKE + '<path d="M9.5 9.5h5v5h-5Z"' + SOLID,
	"ungroup": '<path d="M9 4H4v5M20 9V4h-5M4 15v5h5M15 20h5v-5"' + STROKE + '<path d="M8 12h8"' + STROKE,
	# an anchor
	"anchor": '<path d="M12 8v12M5 13a7 7 0 0 0 14 0M8 9h8"' + STROKE + '<circle cx="12" cy="5.5" r="2.5"' + THIN,
	# a padlock, shackle stroked, body filled
	"lock": '<path d="M8 10V7.5a4 4 0 0 1 8 0V10"' + STROKE + '<path d="M5.5 10.5h13v9h-13Z"' + SOLID,
	"play": '<path d="M7 4.5 19.5 12 7 19.5Z"' + SOLID,
	"stop": '<path d="M6 6h12v12H6Z"' + SOLID,
	"undo": '<path d="M9 7H15a5 5 0 0 1 0 10H7"' + STROKE + '<path d="M12 4 8.5 7 12 10"' + STROKE,
	"redo": '<path d="M15 7H9a5 5 0 0 0 0 10h8"' + STROKE + '<path d="M12 4l3.5 3L12 10"' + STROKE,
	# scissors
	"cut": '<path d="M7 6l10 12M17 6 7 18"' + THIN + '<circle cx="6" cy="19" r="2.4"' + THIN + '<circle cx="18" cy="19" r="2.4"' + THIN,
	# two sheets
	"copy": '<path d="M9 3h8v13H9Z"' + THIN + '<path d="M6 7v14h9"' + THIN,
	# a clipboard
	"paste": '<path d="M6 5h12v16H6Z"' + THIN + '<path d="M9.5 3h5v3.5h-5Z"' + SOLID,
	"duplicate": '<path d="M4 8h11v13H4Z"' + THIN + '<path d="M9 4h11v11"' + THIN + '<path d="M9.5 14.5h0M6.5 14.5h6M9.5 11.5v6"' + STROKE,
	# a bin
	"delete": '<path d="M5 7h14M10 7V4.5h4V7M7 7l1 14h8l1-14"' + THIN + '<path d="M10.5 11v6M13.5 11v6"' + THIN,
	# a floppy
	"save": '<path d="M4 4h13l3 3v13H4Z"' + THIN + '<path d="M8 4h8v6H8Z"' + SOLID + '<path d="M7 14h10v6H7Z"' + THIN,
	# an open folder
	"open": '<path d="M3 19V6h6l2 2.5h10V19Z"' + THIN,
	"reload": '<path d="M20 12a8 8 0 1 1-2.4-5.7"' + STROKE + '<path d="M20 3.5V9h-5.5"' + STROKE,
	# a magnifier over a box
	"focus": '<path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5"' + STROKE + '<circle cx="12" cy="12" r="3.2"' + THIN,

	# ---- the Explorer -------------------------------------------------------------
	"folder": '<path d="M3 20V7h6l2 2.5h10V20Z"' + THIN,
	"model": '<path d="M9 4H4v5M20 9V4h-5M4 15v5h5M15 20h5v-5"' + THIN + '<path d="M9.5 9.5h5v5h-5Z"' + SOLID,
	"brick": '<path d="M12 3 21 7.5v9L12 21 3 16.5v-9Z"' + THIN + '<path d="M12 12 12 21M12 12 3 7.5M12 12l9-4.5"' + THIN,
	"mesh": '<path d="M12 3 21 7.5v9L12 21 3 16.5v-9Z"' + THIN + '<path d="M12 3 3 16.5M12 3l9 13.5M3 16.5h18"' + THIN,
	"wedge": '<path d="M4 19V8l12 11Z"' + THIN + '<path d="M4 8l4-3 12 11-4 3"' + THIN,
	"seat": '<path d="M7 4v10h11M17 14v6M7 14v6"' + THIN,
	"script": '<path d="M6 3h9l4 4v14H6Z"' + THIN + '<path d="M15 3v4h4"' + THIN + '<path d="M10.5 11 8.5 14l2 3M14 11l2 3-2 3"' + THIN,
	"sound": '<path d="M5 10v4h3l4 3.5V6.5L8 10Z"' + SOLID + '<path d="M15.5 9.5a4 4 0 0 1 0 5"' + THIN,
	"light": '<path d="M12 3v2M5 12H3M21 12h-2M6.2 6.2 4.8 4.8M17.8 6.2l1.4-1.4"' + THIN + '<circle cx="12" cy="13" r="4.2"' + THIN + '<path d="M10 19h4"' + THIN,
	"gui": '<path d="M3 5h18v14H3Z"' + THIN + '<path d="M3 9h18"' + THIN + '<path d="M6 12.5h6"' + THIN,
	"tool": '<path d="M14.5 3.5a4 4 0 0 0 5 5L9 19 5 20l1-4Z"' + THIN,
	"spawn": '<path d="M12 21V4M12 4h8l-2.5 3L20 10h-8"' + THIN,
	"service": '<path d="M4 6h16v12H4Z"' + THIN + '<path d="M4 10h16"' + THIN + '<path d="M7 8h0M10 8h0"' + STROKE,
	"attachment": '<circle cx="12" cy="12" r="3"' + THIN + '<path d="M12 3v4M12 17v4M3 12h4M17 12h4"' + THIN,
	"other": '<circle cx="12" cy="12" r="7"' + THIN,
}

# Class -> shape. Anything not listed falls back by suffix in for_class, then to
# the plain circle: a place has far more classes than there are shapes.
const FOR_CLASS := {
	"Folder": "folder", "Configuration": "folder",
	"Model": "model", "Workspace": "model",
	"Part": "brick", "TrussPart": "brick", "CornerWedgePart": "wedge", "WedgePart": "wedge",
	"MeshPart": "mesh", "SpecialMesh": "mesh", "BlockMesh": "mesh", "CylinderMesh": "mesh",
	"Seat": "seat", "VehicleSeat": "seat",
	"Script": "script", "LocalScript": "script", "ModuleScript": "script",
	"Sound": "sound", "SoundGroup": "sound", "SoundService": "sound",
	"PointLight": "light", "SpotLight": "light", "SurfaceLight": "light", "Lighting": "light",
	"Tool": "tool", "Accessory": "tool", "Accoutrement": "tool",
	"SpawnLocation": "spawn",
	"Attachment": "attachment", "Weld": "attachment", "WeldConstraint": "attachment",
	"Motor6D": "attachment", "HingeConstraint": "attachment", "BallSocketConstraint": "attachment",
}

# Tint per shape, so the Explorer reads by hue as well as by shape.
const TINT := {
	"folder": Color("d9a441"), "model": Color("8fb4e8"), "brick": Color("9aa4b2"),
	"mesh": Color("9aa4b2"), "wedge": Color("9aa4b2"), "seat": Color("6fa8dc"),
	"script": Color("74c98f"), "sound": Color("c58ad9"), "light": Color("f0d060"),
	"gui": Color("7fc7cf"), "tool": Color("e08a4b"), "spawn": Color("7fd18f"),
	"service": Color("7f8896"), "attachment": Color("b0b8c4"), "other": Color("8b93a1"),
}

static var _cache := {}

static func icon(id: String, px := 18) -> Texture2D:
	var key := "%s@%d" % [id, px]
	if _cache.has(key):
		return _cache[key]
	if not SRC.has(id):
		_cache[key] = null
		return null
	var img := Image.new()
	# load_svg_from_string takes a scale: pixels per unit of the 24 unit box.
	var err := img.load_svg_from_string(HEAD + SRC[id] + "</svg>", float(px) / BOX)
	if err != OK:
		push_warning("Icons: no SVG support here (error %d); the Studio runs without icons." % err)
		_cache[key] = null
		return null
	_cache[key] = ImageTexture.create_from_image(img)
	return _cache[key]

static func for_class(class_name_: String) -> String:
	if FOR_CLASS.has(class_name_):
		return FOR_CLASS[class_name_]
	for tail in ["Gui", "Frame", "Label", "Button", "Box", "Layout", "Constraint"]:
		if class_name_.ends_with(tail):
			return "gui" if tail != "Constraint" else "attachment"
	if class_name_.ends_with("Service") or class_name_.ends_with("Storage") or class_name_.begins_with("Starter"):
		return "service"
	if class_name_.ends_with("Part"):
		return "brick"
	return "other"

static func tint_for(shape: String) -> Color:
	return TINT.get(shape, TINT["other"])
