# COPY -- do not edit. The original is luau/gdextension/host/Rpc.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# JSON-RPC over HTTP to a chain node: no key, no signing. An HTTPRequest refuses a second
# request while the first is in flight, so callers take a lane from the pool and `many`
# spreads a batch across lanes to run them at once.
class_name PulseBlockzRpc
extends Node

@export var url: String = "https://rpc.v4.testnet.pulsechain.com"

## Concurrent HTTPRequests. One serves a single question; a shelf-full of items is a
## hundred-odd calls.
const LANES := 8

var _lanes: Array[HTTPRequest] = []
var _taken := {}

func _ready() -> void:
	for _i in LANES:
		var lane := HTTPRequest.new()
		# Unthreaded, the TLS handshake runs inside the frame, and every request opens a new
		# connection: a hundred-odd milliseconds of frozen game per call to a public node.
		lane.use_threads = true
		# HTTPRequest has no timeout by default: a silent node would hold its lane for good
		lane.timeout = 20.0
		add_child(lane)
		_lanes.append(lane)

## An unused lane, marked taken; null if none comes free within 30 s. `limit` confines the
## search to the first N lanes.
func _free_lane(limit: int = 0) -> HTTPRequest:
	var pool := _lanes if limit <= 0 else _lanes.slice(0, limit)
	for _try in 600:
		for lane in pool:
			if not _taken.get(lane, false):
				_taken[lane] = true
				if _try >= 20:
					print("[rpc] waited %.1f s for a lane" % (_try * 0.05))
				return lane
		await get_tree().create_timer(0.05).timeout
	print("[rpc] no lane came free in 30 s")
	return null

func _release(lane: HTTPRequest) -> void:
	_taken[lane] = false

## One call. "" when the node did not answer -- a down node is an outcome, not an error.
func one(method: String, params: Array) -> String:
	var got := await one_dict(method, params)
	return String(got.get("result", "")) if got.has("result") else ""

## One call, with the whole response -- when the error matters or the result is not a string.
func one_dict(method: String, params: Array) -> Dictionary:
	var lane := await _free_lane()
	if lane == null:
		return {}
	var body := JSON.stringify({"jsonrpc": "2.0", "id": 1, "method": method, "params": params})
	var err := lane.request(url, ["content-type: application/json"], HTTPClient.METHOD_POST, body)
	if err != OK:
		_release(lane)
		return {}
	var t0 := Time.get_ticks_msec()
	var res: Array = await lane.request_completed
	_release(lane)
	if res[1] != 200:
		print("[rpc] %s answered %d (result %d) after %d ms" % [method, res[1], res[0], Time.get_ticks_msec() - t0])
		return {}
	if Time.get_ticks_msec() - t0 > 5000:
		print("[rpc] %s took %d ms" % [method, Time.get_ticks_msec() - t0])
	var parsed = JSON.parse_string((res[3] as PackedByteArray).get_string_from_utf8())
	return parsed if typeof(parsed) == TYPE_DICTIONARY else {}

## Many calls, answered in the order asked: "" for one that failed, [] if the whole batch did
## or any part of it never went out.
## [] is a chain that did not answer, never a chain with nothing on it.
##
## Every request starts before any is awaited, so the batch costs about as long as its slowest
## lane. Answers arrive through one-shot callbacks: a signal is not queued, so awaiting a lane
## that already finished hangs for good.
func many(calls: Array) -> Array:
	if calls.is_empty():
		return []
	var out := []
	out.resize(calls.size())
	for i in out.size():
		out[i] = ""
	# A sweep holds its lanes for fifteen seconds and more, so it leaves 3 free for a single
	# question -- a nonce, a gas estimate. 8 calls or fewer count as a question and may use any.
	var usable := _lanes.size() if calls.size() <= 8 else maxi(1, _lanes.size() - 3)
	var per := int(ceil(float(calls.size()) / usable))
	# A slice that never went out leaves its calls padded with "", which a caller reads as an
	# answer of zero. The batch is [] instead: the outcome this function names.
	var unasked := false
	var waiting := 0
	var done := [0]
	var t0 := Time.get_ticks_msec()
	for start in range(0, calls.size(), per):
		var lane := await _free_lane(usable)
		if lane == null:
			unasked = true
			break
		var slice := calls.slice(start, mini(start + per, calls.size()))
		var body := []
		for i in slice.size():
			var c: Dictionary = slice[i]
			body.append({"jsonrpc": "2.0", "id": start + i + 1,
				"method": c.get("method", "eth_call"), "params": c.get("params", [])})
		waiting += 1
		# Released per request, not per batch: one stalled request must not hold the pool.
		var answered := func(result, code, _h, bytes):
			if code == 200:
				var parsed = JSON.parse_string((bytes as PackedByteArray).get_string_from_utf8())
				if typeof(parsed) == TYPE_ARRAY:
					for row in parsed:
						if typeof(row) != TYPE_DICTIONARY:
							continue
						var idx := int(row.get("id", 0)) - 1
						if idx >= 0 and idx < out.size():
							out[idx] = String(row.get("result", ""))
			else:
				print("[rpc] a batch of %d answered %d (result %d) after %d ms" % [slice.size(), code, result, Time.get_ticks_msec() - t0])
			done[0] += 1
			_release(lane)
		lane.request_completed.connect(answered, CONNECT_ONE_SHOT)
		if lane.request(url, ["content-type: application/json"], HTTPClient.METHOD_POST,
				JSON.stringify(body)) != OK:
			# A one-shot that never fires stays connected, and the next batch to take this lane
			# would be answered twice and release it twice over.
			lane.request_completed.disconnect(answered)
			unasked = true
			done[0] += 1
			_release(lane)
	var spun := 0
	while done[0] < waiting and spun < 1200:
		spun += 1
		await get_tree().create_timer(0.05).timeout
	if Time.get_ticks_msec() - t0 > 5000:
		print("[rpc] a batch of %d calls took %d ms" % [calls.size(), Time.get_ticks_msec() - t0])
	if unasked:
		print("[rpc] a batch of %d calls went out short, so none of it is an answer" % calls.size())
		return []
	return out

## A threaded request holds the process open until it answers or times out, so quitting a place
## with a lane still waiting on a node that is not answering waits with it. They are cancelled on
## the way out: an answer nobody is left to read is worth nothing.
func _exit_tree() -> void:
	for lane in _lanes:
		if is_instance_valid(lane):
			lane.cancel_request()
