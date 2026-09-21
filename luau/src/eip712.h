// PulseBlockz — signature verification for curation lists (see ../../curation/SPEC.md), engine-agnostic:
// keccak-256, JSON canonicalisation matching the JS reference, the CurationList EIP-712 digest,
// secp256k1 recovery on libsecp256k1 with its optional recovery module. Binding: gdextension/src/pulseblockz_crypto.*.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pulseblockz::crypto {

using Hash = std::array<uint8_t, 32>;

Hash keccak256(const uint8_t* data, size_t len);
inline Hash keccak256(const std::string& s) { return keccak256(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }

std::string toHex(const uint8_t* data, size_t len);            // "0x..."
inline std::string toHex(const Hash& h) { return toHex(h.data(), h.size()); }
bool fromHex(const std::string& hex, std::vector<uint8_t>& out); // accepts with/without 0x

/// EIP-55 checksum form of a 20-byte address given as 40 hex chars (any case, optional 0x).
std::string checksumAddress(const std::string& hex);

/// Re-serialise JSON as curation.js `canonical()` does: keys sorted, no whitespace,
/// JSON.stringify escaping, integral numbers without ".0". False on malformed input.
bool canonicalJson(const std::string& jsonText, std::string& out, std::string* err = nullptr);

struct CurationFields {
    std::string mode;        // "block" | "allow"
    uint64_t    chainId = 0;
    std::string registry;    // 0x + 40 hex
    std::string name;
    uint64_t    updated = 0;
    uint64_t    ttl = 0;
    std::string entriesJson; // the `entries` array as JSON text (any formatting)
};

/// keccak256(canonicalJson(entries)) — the `entriesHash` field of the signed struct.
bool entriesHash(const std::string& entriesJson, Hash& out, std::string* err = nullptr);

/// The EIP-712 digest a maintainer signs. Domain {name:"PulseBlockzCuration", version:"1"}.
bool curationDigest(const CurationFields& f, Hash& out, std::string* err = nullptr);

/// The checksummed signer of a 32-byte digest, from a 65-byte r‖s‖v signature
/// (v = 27/28 or 0/1). Empty on failure.
std::string recoverAddress(const Hash& digest, const std::vector<uint8_t>& signature);

/// Sign a 32-byte digest with a 32-byte secret key: 65 bytes r‖s‖v, v the recovery id
/// 0/1 (EIP-1559 yParity, not the legacy 27/28). RFC6979 nonces and low-S, so one digest
/// and key give one signature. The only place in the engine that holds a private key.
std::vector<uint8_t> signDigest(const Hash& digest, const std::vector<uint8_t>& secretKey);

/// The checksummed address a secret key controls. Empty on failure.
std::string addressFromKey(const std::vector<uint8_t>& secretKey);

/// Recovers the signer of a list and compares it with `maintainer`, case-insensitively.
/// `signerOut` gets the recovered address, or "" if recovery failed.
bool verifyCurationList(const CurationFields& f, const std::string& maintainer, const std::string& signatureHex,
                        std::string* signerOut = nullptr, std::string* err = nullptr);

} // namespace pulseblockz::crypto
