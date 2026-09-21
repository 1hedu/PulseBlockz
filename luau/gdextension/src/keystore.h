// The wallet key at rest: an Ethereum keystore (Web3 Secret Storage, version 3) -- the same JSON
// geth writes and every wallet reads, so a key kept here can be carried anywhere, on any
// platform. PBKDF2-SHA256 over the passphrase, AES-128-CTR over the key, a keccak MAC over the
// ciphertext; nothing bespoke and nothing bound to one machine or one operating system.
#pragma once
#include <godot_cpp/variant/string.hpp>

namespace pulseblockz {
namespace keystore {

/// The keystore JSON for a secret key (hex, 0x optional), locked with `passphrase`.
/// "" when the key is not a key, or the passphrase is empty.
godot::String write(const godot::String& secret_key_hex, const godot::String& passphrase, int iterations = 262144);

/// The secret key as 0x-hex. "" when the passphrase is wrong, the file is not a keystore, or
/// the MAC says the ciphertext was altered.
godot::String read(const godot::String& json, const godot::String& passphrase);

/// The address a keystore names, "0x"-prefixed and checksummed, without needing the passphrase:
/// what the door shows to say whose wallet is on this machine. "" when the JSON has none.
godot::String address(const godot::String& json);

}  // namespace keystore
}  // namespace pulseblockz
