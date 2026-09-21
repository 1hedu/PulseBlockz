#include "keystore.h"
#include "../../src/eip712.h"

#include <godot_cpp/classes/aes_context.hpp>
#include <godot_cpp/classes/crypto.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace godot;
namespace cr = pulseblockz::crypto;

namespace {

// ---- SHA-256, HMAC and PBKDF2 ---------------------------------------------------------
// Written here rather than taken from Godot's Crypto because PBKDF2 runs the hash a quarter of
// a million times, and a quarter of a million allocations to cross the binding is the whole cost.

struct Sha256 {
    uint32_t h[8];
    uint64_t len;
    uint8_t buf[64];
    size_t have;
};

const uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void sha_block(Sha256& s, const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) | p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        const uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = s.h[0], b = s.h[1], c = s.h[2], d = s.h[3], e = s.h[4], f = s.h[5], g = s.h[6], hh = s.h[7];
    for (int i = 0; i < 64; i++) {
        const uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = hh + S1 + ch + kK[i] + w[i];
        const uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s.h[0] += a; s.h[1] += b; s.h[2] += c; s.h[3] += d;
    s.h[4] += e; s.h[5] += f; s.h[6] += g; s.h[7] += hh;
}

void sha_init(Sha256& s) {
    s.h[0] = 0x6a09e667; s.h[1] = 0xbb67ae85; s.h[2] = 0x3c6ef372; s.h[3] = 0xa54ff53a;
    s.h[4] = 0x510e527f; s.h[5] = 0x9b05688c; s.h[6] = 0x1f83d9ab; s.h[7] = 0x5be0cd19;
    s.len = 0; s.have = 0;
}

void sha_update(Sha256& s, const uint8_t* p, size_t n) {
    s.len += n;
    while (n > 0) {
        const size_t take = (64 - s.have) < n ? (64 - s.have) : n;
        memcpy(s.buf + s.have, p, take);
        s.have += take; p += take; n -= take;
        if (s.have == 64) { sha_block(s, s.buf); s.have = 0; }
    }
}

void sha_final(Sha256& s, uint8_t out[32]) {
    const uint64_t bits = s.len * 8;
    uint8_t pad = 0x80;
    sha_update(s, &pad, 1);
    pad = 0;
    while (s.have != 56) sha_update(s, &pad, 1);
    uint8_t tail[8];
    for (int i = 0; i < 8; i++) tail[i] = (uint8_t)(bits >> (56 - i * 8));
    memcpy(s.buf + s.have, tail, 8);
    sha_block(s, s.buf);
    s.have = 0;
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(s.h[i] >> 24); out[i * 4 + 1] = (uint8_t)(s.h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(s.h[i] >> 8); out[i * 4 + 3] = (uint8_t)s.h[i];
    }
}

void hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[32]) {
    uint8_t k[64] = {0};
    if (key_len > 64) {
        Sha256 s; sha_init(s); sha_update(s, key, key_len); sha_final(s, k);
    } else {
        memcpy(k, key, key_len);
    }
    uint8_t inner_pad[64], outer_pad[64];
    for (int i = 0; i < 64; i++) { inner_pad[i] = k[i] ^ 0x36; outer_pad[i] = k[i] ^ 0x5c; }
    uint8_t inner[32];
    Sha256 s; sha_init(s); sha_update(s, inner_pad, 64); sha_update(s, msg, msg_len); sha_final(s, inner);
    Sha256 o; sha_init(o); sha_update(o, outer_pad, 64); sha_update(o, inner, 32); sha_final(o, out);
}

std::vector<uint8_t> pbkdf2(const std::string& pass, const std::vector<uint8_t>& salt, int iterations, size_t dklen) {
    std::vector<uint8_t> out;
    const uint8_t* p = (const uint8_t*)pass.data();
    for (uint32_t block = 1; out.size() < dklen; block++) {
        std::vector<uint8_t> first(salt);
        first.push_back((uint8_t)(block >> 24)); first.push_back((uint8_t)(block >> 16));
        first.push_back((uint8_t)(block >> 8)); first.push_back((uint8_t)block);
        uint8_t u[32], acc[32];
        hmac_sha256(p, pass.size(), first.data(), first.size(), u);
        memcpy(acc, u, 32);
        for (int i = 1; i < iterations; i++) {
            hmac_sha256(p, pass.size(), u, 32, u);
            for (int j = 0; j < 32; j++) acc[j] ^= u[j];
        }
        for (int j = 0; j < 32 && out.size() < dklen; j++) out.push_back(acc[j]);
    }
    return out;
}

// ---- AES-128-CTR ----------------------------------------------------------------------
// The counter blocks are encrypted in one pass through Godot's AES and the keystream is XORed
// over the bytes, which is the same operation either way: CTR encrypts and decrypts alike.
PackedByteArray aes128_ctr(const PackedByteArray& key16, const PackedByteArray& iv16, const PackedByteArray& data) {
    PackedByteArray out;
    if (key16.size() != 16 || iv16.size() != 16) return out;
    const int64_t blocks = (data.size() + 15) / 16;
    PackedByteArray counters;
    counters.resize(blocks * 16);
    uint8_t ctr[16];
    memcpy(ctr, iv16.ptr(), 16);
    for (int64_t b = 0; b < blocks; b++) {
        memcpy(counters.ptrw() + b * 16, ctr, 16);
        for (int i = 15; i >= 0; i--) { if (++ctr[i] != 0) break; }
    }
    Ref<AESContext> aes;
    aes.instantiate();
    if (aes->start(AESContext::MODE_ECB_ENCRYPT, key16) != OK) return out;
    const PackedByteArray stream = aes->update(counters);
    aes->finish();
    if (stream.size() < data.size()) return out;
    out.resize(data.size());
    for (int64_t i = 0; i < data.size(); i++) out.ptrw()[i] = data[i] ^ stream[i];
    return out;
}

// ---- odds and ends --------------------------------------------------------------------
std::string to_hex(const uint8_t* p, size_t n) {
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { s.push_back(d[p[i] >> 4]); s.push_back(d[p[i] & 15]); }
    return s;
}

std::vector<uint8_t> from_hex(const std::string& hex) {
    std::vector<uint8_t> out;
    size_t at = (hex.size() > 1 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) ? 2 : 0;
    if ((hex.size() - at) % 2 != 0) return out;
    for (; at + 1 < hex.size(); at += 2) {
        int hi = -1, lo = -1;
        for (int k = 0; k < 2; k++) {
            const char c = hex[at + k];
            const int v = (c >= '0' && c <= '9') ? c - '0'
                        : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                        : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
            if (v < 0) return std::vector<uint8_t>();
            (k == 0 ? hi : lo) = v;
        }
        out.push_back((uint8_t)((hi << 4) | lo));
    }
    return out;
}

PackedByteArray random_bytes(int n) {
    Ref<Crypto> c;
    c.instantiate();
    return c->generate_random_bytes(n);
}

std::string std_of(const String& s) { return std::string(s.utf8().get_data()); }
String str_of(const std::string& s) { return String::utf8(s.c_str(), (int)s.size()); }

std::vector<uint8_t> vec_of(const PackedByteArray& b) {
    return std::vector<uint8_t>(b.ptr(), b.ptr() + b.size());
}

PackedByteArray bytes_of(const std::vector<uint8_t>& v) {
    PackedByteArray b;
    b.resize((int64_t)v.size());
    if (!v.empty()) memcpy(b.ptrw(), v.data(), v.size());
    return b;
}

/// keccak-256, through the same code that hashes everything else here.
std::vector<uint8_t> keccak(const std::vector<uint8_t>& in) {
    const cr::Hash h = cr::keccak256(std::string((const char*)in.data(), in.size()));
    return std::vector<uint8_t>(h.begin(), h.end());
}

}  // namespace

namespace pulseblockz {
namespace keystore {

String write(const String& secret_key_hex, const String& passphrase, int iterations) {
    const std::vector<uint8_t> key = from_hex(std_of(secret_key_hex));
    if (key.size() != 32 || passphrase.is_empty() || iterations < 1) return String();
    const std::string address = cr::addressFromKey(key);
    if (address.empty()) return String();

    const PackedByteArray salt = random_bytes(32);
    const PackedByteArray iv = random_bytes(16);
    const std::vector<uint8_t> derived = pbkdf2(std_of(passphrase), vec_of(salt), iterations, 32);
    PackedByteArray enc_key;
    enc_key.resize(16);
    memcpy(enc_key.ptrw(), derived.data(), 16);
    const PackedByteArray ciphertext = aes128_ctr(enc_key, iv, bytes_of(key));
    if (ciphertext.size() != 32) return String();

    std::vector<uint8_t> macked(derived.begin() + 16, derived.begin() + 32);
    macked.insert(macked.end(), ciphertext.ptr(), ciphertext.ptr() + ciphertext.size());
    const std::vector<uint8_t> mac = keccak(macked);

    const PackedByteArray id = random_bytes(16);
    const std::string idhex = to_hex(id.ptr(), 16);
    const std::string uuid = idhex.substr(0, 8) + "-" + idhex.substr(8, 4) + "-" + idhex.substr(12, 4)
        + "-" + idhex.substr(16, 4) + "-" + idhex.substr(20, 12);

    std::string lower = address.substr(address.rfind("0x") == 0 ? 2 : 0);
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');

    std::string out = "{\n";
    out += "  \"version\": 3,\n";
    out += "  \"id\": \"" + uuid + "\",\n";
    out += "  \"address\": \"" + lower + "\",\n";
    out += "  \"crypto\": {\n";
    out += "    \"cipher\": \"aes-128-ctr\",\n";
    out += "    \"cipherparams\": { \"iv\": \"" + to_hex(iv.ptr(), 16) + "\" },\n";
    out += "    \"ciphertext\": \"" + to_hex(ciphertext.ptr(), ciphertext.size()) + "\",\n";
    out += "    \"kdf\": \"pbkdf2\",\n";
    out += "    \"kdfparams\": {\n";
    out += "      \"c\": " + std::to_string(iterations) + ",\n";
    out += "      \"dklen\": 32,\n";
    out += "      \"prf\": \"hmac-sha256\",\n";
    out += "      \"salt\": \"" + to_hex(salt.ptr(), 32) + "\"\n";
    out += "    },\n";
    out += "    \"mac\": \"" + to_hex(mac.data(), mac.size()) + "\"\n";
    out += "  }\n";
    out += "}\n";
    return str_of(out);
}

String read(const String& json, const String& passphrase) {
    const Variant parsed = JSON::parse_string(json);
    if (parsed.get_type() != Variant::DICTIONARY) return String();
    const Dictionary top = parsed;
    const Dictionary c = top.get("crypto", top.get("Crypto", Dictionary()));
    if (c.is_empty()) return String();
    if (String(c.get("cipher", "")) != "aes-128-ctr") return String();
    if (String(c.get("kdf", "")) != "pbkdf2") return String();
    const Dictionary kp = c.get("kdfparams", Dictionary());
    const Dictionary cp = c.get("cipherparams", Dictionary());
    const std::vector<uint8_t> salt = from_hex(std_of(String(kp.get("salt", ""))));
    const std::vector<uint8_t> iv = from_hex(std_of(String(cp.get("iv", ""))));
    const std::vector<uint8_t> ciphertext = from_hex(std_of(String(c.get("ciphertext", ""))));
    const std::vector<uint8_t> mac = from_hex(std_of(String(c.get("mac", ""))));
    const int iterations = (int)(int64_t)kp.get("c", 0);
    const int dklen = (int)(int64_t)kp.get("dklen", 32);
    if (salt.empty() || iv.size() != 16 || ciphertext.empty() || mac.size() != 32) return String();
    if (iterations < 1 || iterations > 10000000 || dklen != 32) return String();

    const std::vector<uint8_t> derived = pbkdf2(std_of(passphrase), salt, iterations, 32);
    std::vector<uint8_t> macked(derived.begin() + 16, derived.begin() + 32);
    macked.insert(macked.end(), ciphertext.begin(), ciphertext.end());
    const std::vector<uint8_t> want = keccak(macked);
    // Constant time, so a wrong passphrase tells nothing but that it was wrong.
    uint8_t diff = 0;
    for (size_t i = 0; i < 32; i++) diff |= (uint8_t)(want[i] ^ mac[i]);
    if (diff != 0) return String();

    PackedByteArray enc_key, iv_b;
    enc_key.resize(16);
    memcpy(enc_key.ptrw(), derived.data(), 16);
    iv_b = bytes_of(iv);
    const PackedByteArray key = aes128_ctr(enc_key, iv_b, bytes_of(ciphertext));
    if (key.size() != 32) return String();
    return str_of("0x" + to_hex(key.ptr(), 32));
}

String address(const String& json) {
    const Variant parsed = JSON::parse_string(json);
    if (parsed.get_type() != Variant::DICTIONARY) return String();
    const Dictionary top = parsed;
    std::string a = std_of(String(top.get("address", "")));
    if (a.empty()) return String();
    if (a.rfind("0x", 0) != 0) a = "0x" + a;
    return str_of(cr::checksumAddress(a));
}

}  // namespace keystore
}  // namespace pulseblockz
