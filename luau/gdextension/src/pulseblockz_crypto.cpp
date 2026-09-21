#include "pulseblockz_crypto.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "eip712.h"
#include "keystore.h"
#include <algorithm>

using namespace godot;
namespace cr = pulseblockz::crypto;

static std::string toStd(const String& s) { return std::string(s.utf8().get_data()); }
static String fromStd(const std::string& s) { return String::utf8(s.c_str(), (int)s.size()); }

PackedByteArray PulseBlockzCrypto::keccak256(const PackedByteArray& data) {
    cr::Hash h = cr::keccak256(data.ptr(), (size_t)data.size());
    PackedByteArray out; out.resize(32);
    for (int i = 0; i < 32; i++) out[i] = h[i];
    return out;
}

String PulseBlockzCrypto::keccak256_hex(const String& text) {
    return fromStd(cr::toHex(cr::keccak256(toStd(text))));
}

String PulseBlockzCrypto::canonical_json(const String& json_text) {
    std::string out, err;
    if (!cr::canonicalJson(toStd(json_text), out, &err)) {
        UtilityFunctions::push_warning("PulseBlockzCrypto.canonical_json: ", err.c_str());
        return String();
    }
    return fromStd(out);
}

String PulseBlockzCrypto::checksum_address(const String& address) {
    return fromStd(cr::checksumAddress(toStd(address)));
}

String PulseBlockzCrypto::recover_address(const PackedByteArray& digest, const String& signature_hex) {
    if (digest.size() != 32) return String();
    cr::Hash d; for (int i = 0; i < 32; i++) d[i] = digest[i];
    std::vector<uint8_t> sig;
    if (!cr::fromHex(toStd(signature_hex), sig)) return String();
    return fromStd(cr::recoverAddress(d, sig));
}

String PulseBlockzCrypto::sign_digest(const PackedByteArray& digest, const String& secret_key_hex) {
    if (digest.size() != 32) return String();
    cr::Hash d; for (int i = 0; i < 32; i++) d[i] = digest[i];
    std::vector<uint8_t> key;
    if (!cr::fromHex(toStd(secret_key_hex), key)) return String();
    std::vector<uint8_t> sig = cr::signDigest(d, key);
    // Zero the heap copy so key material does not outlive the call.
    std::fill(key.begin(), key.end(), 0);
    if (sig.size() != 65) return String();
    return fromStd(cr::toHex(sig.data(), sig.size()));
}

String PulseBlockzCrypto::address_from_key(const String& secret_key_hex) {
    std::vector<uint8_t> key;
    if (!cr::fromHex(toStd(secret_key_hex), key)) return String();
    std::string addr = cr::addressFromKey(key);
    std::fill(key.begin(), key.end(), 0);
    return fromStd(addr);
}

// Godot's JSON gives every number as a float, hence the int64 hop on chainId/updated/ttl.
// `entries` is re-stringified and canonicalised in C++, so Godot's key order and spacing
// do not reach the digest.
static bool fieldsOf(const Dictionary& list, cr::CurationFields& f, std::string* err) {
    auto need = [&](const char* k) {
        if (!list.has(k)) { if (err) *err = std::string("missing field ") + k; return false; }
        return true;
    };
    for (const char* k : {"mode", "chainId", "registry", "name", "updated", "ttl", "entries", "maintainer", "signature"})
        if (!need(k)) return false;
    f.mode = toStd(list["mode"]);
    f.chainId = (uint64_t)(int64_t)list["chainId"];
    f.registry = toStd(list["registry"]);
    f.name = toStd(list["name"]);
    f.updated = (uint64_t)(int64_t)list["updated"];
    f.ttl = (uint64_t)(int64_t)list["ttl"];
    if (list["entries"].get_type() != Variant::ARRAY) { if (err) *err = "entries must be an array"; return false; }
    f.entriesJson = toStd(JSON::stringify(list["entries"]));
    return true;
}

PackedByteArray PulseBlockzCrypto::curation_list_digest(const Dictionary& list) {
    cr::CurationFields f; std::string err; cr::Hash h;
    if (!fieldsOf(list, f, &err) || !cr::curationDigest(f, h, &err)) {
        UtilityFunctions::push_warning("PulseBlockzCrypto.curation_list_digest: ", err.c_str());
        return PackedByteArray();
    }
    PackedByteArray out; out.resize(32);
    for (int i = 0; i < 32; i++) out[i] = h[i];
    return out;
}

String PulseBlockzCrypto::curation_list_signer(const Dictionary& list) {
    cr::CurationFields f; std::string err, signer;
    if (!fieldsOf(list, f, &err)) { UtilityFunctions::push_warning("PulseBlockzCrypto.curation_list_signer: ", err.c_str()); return String(); }
    cr::verifyCurationList(f, toStd(list["maintainer"]), toStd(list["signature"]), &signer, &err);
    return fromStd(signer);
}

bool PulseBlockzCrypto::verify_curation_list(const Dictionary& list) {
    cr::CurationFields f; std::string err;
    if (!fieldsOf(list, f, &err)) { UtilityFunctions::push_warning("PulseBlockzCrypto.verify_curation_list: ", err.c_str()); return false; }
    bool ok = cr::verifyCurationList(f, toStd(list["maintainer"]), toStd(list["signature"]), nullptr, &err);
    if (!ok) UtilityFunctions::push_warning("PulseBlockzCrypto.verify_curation_list: ", err.c_str());
    return ok;
}

// ---- the key at rest ------------------------------------------------------------------
// An Ethereum keystore, in keystore.cpp: a passphrase the player knows, PBKDF2 over it, AES over
// the key and a keccak MAC over that. Nothing here is tied to a machine or an operating system,
// so the file is the backup and moving it is copying it.
String PulseBlockzCrypto::keystore_write(const String& secret_key_hex, const String& passphrase) {
    return pulseblockz::keystore::write(secret_key_hex, passphrase);
}
String PulseBlockzCrypto::keystore_read(const String& json, const String& passphrase) {
    return pulseblockz::keystore::read(json, passphrase);
}
String PulseBlockzCrypto::keystore_address(const String& json) {
    return pulseblockz::keystore::address(json);
}

void PulseBlockzCrypto::_bind_methods() {
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("keccak256", "data"), &PulseBlockzCrypto::keccak256);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("keccak256_hex", "text"), &PulseBlockzCrypto::keccak256_hex);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("canonical_json", "json_text"), &PulseBlockzCrypto::canonical_json);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("checksum_address", "address"), &PulseBlockzCrypto::checksum_address);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("recover_address", "digest", "signature_hex"), &PulseBlockzCrypto::recover_address);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("sign_digest", "digest", "secret_key_hex"), &PulseBlockzCrypto::sign_digest);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("address_from_key", "secret_key_hex"), &PulseBlockzCrypto::address_from_key);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("keystore_write", "secret_key_hex", "passphrase"), &PulseBlockzCrypto::keystore_write);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("keystore_read", "json", "passphrase"), &PulseBlockzCrypto::keystore_read);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("keystore_address", "json"), &PulseBlockzCrypto::keystore_address);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("curation_list_digest", "list"), &PulseBlockzCrypto::curation_list_digest);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("curation_list_signer", "list"), &PulseBlockzCrypto::curation_list_signer);
    ClassDB::bind_static_method("PulseBlockzCrypto", D_METHOD("verify_curation_list", "list"), &PulseBlockzCrypto::verify_curation_list);
}
