# The listing thumbnail, shrunk to something a chain can carry.
#
#   godot --headless --path <any project> -s res://../../../scripts/prep-thumb.gd -- <source.png> <out.jpg> [bytes]
#
# The art comes out of the renderer at a couple of megabytes, which is fine on a disk and absurd
# on a ledger: every byte published is paid for in gas, and this one is drawn at the size of a
# card in a list. So it goes down to the width the old one was and is encoded at whatever quality
# lands nearest the size the old one was -- found by trying, because a JPEG's size depends on the
# picture and no quality number predicts it.
extends SceneTree

const WIDTH := 512

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() < 2:
		printerr("usage: -s prep-thumb.gd -- <source.png> <out.jpg> [target bytes]")
		quit(1)
		return
	var src: String = args[0]
	var out: String = args[1]
	var want: int = int(args[2]) if args.size() > 2 else 45634

	var img := Image.new()
	if img.load(src) != OK:
		printerr("could not read %s" % src)
		quit(1)
		return
	var height := int(round(float(WIDTH) * img.get_height() / img.get_width()))
	img.resize(WIDTH, height, Image.INTERPOLATE_LANCZOS)
	print("%dx%d -> %dx%d" % [img.get_width() * img.get_height() / max(height, 1) / max(WIDTH, 1), 0, WIDTH, height])

	# Nearest to the old one's size, from either side: a card that used to be 45 KB should not
	# quietly become 200.
	var best := -1.0
	var best_gap := 1 << 30
	var best_size := 0
	var q := 0.30
	while q <= 0.96:
		if img.save_jpg(out, q) != OK:
			printerr("could not write %s" % out)
			quit(1)
			return
		var size := int(FileAccess.get_file_as_bytes(out).size())
		var gap: int = absi(size - want)
		if gap < best_gap:
			best_gap = gap
			best = q
			best_size = size
		q += 0.02
	img.save_jpg(out, best)
	print("%s  %d bytes at quality %.2f  (the old one was %d)" % [out, best_size, best, want])
	quit(0)
