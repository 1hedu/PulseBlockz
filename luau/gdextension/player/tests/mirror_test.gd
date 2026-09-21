# A place cannot make this machine fetch a url it chose.
#
#   godot --headless --path . -s res://tests/mirror_test.gd
#
# A pblockz:// uri may carry `?src=<url>`, a mirror so a big asset can come from a CDN instead of
# an eth_call. Incoming bytes are checked against the content hash, but the request itself leaves
# the machine carrying whatever the place put in that query, so no mirror is fetched unless both
# gates open: `mirrors_allowed`, the player naming hosts, and the `mirror` capability, the place
# declaring it fetches from outside the chain. `mirrors_allowed` starts empty because no shipped
# asset uses `?src=` -- not in the catalogue, place-assets.json or the manifest on chain -- so a
# host is fetched only once a player names one. This listens on a real socket and counts knocks.
extends SceneTree

const PORT := 8131

var ok := 0
var bad := 0
var server := TCPServer.new()
var assets: Node
var knocks := 0

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	print("MIRROR %-54s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	_run.call_deferred()

## Connections accepted since the last call.
func _knocked() -> int:
	var n := 0
	while server.is_connection_available():
		var c := server.take_connection()
		if c != null:
			n += 1
	return n

func _run() -> void:
	check("a listener is up", server.listen(PORT, "127.0.0.1"), OK)
	assets = load("res://host/ChainAssets.gd").new()
	assets.name = "MirrorProbe"
	root.add_child(assets)
	assets.rpc_url = "http://127.0.0.1:1"      # no chain to answer: a mirror is the only way out
	assets.chain_id = 943

	var uri := "pblockz://" + "ab".repeat(32) + "?src=http://127.0.0.1:%d/beacon?leak=SECRET" % PORT

	check("mirrors are off by default", assets.mirrors_allowed.is_empty(), true)
	var got: PackedByteArray = await assets.fetch(uri)
	for i in 30:
		await process_frame
	knocks = _knocked()
	check("the uri the place chose was never requested", knocks, 0)
	check("and nothing was returned for it", got.size(), 0)

	# The allowed path still fetches, so this cannot pass by mirrors having been removed outright.
	assets.mirrors_allowed = PackedStringArray(["127.0.0.1"])
	check("a named host is allowed again", assets._mirror_allowed("http://127.0.0.1:%d/x" % PORT), true)
	check("but only that host", assets._mirror_allowed("http://example.com/x"), false)
	check("and not as somebody else's path", assets._mirror_allowed("http://evil.test/127.0.0.1"), false)
	# Started, not awaited: this listener accepts and never answers. The knock is the question.
	assets.fetch(uri)
	for i in 120:
		await process_frame
	check("the allowed mirror is actually reached", _knocked() > 0, true)

	# The other gate: a capability list without `mirror` is refused the host the player allowed.
	# A second hash, because a fetch in flight for the first would be joined, not started again.
	var uses := preload("res://host/Uses.gd")
	var uri2 := "pblockz://" + "cd".repeat(32) + "?src=http://127.0.0.1:%d/beacon2" % PORT
	uses.declare(["chain"])
	check("a place that declared no mirror is refused", assets._mirror_allowed("http://127.0.0.1:%d/x" % PORT), false)
	assets.fetch(uri2)
	for i in 90:
		await process_frame
	check("so nothing knocks for it either", _knocked(), 0)
	uses.declare(["chain", "mirror"])
	check("a place that declared it is allowed", assets._mirror_allowed("http://127.0.0.1:%d/x" % PORT), true)
	check("the capability is one the manifest can name", uses.ALL.has("mirror"), true)

	server.stop()
	print("MIRROR %d passed, %d failed" % [ok, bad])
	print("mirror: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
