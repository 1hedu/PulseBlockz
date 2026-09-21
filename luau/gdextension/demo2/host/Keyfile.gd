# COPY -- do not edit. The original is luau/gdextension/host/Keyfile.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# The wallet key on disk: an Ethereum keystore (Web3 Secret Storage v3, PulseBlockzCrypto ->
# src/keystore.cpp). A passphrase the player knows locks it; nothing is bound to this machine or
# this operating system, so the file IS the backup -- copy it to the next machine and it opens
# there with the same passphrase, and any other Ethereum wallet reads it too.
#
# A file written before this is a bare hex key. It still reads, and the door asks once for a
# passphrase to put it away properly.
#
#   match Keyfile.kind(path):
#       Keyfile.NONE      nothing here
#       Keyfile.BARE      a key with no passphrase yet
#       Keyfile.KEYSTORE  Keyfile.unlock(path, passphrase)
extends RefCounted

enum { NONE, BARE, KEYSTORE }

## What is at `path`, without opening it. A file that is neither a keystore nor a key is NONE and
## says so: anything else and some other format's bytes get handed out as if they were a key.
static func kind(path: String) -> int:
	if not FileAccess.file_exists(path):
		return NONE
	var text := FileAccess.get_file_as_string(path).strip_edges()
	if text == "":
		return NONE
	if text.begins_with("{"):
		return KEYSTORE
	if _is_key(text):
		return BARE
	push_warning("Keyfile: %s is neither a keystore nor a key; it is being left alone." % path)
	return NONE

## A secp256k1 secret key as it is written down: 0x and sixty-four hex characters.
static func _is_key(text: String) -> bool:
	var body := text.trim_prefix("0x")
	return body.length() == 64 and body.is_valid_hex_number()

## The key at `path`, or "" when the passphrase is wrong or there is nothing there. A bare key
## ignores the passphrase, because it has none.
static func unlock(path: String, passphrase: String) -> String:
	match kind(path):
		NONE:
			return ""
		BARE:
			return FileAccess.get_file_as_string(path).strip_edges()   # kind() has checked its shape
		_:
			return PulseBlockzCrypto.keystore_read(FileAccess.get_file_as_string(path), passphrase)

## Whose wallet is at `path`, without the passphrase: what the door shows before it asks. "" when
## there is no keystore there (a bare key is read for its address instead).
static func address(path: String) -> String:
	match kind(path):
		NONE:
			return ""
		BARE:
			return PulseBlockzCrypto.address_from_key(FileAccess.get_file_as_string(path).strip_edges())
		_:
			return PulseBlockzCrypto.keystore_address(FileAccess.get_file_as_string(path))

## Writes the key locked with the passphrase. False when the key is not a key, the passphrase is
## empty, or the file could not be written -- callers say so rather than half keeping a key.
static func write(path: String, key: String, passphrase: String) -> bool:
	if passphrase.strip_edges() == "":
		return false
	var json := PulseBlockzCrypto.keystore_write(key, passphrase)
	if json == "":
		return false
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		return false
	f.store_string(json)
	f.close()
	return true
