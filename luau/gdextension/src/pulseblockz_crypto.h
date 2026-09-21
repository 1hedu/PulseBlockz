// Godot binding for curation-list signature checks (../../../curation/SPEC.md).
// Curation.gd uses verify_curation_list as its default verifier when this class is present.
#pragma once
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class PulseBlockzCrypto : public Object {
    GDCLASS(PulseBlockzCrypto, Object)

public:
    /// keccak-256 of raw bytes.
    static PackedByteArray keccak256(const PackedByteArray& data);
    /// keccak-256 of a string's UTF-8, as 0x-hex.
    static String keccak256_hex(const String& text);
    /// Re-serialise JSON text the way curation.js `canonical()` does. "" on malformed input.
    static String canonical_json(const String& json_text);
    /// EIP-55 checksum form of an address; "" if not an address.
    static String checksum_address(const String& address);
    /// Recover the checksummed signer of a 32-byte digest from a 65-byte r‖s‖v signature (hex). "" on failure.
    static String recover_address(const PackedByteArray& digest, const String& signature_hex);
    /// Sign a 32-byte digest with a secret key (hex, 0x optional). Returns 65 bytes r‖s‖v as
    /// 0x-hex, v being the recovery id 0/1. "" on failure. Host-only: the Luau sandbox has no
    /// path to this class, so a script asks the host to sign and the key stays in GDScript.
    static String sign_digest(const PackedByteArray& digest, const String& secret_key_hex);
    /// A secret key written as an Ethereum keystore (Web3 Secret Storage v3) under `passphrase`:
    /// the JSON geth writes and every wallet reads, so the file carries to any machine and any
    /// platform. "" when the key is not a key or the passphrase is empty.
    static String keystore_write(const String& secret_key_hex, const String& passphrase);
    /// The key back, as 0x-hex. "" when the passphrase is wrong or the file was altered.
    static String keystore_read(const String& json, const String& passphrase);
    /// The address a keystore names, without the passphrase: whose wallet this is.
    static String keystore_address(const String& json);
    /// The checksummed address a secret key controls; "" if the key is not valid.
    static String address_from_key(const String& secret_key_hex);
    /// The EIP-712 digest for a curation list Dictionary (its `signature` is ignored). Empty on error.
    static PackedByteArray curation_list_digest(const Dictionary& list);
    /// The address that signed a curation list Dictionary, or "" if the signature is malformed.
    static String curation_list_signer(const Dictionary& list);
    /// True iff the list's signature was made by its `maintainer`. Safe to pass straight to Curation.verifier.
    static bool verify_curation_list(const Dictionary& list);

protected:
    static void _bind_methods();
};

} // namespace godot
