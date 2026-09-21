# The game's only route to PulseChain. The sandbox disables HttpService's request methods, so
# a script asks by attribute on a Chain node; this does the JSON-RPC and answers there.
extends Node

const Tx = preload("res://host/Tx.gd")
const Abi = preload("res://host/Abi.gd")
const TypedData = preload("res://host/TypedData.gd")

signal published(data: Dictionary)

## Derived from the loaded key, never configured. Empty until a key or a watch address arrives.
@export var wallet_address: String = ""
@export var rpc_url: String = "https://rpc.v4.testnet.pulsechain.com"
## The explorer's API, not the node: it answers what JSON-RPC cannot, such as an account's
## transaction history. Read-only and keyless.
@export var explorer_api: String = "https://api.scan.v4.testnet.pulsechain.com/api"
@export var explorer_site: String = "scan.v4.testnet.pulsechain.com"
@export var mainnet_rpc: String = "https://rpc.pulsechain.com"
@export var refresh_seconds: float = 60.0
# How many of the newest registry ids to look at, or 0 for all. A window, not a page size: one
# scan feeds both the shop and an owner's inventory, so a cap hides old items from both.
@export var scan_ids: int = 0
@export var auto_start: bool = true
## Sign as the repo's CREATOR_KEY (.env.testnet, three folders up) when no player key is set.
## Ignored in an exported build whatever this says. Off by default for the case that check
## misses: an unexported app built beside the repo -- a Player, a publisher -- must not sign as
## the creator without saying so.
@export var dev_key_from_repo: bool = false
## Reads in flight at once. Four stays under what the public node starts refusing at.
const LANES := 4

## The actions that leave the chain changed, and so schedule a re-read. A read never does.
const CHANGES_SOMETHING := ["write", "store", "swap", "add_liquidity", "remove_liquidity",
	"add_token", "forget_token"]

## Forty-eight is the largest batch this node answers whole, and the size a town's own scan
## rounds at (CALLS_PER_ROUND in Chain.server.luau). MAX_READS is eight of those trips and a
## place pages past it: eight round trips is already a slow read.
const READS_PER_TRIP := 48
const MAX_READS := 384

## The same ceiling for fetching.
const MAX_FETCHES := 384

## Not found and failed its hash are deliberately one answer: from where a place stands both
## mean it does not have the thing, and telling them apart invites acting on the difference.
const _NOTHING_BACK := "nothing came back for that, or it did not match its hash"
## Seconds to wait for a receipt. Blocks are ~10s and the public node can lag several behind.
const RECEIPT_SECONDS := 120


## What a model fetched from the chain may contain. An allowlist: the wardrobe clones one into
## the character, which is in Workspace, where a class carrying a Source would run as the
## wearer. Never add such a class, and never turn this into a denylist.
const ASSET_CLASSES := [
	"Accessory", "Accoutrement", "Hat", "Model", "Folder", "Part", "MeshPart", "WedgePart",
	"Attachment", "Weld", "WeldConstraint", "Motor6D",
	# Baked geometry: a union or a negation is MeshData, which cannot hold source.
	"UnionOperation", "NegateOperation",
	"Decal", "Texture", "SpecialMesh", "BlockMesh", "CylinderMesh",
	"PointLight", "SpotLight", "SurfaceLight",
	"ParticleEmitter", "Sparkles", "Fire", "Smoke",
	"BillboardGui", "TextLabel", "ImageLabel", "Frame", "UICorner",
]

## Decimal-string arithmetic: a balance is eighteen digits and a double holds fifteen, so no
## amount here goes near a float.
const Decimal = preload("res://host/Decimal.gd")
const Keyfile = preload("res://host/Keyfile.gd")

## The key the title screen unlocked, for as long as this process runs: in memory, never
## written and never logged. The Player has no Wallet while its title is up -- Player.tscn is
## one node and the Wallet is in Session.tscn -- so the door leaves the key here and every
## session takes it from here as it starts.
static var unlocked := ""

## PulseX as deployed on this testnet -- different addresses from the mainnet contracts.
const PULSEX := {
	"chain_id": 943,
	"router": "0xDaE9dd3d1A52CfCe9d5F2fAC7fDe164D500E50f7",
	"factory": "0xFf0538782D122d3112F75dc7121F61562261c0f7",
	"wpls": "0x70499adEBB11Efd915E3b69E700c331778628707",
	"site": "app.pulsex.com",
	"screener": "dexscreener.com/pulsechain",
}

## What the swap opens with, each checked against the testnet factory for a real WPLS pool. A
## decoy "Test Incentive" shares the INC ticker on this chain with no pair at all; this INC is
## the one with the pool. The empty address is native PLS, which PulseX trades as WPLS through
## the router's ETH entry points.
const PULSEX_TOKENS := [
	{"symbol": "PLS",  "address": "", "decimals": 18, "name": "Pulse"},
	{"symbol": "PLSX", "address": "0x8a810ea8B121d08342E9e7696f4a9915cBE494B7", "decimals": 18, "name": "PulseX"},
	{"symbol": "INC",  "address": "0x6eFAfcb715F385c71d8AF763E8478FeEA6faDF63", "decimals": 18, "name": "Incentive"},
	{"symbol": "HEX",  "address": "0x2b591e99afE9f32eAA6214f7B7629768c40Eeb39", "decimals": 8,  "name": "HEX"},
	{"symbol": "USDC", "address": "0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48", "decimals": 6,  "name": "USD Coin"},
	{"symbol": "DAI",  "address": "0x6B175474E89094C44Da98b954EedeAC495271d0F", "decimals": 18, "name": "Dai Stablecoin"},
	{"symbol": "USDT", "address": "0xdAC17F958D2ee523a2206206994597C13D831ec7", "decimals": 6,  "name": "Tether USD"},
	{"symbol": "WETH", "address": "0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2", "decimals": 18, "name": "Wrapped Ether"},
]



## Ask before signing. Off only for headless tests.
@export var confirm_purchases: bool = true


# PulseChain testnet v4 (chainId 943) — scripts/testnet.js writes addresses.943.json.
const ADDRESSES := {
	"network": "PulseChain testnet v4",
	"chain_id": 943,
	# Where this client writes by default. Reads work against any deployment, because a
	# pblockz:// uri names the store it came from, and a place may name its own to write to.
	# The registry, the marketplace and the donation address live in demo2's Contracts.luau.
	"AssetStore": "0x0f9D08e13BE2345856026615d05F7251F07efAfA",
	# ERC-20 balances to read beside the account's PLS.
	"tokens": [],
}

var world: PulseBlockzWorld
var _toast                  # PulseBlockzToast: what came of a transaction
var _chain                  # PulseBlockzRpc: the node, its lanes, and batching
var _http: HTTPRequest      # reads: the periodic refresh
var _lanes: Array[HTTPRequest] = []   # extra readers, so a big batch does not go one at a time
## Which request nodes are in use, by instance id. An HTTPRequest refuses a second request
## while one is in flight, and that refusal reads as an empty answer, so the lock goes on the
## node rather than the caller: _batch's first lane is _http, the node _rpc also reaches for.
var _node_busy := {}
var _mounted_uris := {}     # uri -> the model fetch(as = "model") mounted for it
var _web: HTTPRequest       # anything that is not the chain: DexScreener, the explorer
var _tx_http: HTTPRequest   # writes: buying, so a purchase never queues behind a refresh
var _busy := false
var assets: Node                  # ChainAssets, for pblockz:// fetching and verification
var _published_once := false
var _meta := {}          # token address -> {symbol, decimals}, so each is fetched once
var _key := ""           # the player's private key, if any; never published
var _sign_in_nonce := ""  # the challenge the world handed over, until it has been signed
var _chain_id := 0       # instance id of ServerStorage.Chain, where the place writes requests
var _handled: Array[int] = []   # request numbers already answered, most recent 256
var _preview_n := 0             # serial, so each preview gets its own filename
var _buying := false
var _refresh_wanted := false
var _refresh_after := 0
## Quiet period after the last write, so a run of transactions costs one refresh between them.
const REFRESH_QUIET_MS := 3000
var last_published := {}
## The last payload as a string, so an unchanged one is never written again.
var _published_json := ""

func _ready() -> void:
	_toast = preload("res://host/Toast.gd").new()
	_toast.name = "Toast"
	_toast.explorer_site = explorer_site
	add_child(_toast)
	_pulsex = preload("res://host/Pulsex.gd").new()
	_pulsex.name = "Pulsex"
	# For a local fork (scripts/dev-chain.js), which keeps chain id 943 so nothing else changes.
	var node_override := setting("RPC_URL")
	if node_override != "":
		rpc_url = node_override
		print("[wallet] talking to %s (PBLOCKZ_RPC_URL)" % rpc_url)
	_chain = preload("res://host/Rpc.gd").new()
	_chain.name = "Rpc"
	_chain.url = rpc_url
	add_child(_chain)
	_pulsex.rpc = _chain
	_pulsex.wallet = self
	add_child(_pulsex)
	# Threaded: an unthreaded request runs its TLS handshake inside the frame and stutters the game.
	_http = HTTPRequest.new()
	_http.use_threads = true
	add_child(_http)
	_tx_http = HTTPRequest.new()
	_tx_http.use_threads = true
	add_child(_tx_http)
	_web = HTTPRequest.new()
	_web.use_threads = true
	add_child(_web)
	# This node answers a batched eth_call strictly in order: 46 calls take most of a minute on
	# one connection, and about the slowest lane's time spread over several.
	for i in LANES:
		var lane := HTTPRequest.new()
		lane.use_threads = true
		add_child(lane)
		_lanes.append(lane)
	_pulsex._load_custom_tokens()
	if world == null:
		world = get_parent().get_node_or_null("World")
	# After the world is found: a click on a card asks the place to show the transaction.
	_toast.world = world
	if assets == null:
		assets = get_node_or_null("/root/ChainAssets")
	if assets != null:
		assets.rpc_url = rpc_url
		assets.chain_id = ADDRESSES.chain_id
		# A uri that names only its content says no store, so the client supplies one.
		assets.asset_store = String(ADDRESSES.AssetStore)
	_load_key()
	# The world hands over a nonce when the player joins. The signature says which address this
	# is; the server checks it and puts the address on the Player.
	if world != null and world.has_signal("sign_in_requested"):
		world.sign_in_requested.connect(_on_sign_in_requested)
	# A pblockz:// the engine needs to draw or simulate something. The bytes are checked against
	# their hash and cached to a local path, which is this machine's and travels nowhere.
	if world != null and world.has_signal("asset_wanted"):
		world.asset_wanted.connect(_on_asset_wanted)
	if world != null:
		# Requests arrive as attributes, and get_attributes stays empty unless the world mirrors
		# properties. Off by default: the mirror costs a map write per changed property per frame.
		world.track_properties = true
		# Made only by the side that answers the place, then replicated: a joined client making
		# its own copy would give the tree two OnChain folders and its scripts the first found.
		if _answers_the_place():
			world.run_chunk("chain_folder", """
local rs = game:GetService("ReplicatedStorage")
if not rs:FindFirstChild("OnChain") then
    local f = Instance.new("Folder") f.Name = "OnChain" f.Parent = rs
end
""")
			_publish_place_assets()
		_watch_requests()
		_watch_own_asks()
		_coalesce_refresh()
# A server has no wallet of its own; the place's own server scripts read the town's state
# through the request channel instead.
	if auto_start and _has_own_player():
		await get_tree().create_timer(0.5).timeout
		print("[wallet] reading the chain for %s" % wallet_address)
		while is_inside_tree():
			await refresh()
			await get_tree().create_timer(max(refresh_seconds, 5.0)).timeout

## Fetch everything and publish it into the world. Safe to call at any time.
func refresh() -> void:
	# Two refreshes cannot overlap; a refresh and a purchase can, having a request node each.
	if _busy or world == null or not _has_own_player() or wallet_address == "":
		return
	_busy = true
	if not _published_once:
		_publish({"status": "loading", "address": wallet_address, "network": ADDRESSES.network,
			"can_buy": can_buy(), "gas": "?", "gas_wei": "0", "tokens": [],
			"assetstore": ADDRESSES.AssetStore})
	var data := await _collect()
	_busy = false
	if data.is_empty():
		return
	data["status"] = "ok"

	# Writing the payload wakes every watcher in the place and each decodes the whole thing, so
	# an unchanged payload is not written. The height and base fee move every block and would
	# defeat the comparison, so they are left out of it: what is published carries the height as
	# of the last real change, and a place wanting the current one calls Multicall3's
	# getBlockNumber for itself.
	var compared := data.duplicate(true)
	if compared.get("chain") is Dictionary:
		compared.chain.erase("block")
		compared.chain.erase("base_fee")
	var fresh := JSON.stringify(compared)
	if fresh == _published_json:
		return
	_published_json = fresh
	_published_once = true
	last_published = data
	_publish(data)
	published.emit(data)
	print("[wallet] published: %s, %s PLS, %d token(s)" % [data.address, data.gas, data.tokens.size()])

# ---- the key -----------------------------------------------------------------------
# The environment, then user://, then the repo's gitignored .env.testnet. None of them is
# inside res://, so no key is part of the game's own files.
## An environment setting: PBLOCKZ_PLAYER_KEY, PBLOCKZ_RPC_URL, PBLOCKZ_NO_DEV_KEY. The older
## prefix is honoured too, so a shell or test runner setting it still counts.
static func setting(name: String) -> String:
	var got := OS.get_environment("PBLOCKZ_" + name).strip_edges()
	# The old prefix, spelled in pieces so a rename sweep cannot take the line that reads it.
	return got if got != "" else OS.get_environment("PB" + "LOX_" + name).strip_edges()

func _load_key() -> void:
	# A hosted server holds no key at all (SERVER.md): not loaded, rather than loaded and then
	# refused by the PLAYER_ONLY gate.
	if world != null and int(world.mode) == 1:
		return
	var key := setting("PLAYER_KEY")
	# PBLOCKZ_NO_DEV_KEY: pass over everything this machine keeps -- the repo's .env,
	# user://player.key, user://watch.address -- so only a key handed to this process counts.
	# For running several players on one machine.
	var keyless := setting("NO_DEV_KEY") != ""
	# Not read from the file here: a keystore needs the passphrase, which the title screen asks
	# for once a launch. Until somebody unlocks it this wallet signs nothing, which is a guest.
	if key == "" and not keyless:
		key = unlocked
	# An address with no key: read for it, sign nothing.
	if key == "" and not keyless and FileAccess.file_exists("user://watch.address"):
		var watched := FileAccess.get_file_as_string("user://watch.address").strip_edges()
		if watched.length() == 42 and watched.begins_with("0x"):
			wallet_address = PulseBlockzCrypto.checksum_address(watched)
			print("[wallet] watching %s; reads work, signing is off" % wallet_address)
			return
	if key == "" and dev_key_from_repo and not OS.has_feature("template") and not keyless:
		# CREATOR_KEY: the address that publishes the catalogue and the assets on this testnet.
		var env := ProjectSettings.globalize_path("res://").path_join("../../../.env.testnet").simplify_path()
		if FileAccess.file_exists(env):
			for line in FileAccess.get_file_as_string(env).split("\n"):
				var kv := line.strip_edges().split("=", true, 1)
				if kv.size() == 2 and kv[0] == "CREATOR_KEY":
					key = kv[1].strip_edges()
	if key == "":
		print("[wallet] no key; reads work, buying is off (set PBLOCKZ_PLAYER_KEY to enable)")
		return
	var addr: String = PulseBlockzCrypto.address_from_key(key)
	if addr == "":
		push_warning("Wallet: PBLOCKZ_PLAYER_KEY is not a valid secret key; buying stays off")
		return
	_key = key
	wallet_address = addr
	print("[wallet] buying enabled for %s" % addr)

## Takes a key handed over at the title screen, without a restart. The address comes from the
## key, as in _load_key, never from whatever was typed beside it.
func adopt_key(key: String) -> bool:
	var clean := key.strip_edges()
	var addr: String = PulseBlockzCrypto.address_from_key(clean)
	if addr == "":
		push_warning("Wallet: that is not a valid secret key; nothing changed")
		return false
	_key = clean
	wallet_address = addr
	print("[wallet] signing for %s" % addr)
	# A key that arrived after the player joined signs in now.
	_answer_sign_in()
	if auto_start:
		refresh()
	return true

## Reads for an address without signing for it: `_key` stays empty, so can_buy() is false and
## every path that would sign refuses up front.
func watch_address(address: String) -> bool:
	var clean := address.strip_edges().to_lower()
	if clean.length() != 42 or not clean.begins_with("0x") or not clean.substr(2).is_valid_hex_number():
		return false
	_key = ""
	wallet_address = PulseBlockzCrypto.checksum_address(clean)
	print("[wallet] watching %s; reads work, signing is off" % wallet_address)
	if auto_start:
		refresh()
	return true

func can_buy() -> bool:
	return _key != ""

# ---- sign-in -------------------------------------------------------------------------

## A fetch the world asked for, tried three times before it is reported missing.
func _on_asset_wanted(uri: String, _kind: String) -> void:
	var path := ""
	for attempt in 3:
		path = await _cache_media(uri)
		if path != "" or not is_inside_tree():
			break
		await get_tree().create_timer(2.0 * (attempt + 1)).timeout
	if world != null:
		world.asset_arrived(uri, path)

func _on_sign_in_requested(nonce: String) -> void:
	_sign_in_nonce = nonce
	_answer_sign_in()

## Signs the sign-in nonce and hands it to the world. Once per nonce: the server takes one
## proof per challenge. Keyless, the player stays a guest.
func _answer_sign_in() -> void:
	if _sign_in_nonce == "" or _key == "" or world == null:
		return
	# The message names the server this machine dialled, so the proof cannot be relayed to another.
	var message: String = world.sign_in_message(_sign_in_nonce)
	# EIP-191 personal message: a prefix no transaction can be mistaken for.
	var prefixed := String.chr(0x19) + "Ethereum Signed Message:\n%d" % message.to_utf8_buffer().size() + message
	var digest: PackedByteArray = PulseBlockzCrypto.keccak256(prefixed.to_utf8_buffer())
	var signature: String = PulseBlockzCrypto.sign_digest(digest, _key)
	if signature == "":
		return
	_sign_in_nonce = ""
	print("[wallet] signing in as %s" % wallet_address)
	world.answer_sign_in(wallet_address, signature)

# ---- requests from the world --------------------------------------------------------
# A script cannot reach the network, so it writes a request as an attribute on the place's
# Chain node and this loop answers it there.
## Whether this process answers the place's requests. Attributes always replicate (rbx_net.cpp:
## any property starting `@` goes to every client), so a hosted server's request lands in every
## joined client's mirror too; without this gate their wallet answers it as well. Solo is both
## halves in one process and answers. A joined client answers only its own scripts, in
## _watch_own_asks.
func _answers_the_place() -> bool:
	return world != null and int(world.mode) != 2   # 2 is Client

func _watch_requests() -> void:
	if not _answers_the_place():
		print("[wallet] joined as a client: the server answers the place, not this wallet")
		return
	while is_inside_tree():
		await get_tree().create_timer(0.4).timeout
		# Nothing new is read while a purchase is on screen: one at a time.
		if _buying or world == null:
			continue
		if _chain_id == 0:
			_chain_id = _find_instance("ServerStorage/Chain")
			if _chain_id == 0:
				continue
			print("[wallet] listening for requests on ServerStorage.Chain")
		var attrs: Dictionary = world.get_attributes(_chain_id)
		var raw := String(attrs.get("Request", ""))
		if raw == "":
			continue
		# A list of everything still waiting, not the last thing asked: one request per attribute
		# drops every write made between two ticks, silently. A lone dictionary is still accepted.
		var parsed = JSON.parse_string(raw)
		var queue: Array = []
		if typeof(parsed) == TYPE_ARRAY:
			queue = parsed
		elif typeof(parsed) == TYPE_DICTIONARY:
			queue = [parsed]
		else:
			continue
		for entry in queue:
			if typeof(entry) != TYPE_DICTIONARY:
				continue
			var req: Dictionary = entry
			# Matched by number, not ordered by it: a high-water mark would let one
			# n = 2147483647 mute every later request.
			var seq := int(req.get("n", -1))
			if seq < 0 or _handled.has(seq):
				continue
			_handled.append(seq)
			if _handled.size() > 256:
				_handled.remove_at(0)
			# The action alone: a catalogue read is dozens of calls not worth stringifying.
			print("[wallet] request #%d: %s" % [seq, String(req.get("action", ""))])
			await _handle_request(seq, req)

## Re-reads the chain once, a moment after the last write. Never while a request is in flight:
## a refresh is a hundred-odd calls across the same lanes.
func _coalesce_refresh() -> void:
	while is_inside_tree():
		await get_tree().create_timer(0.5).timeout
		if not _refresh_wanted or _buying:
			continue
		if Time.get_ticks_msec() < _refresh_after:
			continue
		_refresh_wanted = false
		await refresh()

# ---- this player's own asks -----------------------------------------------------------
# A player's own client scripts asking their own wallet (shared/OwnWallet.luau) through the
# AskWallet attribute: a client script's write reaches its own host and no further, so this is
# answered on the player's machine and never on a server.

## Whether this machine has a player of its own to answer for.
func _has_own_player() -> bool:
	return world != null and int(world.mode) != 1   # 1 is Server

var _own_chain_id := 0
var _own_handled: Array[int] = []

func _watch_own_asks() -> void:
	if not _has_own_player():
		return
	while is_inside_tree():
		await get_tree().create_timer(0.3).timeout
		if _buying or world == null:
			continue
		if _own_chain_id == 0:
			_own_chain_id = _find_instance("ReplicatedStorage/Chain")
			if _own_chain_id == 0:
				continue
		var raw := String((world.get_attributes(_own_chain_id) as Dictionary).get("AskWallet", ""))
		if raw == "":
			continue
		var parsed = JSON.parse_string(raw)
		if typeof(parsed) != TYPE_ARRAY:
			continue
		for entry in parsed:
			if typeof(entry) != TYPE_DICTIONARY:
				continue
			var seq := int(entry.get("n", -1))
			if seq < 0 or _own_handled.has(seq):
				continue
			_own_handled.append(seq)
			if _own_handled.size() > 256:
				_own_handled.remove_at(0)
			print("[wallet] this player asks #%d: %s" % [seq, String(entry.get("action", ""))])
			# Awaited only for what signs, because nonces have to go in order. A read is left to
			# run, so a quote does not queue behind a sweep of every pair.
			if String(entry.get("action", "")) in PLAYER_ONLY:
				await _handle_request(seq, entry, _reply_own)
			else:
				_handle_request(seq, entry, _reply_own)

## Back to this player's own scripts, into their own tree.
func _reply_own(seq: int, result: Dictionary) -> void:
	var out := result.duplicate()
	out["n"] = seq
	world.run_client_chunk("wallet_reply", """
local c = game:GetService("ReplicatedStorage"):FindFirstChild("Chain")
if c then c:SetAttribute("WalletResult", %s) end
""" % preload("res://host/Luau.gd").quote(JSON.stringify(out)))

func _find_instance(path: String) -> int:
	var id := 0
	for want in path.split("/"):
		var found := 0
		for cid in world.get_child_ids(id):
			if String((world.get_instance(cid) as Dictionary).get("name", "")) == want:
				found = cid
				break
		if found == 0:
			return 0
		id = found
	return id

const USES := preload("res://host/Uses.gd")
const DANGER := preload("res://host/Danger.gd")
const GAS := preload("res://host/Gas.gd")

## What only a player's own wallet may do. A server is refused these in _handle_request, and
## _watch_own_asks awaits only these.
const PLAYER_ONLY := ["write", "store", "sign_typed", "swap", "add_liquidity", "remove_liquidity", "add_token", "forget_token"]

## Capabilities already refused out loud, so a panel asking every keystroke is told once.
var _refused_said: Array[String] = []

## Says a refusal on this client's own screen.
func _say_refusal(capability: String) -> void:
	if _toast == null or not is_instance_valid(_toast):
		return
	var what := String(USES.ALL.get(capability, {}).get("says", capability))
	# A joined client's permissions were chosen at the door, not declared by the place.
	var how := "Leave the server and join again with the wallet box ticked to allow it." \
		if world != null and int(world.mode) == 2 \
		else "This place did not declare it, so the client will not do it."
	_toast.raise_done("Not allowed here", "%s. %s" % [what, how], false)

func _handle_request(seq: int, req: Dictionary, reply: Callable = Callable()) -> void:
	var action := String(req.get("action", ""))
	# A server holds no key (SERVER.md), so its channel reads and nothing else: a signing request
	# that reached it should have gone to the player instead (Ledger.askPlayer).
	if not reply.is_valid() and int(world.mode) == 1 and action in PLAYER_ONLY:
		_reply(seq, {"ok": false, "message": "The town doesn't sign for anybody. That has to come from the player's own wallet."})
		return
	# What the place declared it uses (Uses.gd), refused here before any of it runs.
	var capability: String = USES.for_action(action)
	if capability != "" and not USES.permits(capability):
		var refused := {"ok": false, "message": USES.refusal(capability)}
		if reply.is_valid(): reply.call(seq, refused)
		else: _reply(seq, refused)
		# The place is handed the refusal and free to show it nowhere, so say it here too.
		if not _refused_said.has(capability):
			_refused_said.append(capability)
			_say_refusal(capability)
		return
	# The action allowlist: read, write, who, store, fetch and sign_typed are the vocabulary, and
	# the rest are one town's features. Anything not named here is dropped without a reply.
	if not action in ["read", "write", "who", "store", "fetch", "sign_typed",
			"quote", "pool_quote", "swap", "add_token", "forget_token", "positions",
			"add_liquidity", "remove_liquidity"]:
		return
	# A quote spends nothing, so it must not block the loops that poll for requests.
	_buying = not action in ["quote", "pool_quote", "read", "who", "fetch"]
	var result := {}
	match action:
		# calls = {...} for many in one round trip, to/fn/args for one.
		"read":
			if typeof(req.get("calls")) == TYPE_ARRAY:
				result = await chain_read_many(req.get("calls"))
			else:
				result = await chain_read(String(req.get("to", "")), String(req.get("fn", "")),
					req.get("args", []), req.get("returns", []))
		"write": result = await chain_write(String(req.get("to", "")), String(req.get("fn", "")),
			req.get("args", []), String(req.get("value", "")), String(req.get("note", "")))
		"store": result = await chain_store(String(req.get("bytes", "")), String(req.get("mime", "")),
			String(req.get("note", "")), String(req.get("to", "")))
		"fetch": result = await host_fetch(req)
		"sign_typed": result = await sign_typed(req.get("typed", {}), String(req.get("note", "")))
		"who": result = chain_who()
		"quote": result = await pulsex_quote(String(req.get("from", "")), String(req.get("to", "")),
			String(req.get("amount", "")), int(req.get("slippage", 50)))
		"swap": result = await pulsex_swap(String(req.get("from", "")), String(req.get("to", "")),
			String(req.get("amount", "")), int(req.get("slippage", 50)))
		# The matching amount, both balances, the prices and the share of the pool. Reads only.
		"pool_quote": result = await pulsex_pool_quote(String(req.get("token", "")), String(req.get("token_b", "")),
			String(req.get("amount", "")), String(req.get("side", "a")))
		"add_token": result = await add_token(String(req.get("address", "")))
		"forget_token": result = forget_token(String(req.get("address", "")))
		"positions": result = {"ok": true, "positions": await pulsex_positions()}
		"add_liquidity": result = await pulsex_add_liquidity(String(req.get("token", "")), String(req.get("token_b", "")),
			String(req.get("amount", "")), int(req.get("slippage", 100)))
		"remove_liquidity": result = await pulsex_remove_liquidity(String(req.get("token", "")), String(req.get("token_b", "")),
			int(req.get("percent", 100)), int(req.get("slippage", 100)))
	_buying = false
	if reply.is_valid():
		reply.call(seq, result)
	else:
		_reply(seq, result)
	# Writes only: a refresh writes the payload, which wakes the place into a read of its own, so
	# refreshing after a read would loop. The reply above goes back immediately either way.
	if result.get("ok", false) and action in CHANGES_SOMETHING:
		_refresh_wanted = true
		_refresh_after = Time.get_ticks_msec() + REFRESH_QUIET_MS

# ---- what a place may ask of a wallet -------------------------------------------------
# A place names the contract, the function and the arguments -- never calldata. There is no
# keccak in the sandbox to build a selector with, and the selector here is hashed from the
# signature the prompt showed, so a place cannot display "add(bytes32,uint8)" while calling
# "transferOwnership(address)".

## A token amount arrives as {amount = "1.5", decimals = 18}, because the scaled integer is
## past what a Luau number holds exactly. Anything else passes through untouched.
func _scaled(v):
	if typeof(v) != TYPE_DICTIONARY or not v.has("amount"):
		return v
	var units := _to_units(String(v.get("amount", "")), int(v.get("decimals", 18)))
	return units if units != "" else null

## An argument as it reads on the prompt: the amount as typed, not the units.
func _arg_shown(v) -> String:
	if typeof(v) == TYPE_DICTIONARY and v.has("amount"):
		return "%s%s" % [String(v.get("amount", "")),
			("" if String(v.get("symbol", "")) == "" else " " + String(v.get("symbol", "")))]
	return str(v)

## "add(bytes32,uint8)" -> ["bytes32", "uint8"]. Empty for "who()" and for anything malformed.
func _arg_types(fn: String) -> Array:
	var open := fn.find("(")
	var close := fn.rfind(")")
	if open < 0 or close < open:
		return []
	var inner := fn.substr(open + 1, close - open - 1).strip_edges()
	if inner == "":
		return []
	var out := []
	for t in inner.split(","):
		out.append(t.strip_edges())
	return out

## One returned value, by its type. A static type is the word itself; a dynamic one -- string,
## bytes, T[] -- holds an offset to its body further down the return.
func _read_word(type: String, hex: String, i: int):
	return _read_at(type, hex, 0, i)

## The word at `i`, as a number. Offsets are byte counts, and a word is thirty-two of them.
func _at(hex: String, i: int) -> int:
	var w := Abi.word(hex, i).lstrip("0")
	return int(("0x" + w).hex_to_int()) if w != "" else 0

## `from` is the word that offsets inside this tuple are measured from: zero for a return, and
## the first word of an array's body for its elements.
func _read_at(type: String, hex: String, from: int, i: int):
	if type.ends_with("[]"):
		var base := from + _at(hex, from + i) / 32
		var count := _at(hex, base)
		var inner := type.substr(0, type.length() - 2)
		var out := []
		# The length word is whatever the node sent, so the loop is bounded by the words present.
		var words := (hex.trim_prefix("0x").length()) / 64
		for k in min(count, max(words - base - 1, 0)):
			out.append(_read_at(inner, hex, base + 1, k))
		return out
	if type == "string" or type == "bytes":
		var base := from + _at(hex, from + i) / 32
		return _read_bytes_at(type, hex, base)
	var w := Abi.word(hex, from + i)
	if w == "":
		return null
	if type == "bool":
		return w.trim_prefix("0x").lstrip("0") != ""
	if type == "address":
		return PulseBlockzCrypto.checksum_address("0x" + w.substr(w.length() - 40))
	if type.begins_with("uint") or type.begins_with("int"):
		return _hex_to_dec("0x" + w)
	return "0x" + w        # bytes32 and anything else: as it came

## A length word, then that many bytes: text for a string, hex for bytes.
func _read_bytes_at(type: String, hex: String, base: int):
	var n := _at(hex, base)
	var body := hex.trim_prefix("0x")
	var at := (base + 1) * 64
	if at >= body.length():
		return "" if type == "string" else "0x"
	var taken := body.substr(at, min(n * 2, body.length() - at))
	if type == "bytes":
		return "0x" + taken
	var raw := PackedByteArray()
	for k in range(0, taken.length() - 1, 2):
		raw.append(("0x" + taken.substr(k, 2)).hex_to_int())
	return raw.get_string_from_utf8()

## A view call: no signature, no gas, no prompt.
func chain_read(to: String, fn: String, args: Array, returns: Array) -> Dictionary:
	var types := _arg_types(fn)
	if types.size() != args.size():
		return {"ok": false, "message": "%s takes %d argument(s), got %d" % [fn, types.size(), args.size()]}
	var data := Abi.selector(fn) + (Abi.encode(types, args) if types.size() > 0 else "")
	var hex := await _tx_call(to, data)
	if hex == "":
		return {"ok": false, "message": "the node did not answer that call"}
	var out := {"ok": true, "data": hex, "words": []}
	for i in returns.size():
		out.words.append(_read_word(String(returns[i]), hex, i))
	return out

## Many view calls in one round trip. Each entry is a read as chain_read takes one --
## {to, fn, args, returns} -- and the answers come back in the same order and the same shape.
## A call that will not encode gets its own {ok = false, message} in its slot; the rest go out.
func chain_read_many(calls: Array) -> Dictionary:
	if calls.size() > MAX_READS:
		return {"ok": false, "message": "a read takes at most %d calls, got %d" % [MAX_READS, calls.size()]}
	var out := []
	out.resize(calls.size())
	var specs := []          # what actually goes to the node
	var slots := []          # where each spec's answer belongs in `out`
	var wants := []          # the return types for each spec
	for i in calls.size():
		var c = calls[i]
		if typeof(c) != TYPE_DICTIONARY:
			out[i] = {"ok": false, "message": "each call must be a table"}
			continue
		var to := String(c.get("to", ""))
		var fn := String(c.get("fn", ""))
		var args: Array = c.get("args", []) if typeof(c.get("args")) == TYPE_ARRAY else []
		var returns: Array = c.get("returns", []) if typeof(c.get("returns")) == TYPE_ARRAY else []
		if not to.begins_with("0x") or to.length() != 42:
			out[i] = {"ok": false, "message": "that is not an address"}
			continue
		var types := _arg_types(fn)
		if types.size() != args.size():
			out[i] = {"ok": false, "message": "%s takes %d argument(s), got %d" % [fn, types.size(), args.size()]}
			continue
		var scaled := []
		for a in args:
			scaled.append(_scaled(a))
		var data := Abi.selector(fn) + (Abi.encode(types, scaled) if types.size() > 0 else "")
		specs.append(_call_spec(to, data))
		slots.append(i)
		wants.append(returns)

	# Sliced: past a certain size the node stops answering the batch at all, and a batch that did
	# not answer looks exactly like a chain with nothing on it. The lanes still overlap inside a slice.
	var answers := []
	for from in range(0, specs.size(), READS_PER_TRIP):
		var part := await _batch(specs.slice(from, min(from + READS_PER_TRIP, specs.size())))
		if part.is_empty():
			# The whole read fails rather than padding blanks, which would read as an empty chain.
			return {"ok": false, "message": "the node did not answer that batch"}
		answers.append_array(part)

	for j in slots.size():
		var hex := String(answers[j]) if j < answers.size() else ""
		if hex == "":
			out[slots[j]] = {"ok": false, "message": "the node did not answer that call"}
			continue
		var one := {"ok": true, "data": hex, "words": []}
		for k in (wants[j] as Array).size():
			one.words.append(_read_word(String(wants[j][k]), hex, k))
		out[slots[j]] = one
	return {"ok": true, "results": out}

## A transaction, shown to the player first. The prompt carries what the wallet encoded, not
## what the place claimed it was.
func chain_write(to: String, fn: String, args: Array, value: String, note: String) -> Dictionary:
	if _key == "":
		return {"ok": false, "message": "No key loaded, so I can't sign anything for you."}
	if not to.begins_with("0x") or to.length() != 42:
		return {"ok": false, "message": "that is not an address"}
	# An empty signature is a plain PLS transfer: no contract, no call, nothing to encode.
	var plain := fn.strip_edges() == ""
	if plain and (value == "" or value == "0"):
		return {"ok": false, "message": "a transfer of nothing, with nothing to call"}
	var types := _arg_types(fn)
	if not plain and types.size() != args.size():
		return {"ok": false, "message": "%s takes %d argument(s), got %d" % [fn, types.size(), args.size()]}
	var values := []
	for v in args:
		var scaled = _scaled(v)
		if scaled == null:
			return {"ok": false, "message": "that is not an amount I can read"}
		values.append(scaled)
	var data := ""
	if not plain:
		data = Abi.selector(fn) + (Abi.encode(types, values) if types.size() > 0 else "")

	# `note` is the place's own words and is labelled as such; everything under it is the
	# wallet's own reading of the call.
	var lines := PackedStringArray()
	if note != "":
		lines.append("\"%s\" says: %s" % [_place_name(), note])
		lines.append("(the place's words -- not checked by the wallet)")
		lines.append("")
	lines.append("To      %s" % to)
	if plain:
		lines.append("Call    nothing -- a plain transfer")
	else:
		lines.append("Call    %s" % fn)
		for i in types.size():
			lines.append("          %s = %s" % [types[i], _arg_shown(args[i])])
	lines.append("Value   %s" % ("nothing leaves your account" if value == "" or value == "0" else value + " PLS"))
	lines.append("Network %s" % ADDRESSES.network)
	var wei := "0x0"
	if value != "" and value != "0":
		wei = "0x" + Tx.arg_uint_dec(_to_units(value, 18)).lstrip("0")
		if wei == "0x": wei = "0x0"
	var danger: PackedStringArray = DANGER.of_call(to, "" if plain else fn, args, value)
	var fee := await _fee_quote(to, "0x" + data.trim_prefix("0x") if data != "" else "0x", wei, 21000 if plain else 300000)
	if String(fee.estimate_failed) != "":
		danger.append("The chain expects this to fail (%s). Sending it anyway spends the gas." % fee.estimate_failed)
	if confirm_purchases and not await _confirm("Send this transaction?", "\n".join(lines), danger, fee):
		return {"ok": false, "message": "You waved that one off. Nothing was sent."}

	var sent := await _send(to, data, 21000 if plain else 300000, wei, fee)
	if not sent.ok:
		return {"ok": false, "message": sent.message}
	return {"ok": true, "hash": sent.get("hash", ""), "message": "Sent."}

## Signs EIP-712 typed data -- eth_signTypedData_v4 -- and hands back the signature. Nothing is
## sent and nothing is paid until somebody else submits it. The digest is computed from exactly
## what the prompt showed (host/TypedData.gd), and the domain's chain must be this wallet's, or
## a place could collect signatures worth something on another chain.
func sign_typed(typed, note: String) -> Dictionary:
	if _key == "":
		return {"ok": false, "message": "No key loaded, so I can't sign anything for you."}
	if typeof(typed) != TYPE_DICTIONARY:
		return {"ok": false, "message": "there is nothing there to sign"}
	var domain = typed.get("domain", {})
	if typeof(domain) != TYPE_DICTIONARY:
		return {"ok": false, "message": "typed data needs a domain"}
	if domain.has("chainId") and int(domain.chainId) != int(ADDRESSES.chain_id):
		return {"ok": false, "message": "That is for chain %d, and this wallet is on %d. Not signing it." % [int(domain.chainId), int(ADDRESSES.chain_id)]}
	var hashed: Dictionary = TypedData.digest(typed)
	if not hashed.get("ok", false):
		return {"ok": false, "message": String(hashed.get("message", "that is not typed data I can read"))}

	var lines := PackedStringArray()
	if note != "":
		lines.append("\"%s\" says: %s" % [_place_name(), note])
		lines.append("(the place's words -- not checked by the wallet)")
		lines.append("")
	lines.append("Signing sends nothing and costs nothing.")
	lines.append("")
	lines.append("For     %s%s" % [String(domain.get("name", "?")),
		(" v" + String(domain.get("version", ""))) if domain.has("version") else ""])
	if domain.has("verifyingContract"):
		lines.append("Contract %s" % String(domain.verifyingContract))
	lines.append("Network %s" % ADDRESSES.network)
	lines.append("")
	lines.append(String(typed.get("primaryType", "")))
	var message = typed.get("message", {})
	for field in (typed.get("types", {}) as Dictionary).get(String(typed.get("primaryType", "")), []):
		var name := String(field.get("name", ""))
		lines.append("  %s = %s" % [name, _typed_shown(message.get(name))])
	var danger: PackedStringArray = DANGER.of_typed(typed)
	if confirm_purchases and not await _confirm("Sign this message?", "\n".join(lines), danger):
		return {"ok": false, "message": "You waved that one off. Nothing was signed."}
	var signature: String = PulseBlockzCrypto.sign_digest(hashed.digest, _key)
	if signature == "":
		return {"ok": false, "message": "the signature did not come out"}
	# v as 27 or 28, which is what eth_signTypedData_v4 gives and all OpenZeppelin's ECDSA.recover
	# accepts. sign_digest ends in the bare recovery id, 0 or 1, right only for a tx's yParity.
	var v := ("0x" + signature.right(2)).hex_to_int()
	if v < 27:
		signature = signature.left(signature.length() - 2) + "%02x" % (v + 27)
	return {"ok": true, "signature": signature, "address": wallet_address,
		"digest": "0x" + (hashed.digest as PackedByteArray).hex_encode(), "message": "Signed."}

## A typed value, as it reads on the prompt: a list as a list, a whole number without ".0".
func _typed_shown(v) -> String:
	if typeof(v) == TYPE_ARRAY:
		var parts := PackedStringArray()
		for item in v:
			parts.append(_typed_shown(item))
		return "[" + ", ".join(parts) + "]"
	if typeof(v) == TYPE_FLOAT and v == floor(v):
		return str(int(v))
	return str(v)

## Put bytes on the chain and hand back the pblockz:// uri they landed at. Not expressible as a
## `write`: each store's content hash is an argument to the next call, so a place cannot name
## the call until the earlier stores have landed. The prompt shows the size, gas scaling with it.
func chain_store(b64: String, mime: String, note: String, store: String = "") -> Dictionary:
	if _key == "":
		return {"ok": false, "message": "No key loaded, so I can't sign anything for you."}
	var data := Marshalls.base64_to_raw(b64)
	if data.is_empty():
		return {"ok": false, "message": "there is nothing there to store"}
	if data.size() > 24575:
		return {"ok": false, "message": "that is too big for one chunk (%d bytes)" % data.size()}
	var clean_mime := mime.strip_edges()
	if clean_mime == "":
		clean_mime = "application/octet-stream"

	var target := store if store != "" else String(ADDRESSES.AssetStore)
	var fee := await _fee_quote(target, _store_call(data, clean_mime), "0x0", 400000 + data.size() * 260)
	var lines := PackedStringArray()
	if note != "":
		lines.append("\"%s\" says: %s" % [_place_name(), note])
		lines.append("(the place's words -- not checked by the wallet)")
		lines.append("")
	lines.append("Size    %s (%d bytes)" % [String.humanize_size(data.size()), data.size()])
	lines.append("Kind    %s" % clean_mime)
	lines.append("Network %s" % ADDRESSES.network)
	lines.append("")
	lines.append("Published this way it is public and permanent: addressed by the hash of its")
	lines.append("own bytes, readable by anybody, and there is nobody who can take it down.")
	var danger := PackedStringArray()
	if String(fee.estimate_failed) != "":
		danger.append("The chain expects this to fail (%s). Sending it anyway spends the gas." % fee.estimate_failed)
	if confirm_purchases and not await _confirm("Publish this to the chain?", "\n".join(lines), danger, fee):
		return {"ok": false, "message": "You waved that one off. Nothing was sent."}
	# A place may name a store, so its content lands beside the rest of its assets.
	if store != "" and (not store.begins_with("0x") or store.length() != 42):
		return {"ok": false, "message": "that is not an AssetStore address"}
	return await _store_asset(data, clean_mime, store, fee)

## Bytes back off the chain, refused by ChainAssets unless they hash to the name asked for.
## No key, no gas, no prompt. What to fetch:
##
##     {uri = "pblockz://..."}                one
##     {uris = {...}}                         many, warmed in one round trip before any is read
##     {hash = ..., store = ..., blob = ...}  the parts, when a contract handed over a hash
##
## and `as` says in what form:
##
##     ""       base64 bytes
##     "text"   a string
##     "file"   cached to disk, path back -- what an ImageLabel can be pointed at
##     "model"  decoded and mounted for cloning; answers with the name and slot it went in under
func host_fetch(req) -> Dictionary:
	# A bare uri instead of a table.
	if typeof(req) == TYPE_STRING:
		req = {"uri": req}
	if typeof(req) != TYPE_DICTIONARY:
		return {"ok": false, "message": "a fetch takes a uri or a table"}
	var want := String(req.get("as", ""))
	if not want in ["", "text", "file", "model"]:
		return {"ok": false, "message": "a fetch can be as bytes, text, file or model"}

	# Warmed together first: sixty documents are one round trip asked for at once, sixty singly.
	if typeof(req.get("uris")) == TYPE_ARRAY:
		var uris := []
		for e in req.uris:
			uris.append(_uri_of(e))
		if uris.size() > MAX_FETCHES:
			return {"ok": false, "message": "a fetch takes at most %d, got %d" % [MAX_FETCHES, uris.size()]}
		if assets != null:
			var real := []
			for u in uris:
				if u != "" and not real.has(u):
					real.append(u)
			# Two ways onto the chain, and a batch is handed a mix: a blob is read with eth_call,
			# while calldata needs its manifest read first. Each prefetch ignores what is not its own.
			await assets.prefetch(real)
			await assets.prefetch_calldata(real)
		var out := []
		for u in uris:
			out.append(await _fetch_one(u, want))
		return {"ok": true, "results": out}

	return await _fetch_one(_uri_of(req), want)

## A uri, however it was named: a place holding a bare hash from a contract gets one composed
## here, rather than every place implementing the addressing scheme again.
func _uri_of(e) -> String:
	if typeof(e) == TYPE_STRING:
		return _content_uri(String(e)) if is_content_hash(String(e)) else String(e)
	if typeof(e) != TYPE_DICTIONARY:
		return ""
	var uri := String(e.get("uri", ""))
	if uri != "":
		# A document names an asset by bare content hash, so `uri` may hold one.
		return _content_uri(uri) if is_content_hash(uri) else uri
	var hash := String(e.get("hash", ""))
	if hash == "":
		return ""
	if int(e.get("blob", 0)) <= 0:
		return _content_uri(hash)
	return PulseBlockzChain.format_asset_uri({
		"content_hash": hash, "has_chain": true,
		"chain_id": int(e.get("chain_id", ADDRESSES.chain_id)),
		"store": String(e.get("store", ADDRESSES.AssetStore)),
		"blob_id": int(e.get("blob", 0)),
		"mime": String(e.get("mime", "application/json"))})

## True for exactly 32 bytes of hex and nothing else.
static func is_content_hash(s: String) -> bool:
	var h := s.trim_prefix("0x")
	if h.length() != 64:
		return false
	return h.is_valid_hex_number(false)

## A uri from a bare content hash. Which store holds it and what kind of thing it is are on
## chain already (ChainAssets asks), so a document naming either would have to be rewritten the
## day the store changed.
func _content_uri(hash: String) -> String:
	return PulseBlockzChain.format_asset_uri({"content_hash": hash})

func _fetch_one(uri: String, want: String) -> Dictionary:
	if not uri.begins_with("pblockz://"):
		return {"ok": false, "message": "that is not a pblockz:// uri"}
	if assets == null:
		return {"ok": false, "message": "there is nothing here to fetch with"}
	if want == "file":
		var path := await _cache_media(uri)
		if path == "":
			return {"ok": false, "message": _NOTHING_BACK, "uri": uri}
		return {"ok": true, "path": path, "uri": uri}
	if want == "model":
		return await _mount_model(uri)
	var data: PackedByteArray = await assets.fetch(uri)
	if data.is_empty():
		return {"ok": false, "message": _NOTHING_BACK, "uri": uri}
	if want == "text":
		return {"ok": true, "text": data.get_string_from_utf8(), "size": data.size(), "uri": uri}
	return {"ok": true, "bytes": Marshalls.raw_to_base64(data), "size": data.size(), "uri": uri}

## Bytes into an instance the game can clone. Once per uri: a uri is a content hash, so what it
## names cannot change, and rebuilding it from JSON is a frame hitch.
func _mount_model(uri: String) -> Dictionary:
	if _mounted_uris.has(uri):
		var was: Dictionary = _mounted_uris[uri]
		return {"ok": true, "name": was.name, "slot": was.slot, "uri": uri}
	var raw: PackedByteArray = await assets.fetch(uri)
	if raw.is_empty():
		return {"ok": false, "message": _NOTHING_BACK, "uri": uri}
	var parsed = JSON.parse_string(raw.get_string_from_utf8())
	if typeof(parsed) != TYPE_DICTIONARY:
		return {"ok": false, "message": "that is not a model", "uri": uri}
	var forbidden := _forbidden_classes(parsed)
	if not forbidden.is_empty():
		return {"ok": false, "message": "that model contains %s" % ", ".join(forbidden), "uri": uri}
	var slot := _slot_of(parsed)
	# Nested pblockz:// uris -- a Decal's Texture, say -- are left as they are: the engine
	# resolves one wherever it draws it, on every client the model replicates to.
	var named := String((parsed.get("properties", {}) as Dictionary).get("Name", ""))
	if named == "":
		named = "Model" + uri.substr(8, 12)
	if world != null:
		world.add_model("ReplicatedStorage/OnChain", named, JSON.stringify(parsed))
	_mounted_uris[uri] = {"name": named, "slot": slot}
	return {"ok": true, "name": named, "slot": slot, "uri": uri}

## Whose wallet this is and what it can do, so a place can tell before it offers a counter.
func chain_who() -> Dictionary:
	return {"ok": true, "address": wallet_address, "can_sign": _key != "",
		"network": ADDRESSES.network, "chain_id": ADDRESSES.chain_id}

## The name the running place goes by, for attributing its own words on the prompt.
func _place_name() -> String:
	if world == null:
		return "this place"
	# A place names itself with a Name attribute on ReplicatedStorage.Place, unchecked.
	var id := _find_instance("ReplicatedStorage/Place")
	if id != 0:
		var got = world.get_attributes(id).get("Name", "")
		if typeof(got) == TYPE_STRING and got != "":
			return String(got)
	return "this place"

func _reply(seq: int, result: Dictionary) -> void:
	result["n"] = seq
	world.run_chunk("chain_reply", """
local c = game:GetService("ServerStorage"):FindFirstChild("Chain")
if c then c:SetAttribute("Result", %s) end
""" % preload("res://host/Luau.gd").quote(JSON.stringify(result)))

# ---- buying ---------------------------------------------------------------------------
func _confirm(question: String, detail: String, warnings: PackedStringArray = PackedStringArray(), fee: Dictionary = {}) -> bool:
	var layer := CanvasLayer.new()
	layer.layer = 100
	add_child(layer)

	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.55)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	layer.add_child(dim)

	var panel := PanelContainer.new()
	panel.set_anchors_preset(Control.PRESET_CENTER)
	panel.position = Vector2(-230, -90)
	panel.custom_minimum_size = Vector2(460, 0)
	layer.add_child(panel)

	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 18)
	panel.add_child(margin)

	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 10)
	margin.add_child(box)

	var heading := Label.new()
	heading.text = question
	heading.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	heading.add_theme_font_size_override("font_size", 18)
	box.add_child(heading)

	for warning in warnings:
		var loud := Label.new()
		loud.text = "WARNING: " + warning
		loud.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		loud.add_theme_font_size_override("font_size", 14)
		loud.add_theme_color_override("font_color", Color(1.0, 0.36, 0.3))
		box.add_child(loud)

	var small := Label.new()
	small.text = detail
	small.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	small.add_theme_font_size_override("font_size", 12)
	small.modulate = Color(0.75, 0.79, 0.86)
	box.add_child(small)

	# Only when this spends gas. What is confirmed is written back into `fee`, which is what gets bid.
	var read_fee := func() -> Dictionary: return {"ok": true}
	if not fee.is_empty():
		read_fee = _fee_section(box, fee)

	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_END
	row.add_theme_constant_override("separation", 8)
	box.add_child(row)

	var no := Button.new()
	no.text = "Reject"
	no.custom_minimum_size = Vector2(110, 34)
	row.add_child(no)
	var yes := Button.new()
	yes.text = "Confirm"
	yes.custom_minimum_size = Vector2(140, 34)
	row.add_child(yes)
	# With a warning up, focus starts on Reject, so Enter does not confirm.
	if warnings.is_empty():
		yes.grab_focus()
	else:
		no.grab_focus()

	var answered := [false]
	yes.pressed.connect(func():
		var chosen: Dictionary = read_fee.call()
		if not chosen.get("ok", false):
			return
		answered[0] = true
		layer.queue_free())
	no.pressed.connect(func(): layer.queue_free())
	await layer.tree_exited
	return answered[0]

## Draws the Fees section and returns a callable that validates what was typed, writes it into
## `fee` and says whether it may be sent.
func _fee_section(box: VBoxContainer, fee: Dictionary) -> Callable:
	var title := Label.new()
	title.text = "Fees  (base fee now %s gwei)" % GAS.wei_to_gwei(int(fee.base_wei))
	title.add_theme_font_size_override("font_size", 13)
	box.add_child(title)

	var speeds := HBoxContainer.new()
	speeds.add_theme_constant_override("separation", 6)
	box.add_child(speeds)
	var group := ButtonGroup.new()
	var buttons := {}
	for s in GAS.SPEEDS:
		var b := Button.new()
		b.name = "Speed" + s
		b.text = "%s  %s gwei" % [s, GAS.wei_to_gwei(int(fee.presets[s]))]
		b.toggle_mode = true
		b.button_group = group
		b.button_pressed = s == String(fee.speed)
		speeds.add_child(b)
		buttons[s] = b

	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", 10)
	box.add_child(grid)
	var field := func(label: String, text: String, name: String) -> LineEdit:
		var l := Label.new()
		l.text = label
		l.add_theme_font_size_override("font_size", 12)
		grid.add_child(l)
		var e := LineEdit.new()
		e.name = name
		e.text = text
		e.custom_minimum_size = Vector2(160, 0)
		grid.add_child(e)
		return e
	var tip_edit: LineEdit = field.call("Tip (gwei)", GAS.wei_to_gwei(int(fee.tip_wei)), "Tip")
	var max_edit: LineEdit = field.call("Max fee (gwei)", GAS.wei_to_gwei(int(fee.max_fee_wei)), "MaxFee")
	var needs_limit := int(fee.gas_limit) > 0
	var limit_edit: LineEdit = null
	if needs_limit:
		limit_edit = field.call("Gas limit", str(int(fee.gas_limit)), "GasLimit")

	var cost := Label.new()
	cost.name = "FeeCost"
	cost.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	cost.add_theme_font_size_override("font_size", 12)
	box.add_child(cost)

	var read := func() -> Dictionary:
		return GAS.read_choice(tip_edit.text, max_edit.text, limit_edit.text if limit_edit else "", needs_limit)
	var refresh := func() -> void:
		var c: Dictionary = read.call()
		if not c.ok:
			cost.text = String(c.message)
			cost.add_theme_color_override("font_color", Color(1.0, 0.45, 0.35))
			return
		cost.remove_theme_color_override("font_color")
		if needs_limit:
			cost.text = "At most %s PLS: gas limit x max fee.%s" % [GAS.max_cost_pls(int(c.gas_limit), int(c.max_fee_wei)),
				"" if bool(fee.estimated) else " The limit is a guess -- the chain could not estimate it."]
		else:
			cost.text = "Each transaction pays the base fee plus the tip, never more than the max fee per gas."
	for s in GAS.SPEEDS:
		var speed: String = s
		buttons[s].pressed.connect(func():
			fee.speed = speed
			tip_edit.text = GAS.wei_to_gwei(int(fee.presets[speed]))
			max_edit.text = GAS.wei_to_gwei(GAS.max_fee_for(int(fee.base_wei), int(fee.presets[speed])))
			refresh.call())
	var typed := func(_t: String) -> void:
		fee.speed = "Custom"
		for s in GAS.SPEEDS:
			buttons[s].set_pressed_no_signal(false)
		refresh.call()
	tip_edit.text_changed.connect(typed)
	max_edit.text_changed.connect(typed)
	if limit_edit:
		limit_edit.text_changed.connect(typed)
	refresh.call()

	return func() -> Dictionary:
		var c: Dictionary = read.call()
		if c.ok:
			fee.tip_wei = int(c.tip_wei)
			fee.max_fee_wei = int(c.max_fee_wei)
			if needs_limit:
				fee.gas_limit = int(c.gas_limit)
		else:
			refresh.call()
		return c

## eth_call on the transaction node, so a purchase never waits on a refresh.
func _tx_call(to: String, data: String) -> String:
	return await _rpc("eth_call", [{"to": to, "data": data}, "latest"], _tx_http)

## symbol() and decimals() for a token, cached, on the transaction node.
func _tx_token_meta(token: String) -> Dictionary:
	if _meta.has(token):
		return _meta[token]
	var sym := PulseBlockzChain.decode_bytes(await _tx_call(token, _sel("symbol()"))).get_string_from_utf8()
	var dec := int(_hex_to_dec(await _tx_call(token, _sel("decimals()"))))
	_meta[token] = {"symbol": sym if sym != "" else "?", "decimals": dec}
	return _meta[token]

func _call_spec(to: String, data: String) -> Dictionary:
	return Abi.call_spec(to, data)

func _decode_string(hex: String) -> String:
	if hex == "":
		return ""
	return PulseBlockzChain.decode_bytes(hex).get_string_from_utf8()

# ---- publishing into the world ---------------------------------------------------
## Onto ReplicatedStorage.Chain as the "Wallet" attribute, written client-side, so it stays on
## this machine. Mine.client.luau watches that attribute and hands it to the town's desks; the
## "Json" attribute beside it is the town's own payload (Chain.server.luau).
func _publish(data: Dictionary) -> void:
	# Balances and history are handed over unasked, so a place that did not declare "chain" in
	# Uses.gd does not get them.
	if not USES.permits("chain"):
		return
	var json := JSON.stringify(data)
	# Quoted, not wrapped in [==[ ]==]: a token name off the chain can contain ]==] and end the string.
	var chunk := """
-- The town's Chain node, as it replicates in. Not made here: a second, local "Chain" would be
-- one nobody else is looking at.
local c = game:GetService("ReplicatedStorage"):WaitForChild("Chain", 60)
if c then c:SetAttribute("Wallet", %s) end
""" % preload("res://host/Luau.gd").quote(json)
	world.run_client_chunk("chain_publish", chunk)

# ---- JSON-RPC --------------------------------------------------------------------
## Waits for a request node to be free, then claims it. Two callers can find one idle in the
## same frame and both call request(); the second is refused, and both then await
## request_completed on that node, so one answer wakes both. Timed in seconds, not frames:
## headless frames pass in microseconds and a stalled one never passes at all.
func _take(http: HTTPRequest) -> bool:
	var key := http.get_instance_id()
	var waited := 0.0
	while _node_busy.get(key, false) and waited < 30.0:
		await get_tree().create_timer(0.05).timeout
		waited += 0.05
	if _node_busy.get(key, false):
		push_warning("Wallet: gave up waiting for a request node after 30s")
		return false
	_node_busy[key] = true
	return true

func _release(http: HTTPRequest) -> void:
	_node_busy[http.get_instance_id()] = false

## One call to the node, through Rpc.gd. Hands back the `result` string, or "" on any failure.
## The `http` argument is taken and ignored: Rpc owns the lanes, and `many` never takes the
## last one, so a single call always has somewhere to go.
func _rpc(method: String, params: Array, http: HTTPRequest = null) -> String:
	return await _chain.one(method, params)

## Many calls in one round trip, answered in the order they were asked. See Rpc.gd.
func _batch(calls: Array, url: String = "") -> Array:
	return await _chain.many(calls)

func _call(to: String, data: String) -> String:
	return await _rpc("eth_call", [{"to": to, "data": data}, "latest"])

# ---- ABI helpers -------------------------------------------------------------------
# Thin forwarders; the encoding lives in Abi.gd, away from the code that holds the key.
func _sel(sig: String) -> String:
	return Abi.selector(sig)

func _arg_addr(a: String) -> String:
	return Abi.arg_address(a)

func _arg_uint(n: int) -> String:
	return Abi.arg_uint(n)

func _hex_to_dec(hex: String) -> String:
	return Decimal.from_hex(hex)

func _format_units(dec: String, decimals: int, places: int) -> String:
	return Decimal.format(dec, decimals, places)

# ---- PulseX ---------------------------------------------------------------------------
# Pulsex.gd composes a quote, a swap or a liquidity call; the signing stays here, with the key,
# and goes through the same prompt as any other transaction.
var _pulsex                 # PulseBlockzPulsex: quotes, swaps, liquidity

func pulsex_quote(from_addr: String, to_addr: String, amount: String, slippage_bps: int = 50) -> Dictionary:
	return await _pulsex.pulsex_quote(from_addr, to_addr, amount, slippage_bps)

func pulsex_swap(from_addr: String, to_addr: String, amount: String, slippage_bps: int = 50) -> Dictionary:
	return await _pulsex.pulsex_swap(from_addr, to_addr, amount, slippage_bps)

func pulsex_positions() -> Array:
	return await _pulsex.pulsex_positions()

func pulsex_pool_quote(a_addr: String, b_addr: String, amount: String, side: String = "a") -> Dictionary:
	return await _pulsex.pulsex_pool_quote(a_addr, b_addr, amount, side)

func pulsex_add_liquidity(a_addr: String, b_addr: String, a_amount: String, slippage_bps: int = 100) -> Dictionary:
	return await _pulsex.pulsex_add_liquidity(a_addr, b_addr, a_amount, slippage_bps)

func pulsex_remove_liquidity(a_addr: String, b_addr: String, percent: int, slippage_bps: int = 100) -> Dictionary:
	return await _pulsex.pulsex_remove_liquidity(a_addr, b_addr, percent, slippage_bps)

func add_token(address: String) -> Dictionary:
	return await _pulsex.add_token(address)

func forget_token(address: String) -> Dictionary:
	return _pulsex.forget_token(address)

## The tokens this wallet knows about, for the payload it publishes.
func _pulsex_tokens() -> Array:
	return _pulsex._pulsex_tokens()

func _dec_ratio_bps(num: String, den: String) -> int:
	return Decimal.ratio_bps(num, den)



# ---- the screener ---------------------------------------------------------------------
# Prices come off DexScreener's public API, which indexes mainnet, while the swap next door
# runs on testnet: the board is labelled as another chain's market.

## "3d", "5h", "12m" from the millisecond timestamp a pool was created at. A pair minted an
## hour ago with a good-looking price is the oldest trick there is.
func _age_since(ms: float) -> String:
	if ms <= 0:
		return "-"
	var seconds := int(Time.get_unix_time_from_system() - ms / 1000.0)
	if seconds < 0:
		return "-"
	if seconds < 3600:
		return "%dm" % maxi(1, seconds / 60)
	if seconds < 86400:
		return "%dh" % (seconds / 3600)
	if seconds < 86400 * 365:
		return "%dd" % (seconds / 86400)
	return "%dy" % (seconds / (86400 * 365))



## What the wallet knows about itself and nothing about any town: whose it is, what it holds,
## which chain it is on. A place builds its own payload on top (demo2's Chain.server.luau).
func _collect() -> Dictionary:
	var addr := wallet_address
	var out := {
		"address": wallet_address, "network": ADDRESSES.network,
		"can_buy": can_buy(), "gas": "?", "gas_wei": "0", "tokens": [],
		# Told to the place so it can read its own things back without being configured twice.
		"assetstore": ADDRESSES.AssetStore,
	}
	var calls := [{"method": "eth_getBalance", "params": [addr, "latest"]}]
	for token in ADDRESSES.tokens:
		calls.append(_call_spec(token, _sel("symbol()")))
		calls.append(_call_spec(token, _sel("decimals()")))
		calls.append(_call_spec(token, _sel("balanceOf(address)") + _arg_addr(addr)))
	var r := await _batch(calls)
	if r.is_empty() or String(r[0]) == "":
		push_warning("Wallet: RPC unreachable (%s); the teller will say the line is down" % rpc_url)
		return {}
	# Both forms of every balance: the formatted one to read out, the units for anything that
	# compares against a contract's own numbers.
	out.gas_wei = _hex_to_dec(String(r[0]))
	out.gas = _format_units(out.gas_wei, 18, 4)
	for i in ADDRESSES.tokens.size():
		var token: String = ADDRESSES.tokens[i]
		var sym := _decode_string(String(r[1 + i * 3]))
		var dec := int(_hex_to_dec(String(r[2 + i * 3])))
		var held := _hex_to_dec(String(r[3 + i * 3]))
		_meta[token] = {"symbol": sym if sym != "" else "?", "decimals": dec}
		# decimals goes out too: a place spending this token has to say how much in units.
		out.tokens.append({"token": token, "symbol": _meta[token].symbol, "decimals": dec,
			"balance": _format_units(held, dec, 2), "balance_units": held})

	# A cold start publishes the address and balance early, so a place can begin its own read
	# before the pools and the history are in. It goes out past refresh()'s unchanged
	# comparison, hence the once-only guard.
	out["status"] = "ok"
	if not _published_once:
		_publish(out)

	await _collect_chain(out)
	await _collect_pulsex(out)
	await _collect_history(out)
	return out



## Compares two decimal strings; a float would quietly lose precision on a uint256.
func _dec_less_than(a: String, b: String) -> bool:
	return Decimal.less_than(a, b)

func _dec_subtract(a: String, b: String) -> String:
	return Decimal.subtract(a, b)

func _http_get(url: String) -> PackedByteArray:
	var node: HTTPRequest = _web if _web != null else _http

	# Locked on the node actually used, which is _http when there is no _web to fall back from.
	if not await _take(node):
		return PackedByteArray()

	# It can still be closing from the GET before this; wait for that too.
	for _i in 40:
		if node.get_http_client_status() == HTTPClient.STATUS_DISCONNECTED:
			break
		await get_tree().create_timer(0.05).timeout
	if node.request(url) != OK:
		push_warning("Wallet: could not start a request for %s" % url)
		_release(node)
		return PackedByteArray()
	var res: Array = await node.request_completed
	_release(node)
	return res[3] if res[1] == 200 else PackedByteArray()

func _publish_place_assets() -> void:
	var path := "res://place-assets.json"
	if not FileAccess.file_exists(path):
		return
	var manifest = JSON.parse_string(FileAccess.get_file_as_string(path))
	if typeof(manifest) != TYPE_DICTIONARY:
		printerr("[wallet] place-assets.json is not an object")
		return
	publish_place_assets(manifest)

## A place's own assets by name into ReplicatedStorage.PlaceAssets, as StringValues holding
## each pblockz:// uri. From res://place-assets.json for a place run from its own files, or
## from the manifest for one mounted from the chain (the Player calls this).
func publish_place_assets(manifest: Dictionary) -> void:
	if world == null:
		return
	world.run_chunk("place_assets_folder", """
local rs = game:GetService("ReplicatedStorage")
if not rs:FindFirstChild("PlaceAssets") then
    local f = Instance.new("Folder") f.Name = "PlaceAssets" f.Parent = rs
end
""")
	# The uri, not a cached path: a path is this machine's and names nothing in a joined client's
	# SoundId or Texture, while a pblockz:// resolves wherever it is drawn, as an rbxassetid:// does.
	for name in manifest.keys():
		if String(name).begins_with("_"):
			continue
		var uri := String(manifest[name])
		if not uri.begins_with("pblockz://"):
			printerr("[wallet] place asset %s is not a pblockz:// uri" % name)
			continue
		world.run_chunk("place_asset", """
local rs = game:GetService("ReplicatedStorage")
local folder = rs:WaitForChild("PlaceAssets")
local v = folder:FindFirstChild(%s)
if not v then v = Instance.new("StringValue") v.Name = %s v.Parent = folder end
v.Value = %s
""" % [preload("res://host/Luau.gd").quote(name), preload("res://host/Luau.gd").quote(name), preload("res://host/Luau.gd").quote(uri)])
	# Complete means the manifest was read, not that the bytes are here -- that is
	# ContentProvider:PreloadAsync's to say.
	world.run_chunk("place_assets_done", """
local rs = game:GetService("ReplicatedStorage")
rs:WaitForChild("PlaceAssets"):SetAttribute("Complete", true)
""")

## One call whose answer is an object -- a block, a receipt -- unwrapped from the JSON-RPC
## envelope. {} when there is none, which is also what an unmined transaction gives ("result": null).
func _rpc_dict(method: String, params: Array, http: HTTPRequest = null) -> Dictionary:
	var got: Dictionary = await _chain.one_dict(method, params)
	var result = got.get("result")
	return result if typeof(result) == TYPE_DICTIONARY else {}

## A prepared transaction with no prompt of its own, for a host tool that asked once over the
## whole plan (host/Publish.gd). Not reachable from a place: it is not in _handle_request's
## action allowlist. Returns {ok, hash, receipt, message}.
func send_prepared(to: String, data: String, fallback_gas: int, what: String = "") -> Dictionary:
	if _key == "":
		return {"ok": false, "message": "No key loaded, so nothing can be signed."}
	var fee := await _fee_quote(to, data, "0x0", fallback_gas)
	if String(fee.estimate_failed) != "":
		return {"ok": false, "message": "the chain expects this to fail: %s" % fee.estimate_failed}
	var sent := await _send(to, data, fallback_gas, "0x0", fee, what)
	if not sent.get("ok", false):
		return sent
	sent["receipt"] = await _rpc_dict("eth_getTransactionReceipt", [sent.hash], _tx_http)
	return sent

## Signs one transaction, sends it, and waits for its receipt. `what` names the act on the
## card: an approval and the call it approved are two sends, and a swap is both in a row.
func _send(to: String, data: String, gas: int, value: String = "0x0", fee: Dictionary = {}, what: String = "") -> Dictionary:
	var nonce_hex := await _rpc("eth_getTransactionCount", [wallet_address, "pending"], _tx_http)
	if nonce_hex == "":
		return {"ok": false, "message": "couldn't read the account nonce"}
	# What the player chose on the prompt (Gas.gd), or the Normal preset where no prompt offered
	# a choice. Never the node's own suggestion.
	var choice := fee if not fee.is_empty() else await _fee_quote("", "", "", 0)
	var tip := int(choice.get("tip_wei", GAS.FLOOR_WEI))
	var ceiling := maxi(int(choice.get("max_fee_wei", tip)), tip)
	var limit := int(choice.get("gas_limit", 0))
	# A prompt covering several transactions carries no limit: at the time it went up, the
	# approval the later calls need had not been mined and the chain would have said they
	# revert. By here it has, so ask. Too low a limit runs out of gas ON CHAIN and spends the
	# lot, so the estimate only ever raises it; nothing is refused here.
	if limit <= 0:
		var asked: Dictionary = await gas_for(to, data, value, gas)
		limit = maxi(int(asked.gas), gas)
	var tx := {
		"chain_id": ADDRESSES.chain_id, "nonce": int(nonce_hex.trim_prefix("0x").hex_to_int()),
		"max_priority_fee": _even_hex(tip), "max_fee": _even_hex(ceiling),
		"gas": limit if limit > 0 else gas, "to": to, "value": value, "data": data,
	}
	var raw := Tx.sign(tx, _key)
	if raw == "":
		return {"ok": false, "message": "signing failed"}
	var named := what if what != "" else "Transaction"
	var hash := await _rpc("eth_sendRawTransaction", [raw], _tx_http)
	if hash == "" or not hash.begins_with("0x"):
		_toast.raise_done(named, "The node rejected it. Nothing was spent.", false)
		return {"ok": false, "message": "the node rejected the transaction"}
	# One card per transaction: raised here, settled below.
	var card: Control = _toast.raise_pending(named, "Sent. Waiting for a block.", hash)
	# A receipt with status 0x0 reverted on chain, which is not the same as never being accepted.
	for attempt in RECEIPT_SECONDS:
		await get_tree().create_timer(1.0).timeout
		var receipt := await _rpc_dict("eth_getTransactionReceipt", [hash])
		if receipt.is_empty():
			continue
		if String(receipt.get("status", "0x1")) == "0x0":
			# Out of gas reads as a revert in the receipt and is not one; gasUsed within a
			# sixty-fourth of the limit is the tell.
			var used := String(receipt.get("gasUsed", "0x0")).hex_to_int()
			var ran_out := used >= int(tx.gas) - (int(tx.gas) / 64)
			_toast.settle(card, named, "Ran out of gas. The gas is spent." if ran_out
				else "Reverted. It was mined and the contract refused it; the gas is spent.", false, hash)
			return {"ok": false, "hash": hash,
				"message": "it ran out of gas on chain" if ran_out else "it reverted on chain"}
		_toast.settle(card, named, "Confirmed. It is on the chain.", true, hash)
		return {"ok": true, "message": "confirmed", "hash": hash}
	# Unconfirmed is not failed: it may still land, and calling it a failure is how the same
	# transaction gets sent twice.
	_toast.settle(card, named,
		"Still pending: no block in %d seconds. It may yet land -- open it to watch." % RECEIPT_SECONDS, false, hash)
	return {"ok": false, "message": "it hasn't confirmed in %d seconds; it may still land" % RECEIPT_SECONDS, "hash": hash}

## _send with PLS attached: PulseX's native-coin entry points take the amount as the
## transaction's value and wrap it, and there is no allowance to give on the native coin.
func _send_value(to: String, data: String, gas: int, value: String, fee: Dictionary = {}, what: String = "") -> Dictionary:
	return await _send(to, data, gas, value, fee, what)

## An integer as an even-length 0x-hex, which is what the transaction encoder reads bytes out of.
func _even_hex(v: int) -> String:
	var h := "%x" % v
	return "0x" + ("0" + h if h.length() % 2 == 1 else h)

# Cached: the base fee is the same for every write and moves by a few wei a block, so a prompt
# need not spend two round trips before it can go up.
var _base_seen := {}         # {base, rows, at}
const BASE_FRESH_MS := 20000

## The base fee, the Low / Normal / Fast tips, and for one call to `to` a gas limit from the
## chain's own estimate. An empty `to` is for several transactions in a row -- a swap and its
## approval -- each of which keeps its own limit.
func _fee_quote(to: String, data: String, value: String, fallback_gas: int) -> Dictionary:
	var base: int
	var rows := []
	if not _base_seen.is_empty() and Time.get_ticks_msec() - int(_base_seen.at) < BASE_FRESH_MS:
		base = int(_base_seen.base)
		rows = _base_seen.rows
	else:
		var block := await _rpc_dict("eth_getBlockByNumber", ["latest", false], _tx_http)
		base = String(block.get("baseFeePerGas", "0x0")).hex_to_int()
		if not GAS.PRESETS_GWEI.has(int(ADDRESSES.chain_id)):
			var hist := await _rpc_dict("eth_feeHistory", ["0x14", "latest", [10, 50, 90]], _tx_http)
			if typeof(hist.get("reward")) == TYPE_ARRAY:
				rows = hist.reward
		_base_seen = {"base": base, "rows": rows, "at": Time.get_ticks_msec()}
	var presets := GAS.presets(int(ADDRESSES.chain_id), rows)
	var tip: int = presets[GAS.DEFAULT_SPEED]
	var quote := {"base_wei": base, "presets": presets, "speed": GAS.DEFAULT_SPEED,
		"tip_wei": tip, "max_fee_wei": GAS.max_fee_for(base, tip),
		"gas_limit": 0, "estimated": false, "estimate_failed": ""}
	if to == "":
		return quote
	quote.gas_limit = fallback_gas
	var call := {"from": wallet_address, "to": to, "data": data if data != "" else "0x", "value": value if value != "" else "0x0"}
	var est: Dictionary = await _chain.one_dict("eth_estimateGas", [call])
	if est.has("result"):
		quote.gas_limit = GAS.limit_from_estimate(String(est.result).hex_to_int())
		quote.estimated = true
	elif typeof(est.get("error")) == TYPE_DICTIONARY:
		quote.estimate_failed = String((est.error as Dictionary).get("message", "it would revert"))
	return quote

## What a call needs, asked of the chain, with the headroom a prompt's estimate gets. For a
## write that follows another -- an approval, then what it approved -- where the prompt could
## not estimate: the constant beside such a call is a guess, and taking liquidity out of PulseX
## needs about 480,000 here. `refused` is the node's words for a call it says would revert, and
## nothing should be sent after one.
func gas_for(to: String, data: String, value: String, fallback: int) -> Dictionary:
	var call := {"from": wallet_address, "to": to, "data": data if data != "" else "0x",
		"value": value if value != "" else "0x0"}
	var est: Dictionary = await _chain.one_dict("eth_estimateGas", [call])
	if est.has("result"):
		return {"gas": GAS.limit_from_estimate(String(est.result).hex_to_int()), "refused": ""}
	# No answer at all is not a refusal: a node that did not reply has said nothing about whether
	# this works, so the fallback stands.
	if typeof(est.get("error")) == TYPE_DICTIONARY:
		return {"gas": fallback, "refused": String((est.error as Dictionary).get("message", "it would revert"))}
	return {"gas": fallback, "refused": ""}

## The storeAndPublish call for one chunk, which is both what is sent and what is estimated.
func _store_call(data: PackedByteArray, mime: String) -> String:
	var hash: String = PulseBlockzChain.content_hash_of(data)
	return Abi.selector("storeAndPublish(bytes32,uint32,string,bytes[])") + Abi.encode(
		["bytes32", "uint32", "string", "bytes[]"], [hash, data.size(), mime, [data]])

## Put bytes on chain. `store` is which AssetStore; empty means the client's own. Returns
## {ok, uri, hash, message}.
func _store_asset(data: PackedByteArray, mime: String, store: String = "", fee: Dictionary = {}) -> Dictionary:
	if store == "":
		store = String(ADDRESSES.AssetStore)
	# 24,575 bytes is one contract's worth of code; more would have to be chunked.
	if data.size() > 24575:
		return {"ok": false, "message": "that is too big for one chunk (%d bytes)" % data.size()}
	var hash: String = PulseBlockzChain.content_hash_of(data)
	var call := _store_call(data, mime)
	# Code deposit is 200 gas a byte, so the limit scales with what is being stored.
	var sent := await _send(store, call, 400000 + data.size() * 260, "0x0", fee, "Storing on chain")
	if not sent.ok:
		return {"ok": false, "message": sent.message}
	var receipt := await _rpc_dict("eth_getTransactionReceipt", [sent.hash], _tx_http)
	var topics := Abi.find_log(receipt, store,
		"BlobPublished(uint256,address,bytes32,uint32,string,uint256)")
	if topics.size() < 2:
		return {"ok": false, "message": "stored, but the chain did not say under what id"}
	var blob_id := int(_hex_to_dec(String(topics[1])))
	return {"ok": true, "hash": hash, "message": "stored",
		"uri": PulseBlockzChain.format_asset_uri({
			"content_hash": hash, "has_chain": true, "chain_id": ADDRESSES.chain_id,
			"store": store, "blob_id": blob_id, "mime": mime})}

func _to_units(amount: String, decimals: int) -> String:
	return Decimal.to_units(amount, decimals)

func _cache_media(uri: String) -> String:
	var p: Dictionary = PulseBlockzChain.parse_asset_uri(uri)
	if not p.get("ok", false):
		return ""
	# Godot picks its loader from the extension, so the mime decides the suffix: a .glb saved as
	# .png fails silently. A content-only uri carries no mime, so the store is asked for one.
	var kind := String(_text_or(p.get("mime"), ""))
	if kind == "" and assets != null:
		kind = await assets.mime_of(uri)
	var ext: String = {
		"image/png": "png", "image/jpeg": "jpg", "image/webp": "webp",
		"image/bmp": "bmp", "image/svg+xml": "svg",
		"model/gltf-binary": "glb", "model/gltf+json": "gltf",
		"model/obj": "obj", "text/plain": "obj",
		"font/ttf": "ttf", "font/otf": "otf", "font/woff": "woff", "font/woff2": "woff2",
		"application/x-font-ttf": "ttf", "application/font-woff": "woff",
		"audio/wav": "wav", "audio/x-wav": "wav", "audio/ogg": "ogg", "audio/mpeg": "mp3",
	}.get(kind, "png")
	var path := "user://media/%s.%s" % [String(p.content_hash).trim_prefix("0x").substr(0, 32), ext]
	if FileAccess.file_exists(path):
		return path
	var bytes: PackedByteArray = await assets.fetch(uri)
	if bytes.is_empty():
		return ""
	DirAccess.make_dir_recursive_absolute("user://media")
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		return ""
	f.store_buffer(bytes)
	f.close()
	return path

func _short(a: String) -> String:
	return a if a.length() < 12 else a.substr(0, 6) + "..." + a.substr(a.length() - 4)






func _text_or(v, fallback: String) -> String:
	if v == null:
		return fallback
	# A JSON field can be an object or a list as easily as a scalar, and String() has no
	# constructor for either.
	var ty := typeof(v)
	if ty == TYPE_DICTIONARY or ty == TYPE_ARRAY or ty == TYPE_OBJECT:
		return fallback
	# str(), not String(): there is no String(float) overload, and every number JSON parses is a float.
	var t := str(v)
	return fallback if t == "" else t



## The height, the base fee, this account's transaction count, the chain id and the store.
func _collect_chain(out: Dictionary) -> void:
	var r := await _batch([
		{"method": "eth_getTransactionCount", "params": [wallet_address, "latest"]},
		{"method": "eth_chainId", "params": []},
	])
	if r.size() != 2:
		return
	# The header, for the height and the base fee: eth_gasPrice on this node suggests thousands
	# of gwei against a base fee of a few wei.
	var head := await _rpc_dict("eth_getBlockByNumber", ["latest", false], _http)
	# The head just read serves the next prompt too (_fee_quote), where the chain has presets.
	if head.has("baseFeePerGas") and GAS.PRESETS_GWEI.has(int(ADDRESSES.chain_id)):
		_base_seen = {"base": String(head.get("baseFeePerGas", "0x0")).hex_to_int(), "rows": [], "at": Time.get_ticks_msec()}
	out["chain"] = {
		"block": _hex_to_dec(String(head.get("number", ""))),
		"base_fee": _hex_to_dec(String(head.get("baseFeePerGas", "0x0"))),   # wei: at a few wei, gwei rounds to zero
		"txs": _hex_to_dec(String(r[0])),
		"chain_id": _hex_to_dec(String(r[1])),
		"assetstore": ADDRESSES.AssetStore,
	}
## The pools as the last answered read left them, for a read that did not answer. An
## unanswered read is not a wallet holding nothing: a missing balance reads as a balance of
## nothing (Bank.server.luau sendable()), and an empty token list as a market with nothing in
## it (Trade.server.luau market()). Before any read answers, the key stays absent, which
## market() already tells apart -- it says the pools did not answer.
func _keep_pulsex(out: Dictionary) -> void:
	# Only this wallet's own: last_published outlives adopt_key, and balances carried across a
	# key change would name the address that is signed in now and hold what the last one had.
	if String(last_published.get("address", "")) != String(out.get("address", "")):
		return
	var kept = last_published.get("pulsex")
	if typeof(kept) == TYPE_DICTIONARY:
		out["pulsex"] = kept

## What each pool quotes, and the reserves behind it. The reserves take a second round, once
## the first has said which pool each pair is: a pool with nothing in it quotes anything.
func _collect_pulsex(out: Dictionary) -> void:
	var probe := "1000"                                  # PLS, as the quote size
	var wei := _to_units(probe, 18)
	# From Pulsex.gd, which owns importing, so a token this player pasted in is included. The
	# native coin is in that list with an empty address and has no WPLS pair of its own.
	var known: Array = _pulsex._pulsex_tokens() if _pulsex != null else PULSEX_TOKENS
	var pairs := []
	for t in known:
		if String(t.address) != "":
			pairs.append(t)
	var calls := []
	for t in pairs:
		# getAmountsOut(amountIn, [WPLS, token]) -- the router's own quote, fees included.
		calls.append(_call_spec(PULSEX.router, _sel("getAmountsOut(uint256,address[])")
			+ Abi.encode(["uint256", "address[]"], [wei, [PULSEX.wpls, t.address]])))
		calls.append(_call_spec(PULSEX.factory, _sel("getPair(address,address)")
			+ _arg_addr(PULSEX.wpls) + _arg_addr(t.address)))
		# What this wallet holds of it: the top-level `tokens` list covers the town's own tokens
		# and nothing else, so nothing else feeds the swap card's balance line.
		calls.append(_call_spec(String(t.address), _sel("balanceOf(address)") + _arg_addr(wallet_address)))
	var r := await _batch(calls)
	if r.size() != calls.size():
		_keep_pulsex(out)
		return
	var quotes := []
	var held := {}
	for i in pairs.size():
		var bal := String(r[i * 3 + 2])
		if bal != "":
			held[String(pairs[i].address).to_lower()] = _hex_to_dec(bal)
	for i in pairs.size():
		var t: Dictionary = pairs[i]
		var raw := String(r[i * 3])
		if raw == "":
			continue
		# A dynamic uint256[]: offset, length, then the amounts, so word 3 is what comes out the
		# far end of the path.
		var amount := _hex_to_dec("0x" + Abi.word(raw, 3))
		var pair := String(r[i * 3 + 1])
		quotes.append({
			"symbol": t.symbol,
			"out": _format_units(amount, int(t.decimals), 6),
			"pair": "0x" + Abi.word(pair, 0).substr(24) if pair != "" else "",
		})
	if quotes.is_empty():
		_keep_pulsex(out)
		return

	# token0 says which side of the reserves is which.
	var depth := []
	for q in quotes:
		if q.pair == "" or q.pair == "0x" + "0".repeat(40):
			continue
		depth.append(q)
	if not depth.is_empty():
		var pool_calls := []
		for q in depth:
			pool_calls.append(_call_spec(q.pair, _sel("getReserves()")))
			pool_calls.append(_call_spec(q.pair, _sel("token0()")))
		var pr := await _batch(pool_calls)
		for i in depth.size():
			var res := String(pr[i * 2]) if i * 2 < pr.size() else ""
			var t0 := String(pr[i * 2 + 1]) if i * 2 + 1 < pr.size() else ""
			if res == "" or t0 == "":
				continue
			var r0 := _hex_to_dec("0x" + Abi.word(res, 0))
			var r1 := _hex_to_dec("0x" + Abi.word(res, 1))
			# Whichever side is not WPLS is the token; the other is the depth in PLS.
			var first := "0x" + Abi.word(t0, 0).substr(24)
			var pls := r1 if first.to_lower() != PULSEX.wpls.to_lower() else r0
			depth[i]["liquidity"] = _format_units(pls, 18, 0)

	# The native coin's balance is the wallet's own, read before this ran.
	var listed := []
	for t in known:
		var one: Dictionary = t.duplicate()
		var units := String(out.get("gas_wei", "")) if String(t.address) == "" \
			else String(held.get(String(t.address).to_lower(), ""))
		one["balance_units"] = units
		one["balance"] = _format_units(units, int(t.decimals), 4) if units != "" else ""
		listed.append(one)
	out["pulsex"] = {
		"probe": probe, "quotes": quotes, "wpls": PULSEX.wpls,
		"router": PULSEX.router, "site": PULSEX.site,
		"chain_id": PULSEX.chain_id, "tokens": listed,
	}
func _collect_history(out: Dictionary) -> void:
	var url := "%s?module=account&action=txlist&address=%s&page=1&offset=6&sort=desc" % [explorer_api, wallet_address]
	var body := await _http_get(url)
	if body.is_empty():
		return
	var parsed = JSON.parse_string(body.get_string_from_utf8())
	if typeof(parsed) != TYPE_DICTIONARY or typeof(parsed.get("result")) != TYPE_ARRAY:
		return
	var history := []
	for row in parsed.result:
		if typeof(row) != TYPE_DICTIONARY:
			continue
		history.append({
			"hash": String(row.get("hash", "")),
			"block": String(row.get("blockNumber", "")),
			"to": String(row.get("to", "")),
			"ok": String(row.get("isError", "0")) == "0",
			"gas": String(row.get("gasUsed", "")),
			# The first four bytes of the input say which function it called.
			"selector": String(row.get("input", "0x")).substr(0, 10),
		})
	out["history"] = history
	out["explorer"] = explorer_site



## An Accessory follows exactly one limb and names it by the single Attachment inside its
## Handle. The fallback for a model whose own metadata does not say which slot it wants.
const SLOT_BY_ATTACHMENT := {
	"HatAttachment": "head",
	"BodyBackAttachment": "back",
	"NeckAttachment": "chest",
	"BodyFrontAttachment": "shirt",
	"WaistCenterAttachment": "legs",
	"LeftFootAttachment": "legs",
	"RightFootAttachment": "legs",
	"RightGripAttachment": "mainhand",
	"LeftGripAttachment": "offhand",
}

func _slot_of(node) -> String:
	if typeof(node) != TYPE_DICTIONARY:
		return ""
	if String(node.get("className", "")) == "Attachment":
		var found := String(node.get("name", ""))
		if SLOT_BY_ATTACHMENT.has(found):
			return SLOT_BY_ATTACHMENT[found]
	for child in node.get("children", []):
		var found := _slot_of(child)
		if found != "":
			return found
	return ""

## Every className in a model that is not in ASSET_CLASSES, plus a Source if one is hiding.
func _forbidden_classes(node: Variant, found: Array = []) -> Array:
	if typeof(node) == TYPE_DICTIONARY:
		var cls := String(node.get("className", ""))
		if cls != "" and not ASSET_CLASSES.has(cls) and not found.has(cls):
			found.append(cls)
		# A Source anywhere is out, whatever the class it sits on is called.
		var props = node.get("properties", {})
		if typeof(props) == TYPE_DICTIONARY and props.has("Source") and not found.has("Source"):
			found.append("a Source")
		for key in node.keys():
			_forbidden_classes(node[key], found)
	elif typeof(node) == TYPE_ARRAY:
		for v in node:
			_forbidden_classes(v, found)
	return found
