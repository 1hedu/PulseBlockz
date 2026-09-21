# COPY -- do not edit. The original is luau/gdextension/host/Uses.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# What a published game declared it uses (publish-experience.js --uses), enforced at the one
# place each capability goes through: the wallet's request dispatch, the scan and the market. A
# game run off local files has no manifest, declares nothing, and is refused nothing.
#
# Declared, not read out of the scripts: a script can assemble an action name like "swap" at run
# time, so only a manifest lets a game listing or a "run this?" screen be right about what it uses.
#
# Declaring none of transact, sign or pulsex means no wallet prompt is possible at all; declaring
# one admits anything of that kind, including transactions and signatures that give tokens away,
# so past that point the gate is the wallet's prompt and Danger.gd's warnings on it.
#
# preload("res://host/Uses.gd") rather than a class_name: a project run with -s has no class cache.
extends RefCounted

## Every capability: the wallet actions it admits, and the line shown for it.
const ALL := {
	"chain": {"actions": ["read", "fetch", "who"], "says": "Reads the chain, and which address you signed in with"},
	"transact": {"actions": ["write", "store"],
		"says": "Can ask you to send ANY transaction, including ones that give away or approve your tokens"},
	"sign": {"actions": ["sign_typed"],
		"says": "Can ask you to sign messages, some of which can give away your tokens without any transaction"},
	"pulsex": {"actions": ["quote", "pool_quote", "swap", "positions", "add_token", "forget_token", "add_liquidity", "remove_liquidity"],
		"says": "Can ask you to swap tokens and move liquidity on PulseX"},
	"mirror": {"actions": [], "says": "Fetches its assets from mirrors outside the chain"},
	"scan": {"actions": [], "says": "Reads the block explorer"},
	"market": {"actions": [], "says": "Searches DexScreener"},
}

## False for a game off local files, where every capability is permitted.
static var declared := false
static var uses: PackedStringArray = []

## A manifest's list. Unknown names are dropped: what the client cannot enforce it does not show.
static func declare(list) -> void:
	declared = true
	uses = PackedStringArray()
	if typeof(list) == TYPE_ARRAY:
		for one in list:
			if ALL.has(String(one)) and not uses.has(String(one)):
				uses.append(String(one))

static func permits(capability: String) -> bool:
	return not declared or uses.has(capability)

## The capability a wallet action needs, or "" for an action no capability names.
static func for_action(action: String) -> String:
	for name in ALL:
		if action in ALL[name].actions:
			return name
	return ""

static func refusal(capability: String) -> String:
	return "This game didn't say it uses %s, so the client won't do that for it." % capability

## Whether a game can ask the player's wallet for anything at all.
static func touches_wallet(list) -> bool:
	if typeof(list) != TYPE_ARRAY and typeof(list) != TYPE_PACKED_STRING_ARRAY:
		return false
	for one in list:
		if String(one) in ["transact", "sign", "pulsex"]:
			return true
	return false

## One line per declared capability, for a listing or a confirm screen.
static func describe(list) -> PackedStringArray:
	var out := PackedStringArray()
	if typeof(list) == TYPE_ARRAY or typeof(list) == TYPE_PACKED_STRING_ARRAY:
		for one in list:
			if ALL.has(String(one)):
				out.append(String(ALL[String(one)].says))
	return out
