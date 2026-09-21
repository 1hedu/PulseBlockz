#include "eip712.h"

#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>

namespace pulseblockz::crypto {

// ---- keccak-256 -------------------------------------------------------------------
namespace {
constexpr uint64_t RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};
constexpr int ROT[24] = {1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44};
constexpr int PI[24] = {10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1};
inline uint64_t rotl(uint64_t x, int n) { return (x << n) | (x >> (64 - n)); }

void keccakF(uint64_t s[25]) {
    for (int round = 0; round < 24; round++) {
        uint64_t bc[5];
        for (int i = 0; i < 5; i++) bc[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20];
        for (int i = 0; i < 5; i++) {
            uint64_t t = bc[(i + 4) % 5] ^ rotl(bc[(i + 1) % 5], 1);
            for (int j = 0; j < 25; j += 5) s[j + i] ^= t;
        }
        uint64_t t = s[1];
        for (int i = 0; i < 24; i++) { int j = PI[i]; uint64_t tmp = s[j]; s[j] = rotl(t, ROT[i]); t = tmp; }
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; i++) bc[i] = s[j + i];
            for (int i = 0; i < 5; i++) s[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }
        s[0] ^= RC[round];
    }
}
} // namespace

Hash keccak256(const uint8_t* data, size_t len) {
    constexpr size_t rate = 136;
    uint64_t st[25] = {0};
    uint8_t block[rate];
    size_t off = 0;
    auto absorb = [&](const uint8_t* b) {
        for (size_t i = 0; i < rate / 8; i++) {
            uint64_t w = 0;
            for (int k = 0; k < 8; k++) w |= (uint64_t)b[i * 8 + k] << (8 * k);
            st[i] ^= w;
        }
        keccakF(st);
    };
    while (len - off >= rate) { absorb(data + off); off += rate; }
    size_t rem = len - off;
    std::memset(block, 0, rate);
    if (rem) std::memcpy(block, data + off, rem);
    block[rem] ^= 0x01;
    block[rate - 1] ^= 0x80;
    absorb(block);
    Hash out;
    for (size_t i = 0; i < 4; i++)
        for (int k = 0; k < 8; k++) out[i * 8 + k] = (uint8_t)(st[i] >> (8 * k));
    return out;
}

// ---- hex ------------------------------------------------------------------------------
std::string toHex(const uint8_t* data, size_t len) {
    static const char* d = "0123456789abcdef";
    std::string s = "0x";
    s.reserve(2 + len * 2);
    for (size_t i = 0; i < len; i++) { s += d[data[i] >> 4]; s += d[data[i] & 15]; }
    return s;
}

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool fromHex(const std::string& hexIn, std::vector<uint8_t>& out) {
    std::string h = hexIn;
    if (h.size() >= 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X')) h = h.substr(2);
    if (h.size() % 2) return false;
    out.clear(); out.reserve(h.size() / 2);
    for (size_t i = 0; i < h.size(); i += 2) {
        int a = hexVal(h[i]), b = hexVal(h[i + 1]);
        if (a < 0 || b < 0) return false;
        out.push_back((uint8_t)(a * 16 + b));
    }
    return true;
}

std::string checksumAddress(const std::string& hexIn) {
    std::string h = hexIn;
    if (h.size() >= 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X')) h = h.substr(2);
    if (h.size() != 40) return "";
    for (auto& c : h) { if (hexVal(c) < 0) return ""; c = (char)std::tolower((unsigned char)c); }
    Hash k = keccak256(h);
    std::string out = "0x";
    for (size_t i = 0; i < 40; i++) {
        int nib = (i % 2 == 0) ? (k[i / 2] >> 4) : (k[i / 2] & 15);
        out += (h[i] >= 'a' && nib >= 8) ? (char)std::toupper((unsigned char)h[i]) : h[i];
    }
    return out;
}

// ---- JSON canonicalisation ------------------------------------------------------------
namespace {
struct JVal {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double num = 0;
    std::string s;
    std::vector<JVal> arr;
    std::map<std::string, JVal> obj; // std::map: byte-order sorted keys, as JS sorts ASCII keys
};

struct Parser {
    const std::string& in; size_t i = 0; std::string err;
    explicit Parser(const std::string& s) : in(s) {}
    void ws() { while (i < in.size() && (in[i] == ' ' || in[i] == '\n' || in[i] == '\r' || in[i] == '\t')) i++; }
    bool fail(const char* m) { if (err.empty()) err = std::string(m) + " at " + std::to_string(i); return false; }
    static void utf8(uint32_t cp, std::string& o) {
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
        else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
    }
    bool hex4(uint32_t& v) {
        if (i + 4 > in.size()) return fail("bad \\u escape");
        v = 0;
        for (int k = 0; k < 4; k++) { int h = hexVal(in[i++]); if (h < 0) return fail("bad \\u escape"); v = v * 16 + h; }
        return true;
    }
    bool str(std::string& o) {
        if (in[i] != '"') return fail("expected string");
        i++;
        while (i < in.size()) {
            char c = in[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= in.size()) return fail("bad escape");
                char e = in[i++];
                switch (e) {
                    case '"': o += '"'; break; case '\\': o += '\\'; break; case '/': o += '/'; break;
                    case 'b': o += '\b'; break; case 'f': o += '\f'; break; case 'n': o += '\n'; break;
                    case 'r': o += '\r'; break; case 't': o += '\t'; break;
                    case 'u': {
                        uint32_t cp; if (!hex4(cp)) return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < in.size() && in[i] == '\\' && in[i + 1] == 'u') {
                            size_t save = i; i += 2; uint32_t lo;
                            if (!hex4(lo)) return false;
                            if (lo >= 0xDC00 && lo <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            else i = save;
                        }
                        utf8(cp, o); break;
                    }
                    default: return fail("bad escape");
                }
            } else if ((unsigned char)c < 0x20) return fail("control char in string");
            else o += c;
        }
        return fail("unterminated string");
    }
    bool val(JVal& v) {
        ws();
        if (i >= in.size()) return fail("unexpected end");
        char c = in[i];
        if (c == '{') {
            v.t = JVal::Obj; i++; ws();
            if (i < in.size() && in[i] == '}') { i++; return true; }
            for (;;) {
                ws(); std::string k; if (!str(k)) return false; ws();
                if (i >= in.size() || in[i] != ':') return fail("expected ':'");
                i++; JVal child; if (!val(child)) return false;
                v.obj[k] = std::move(child); // duplicate keys: last wins, as JSON.parse
                ws(); if (i >= in.size()) return fail("unexpected end");
                if (in[i] == ',') { i++; continue; }
                if (in[i] == '}') { i++; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.t = JVal::Arr; i++; ws();
            if (i < in.size() && in[i] == ']') { i++; return true; }
            for (;;) {
                JVal child; if (!val(child)) return false; v.arr.push_back(std::move(child));
                ws(); if (i >= in.size()) return fail("unexpected end");
                if (in[i] == ',') { i++; continue; }
                if (in[i] == ']') { i++; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.t = JVal::Str; return str(v.s); }
        if (in.compare(i, 4, "true") == 0) { v.t = JVal::Bool; v.b = true; i += 4; return true; }
        if (in.compare(i, 5, "false") == 0) { v.t = JVal::Bool; v.b = false; i += 5; return true; }
        if (in.compare(i, 4, "null") == 0) { v.t = JVal::Null; i += 4; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t start = i; i++;
            while (i < in.size() && (std::isdigit((unsigned char)in[i]) || in[i] == '.' || in[i] == 'e' || in[i] == 'E' || in[i] == '+' || in[i] == '-')) i++;
            v.t = JVal::Num;
            auto r = std::from_chars(in.data() + start, in.data() + i, v.num);
            if (r.ec != std::errc() || r.ptr != in.data() + i) return fail("bad number");
            return true;
        }
        return fail("unexpected character");
    }
};

void writeStr(const std::string& s, std::string& o) {
    static const char* d = "0123456789abcdef";
    o += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break; case '\\': o += "\\\\"; break;
            case '\b': o += "\\b"; break; case '\f': o += "\\f"; break; case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break; case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { o += "\\u00"; o += d[c >> 4]; o += d[c & 15]; }
                else o += (char)c;
        }
    }
    o += '"';
}

void writeNum(double n, std::string& o) {
    if (std::isfinite(n) && n == std::floor(n) && std::fabs(n) < 1e21) {
        // JS prints integral doubles below 1e21 without a fraction
        char buf[32]; auto r = std::to_chars(buf, buf + sizeof buf, (long long)n);
        o.append(buf, r.ptr);
        return;
    }
    if (!std::isfinite(n)) { o += "null"; return; } // JSON.stringify(NaN) === "null"
    char buf[64]; auto r = std::to_chars(buf, buf + sizeof buf, n); // shortest round-trip, like JS
    o.append(buf, r.ptr);
}

void write(const JVal& v, std::string& o) {
    switch (v.t) {
        case JVal::Null: o += "null"; break;
        case JVal::Bool: o += v.b ? "true" : "false"; break;
        case JVal::Num: writeNum(v.num, o); break;
        case JVal::Str: writeStr(v.s, o); break;
        case JVal::Arr: {
            o += '['; bool first = true;
            for (auto& e : v.arr) { if (!first) o += ','; first = false; write(e, o); }
            o += ']'; break;
        }
        case JVal::Obj: {
            o += '{'; bool first = true;
            for (auto& [k, e] : v.obj) { if (!first) o += ','; first = false; writeStr(k, o); o += ':'; write(e, o); }
            o += '}'; break;
        }
    }
}
} // namespace

bool canonicalJson(const std::string& jsonText, std::string& out, std::string* err) {
    Parser p(jsonText); JVal v;
    if (!p.val(v)) { if (err) *err = p.err; return false; }
    p.ws();
    if (p.i != jsonText.size()) { if (err) *err = "trailing characters"; return false; }
    out.clear(); write(v, out);
    return true;
}

// ---- EIP-712 -----------------------------------------------------------------------------
namespace {
void putU256(uint64_t v, std::string& o) { std::string w(32, '\0'); for (int k = 0; k < 8; k++) w[31 - k] = (char)(v >> (8 * k)); o += w; }
void putHash(const Hash& h, std::string& o) { o.append(reinterpret_cast<const char*>(h.data()), 32); }
} // namespace

bool entriesHash(const std::string& entriesJson, Hash& out, std::string* err) {
    std::string canon;
    if (!canonicalJson(entriesJson, canon, err)) return false;
    if (canon.empty() || canon[0] != '[') { if (err) *err = "entries must be a JSON array"; return false; }
    out = keccak256(canon);
    return true;
}

bool curationDigest(const CurationFields& f, Hash& out, std::string* err) {
    std::vector<uint8_t> reg;
    if (!fromHex(f.registry, reg) || reg.size() != 20) { if (err) *err = "registry must be a 20-byte hex address"; return false; }
    Hash eh; if (!entriesHash(f.entriesJson, eh, err)) return false;

    static const Hash typeHash = keccak256(std::string("CurationList(string mode,uint256 chainId,address registry,string name,uint64 updated,uint64 ttl,bytes32 entriesHash)"));
    std::string enc;
    putHash(typeHash, enc);
    putHash(keccak256(f.mode), enc);
    putU256(f.chainId, enc);
    enc.append(12, '\0'); enc.append(reinterpret_cast<const char*>(reg.data()), 20);
    putHash(keccak256(f.name), enc);
    putU256(f.updated, enc);
    putU256(f.ttl, enc);
    putHash(eh, enc);
    Hash structHash = keccak256(enc);

    static const Hash domainSep = [] {
        std::string d;
        putHash(keccak256(std::string("EIP712Domain(string name,string version)")), d);
        putHash(keccak256(std::string("PulseBlockzCuration")), d);
        putHash(keccak256(std::string("1")), d);
        return keccak256(d);
    }();

    std::string msg = "\x19\x01";
    putHash(domainSep, msg);
    putHash(structHash, msg);
    out = keccak256(msg);
    return true;
}

// One context for the process. Since secp256k1 0.2, CONTEXT_NONE covers signing as well
// as recovery and CONTEXT_SIGN is a deprecated no-op.
static secp256k1_context* signingContext() {
    struct CtxDel { void operator()(secp256k1_context* c) const { secp256k1_context_destroy(c); } };
    static std::unique_ptr<secp256k1_context, CtxDel> ctx(secp256k1_context_create(SECP256K1_CONTEXT_NONE));
    return ctx.get();
}

std::vector<uint8_t> signDigest(const Hash& digest, const std::vector<uint8_t>& secretKey) {
    if (secretKey.size() != 32) return {};
    secp256k1_context* ctx = signingContext();
    if (!secp256k1_ec_seckey_verify(ctx, secretKey.data())) return {};
    secp256k1_ecdsa_recoverable_signature rsig;
    // nullptr nonce function: the RFC6979 default, and low-S as EIP-2 requires
    if (!secp256k1_ecdsa_sign_recoverable(ctx, &rsig, digest.data(), secretKey.data(), nullptr, nullptr)) return {};
    std::vector<uint8_t> out(65, 0);
    int recid = 0;
    if (!secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, out.data(), &recid, &rsig)) return {};
    out[64] = (uint8_t)recid;   // 0 or 1: EIP-1559's yParity, not the legacy 27/28
    return out;
}

std::string addressFromKey(const std::vector<uint8_t>& secretKey) {
    if (secretKey.size() != 32) return "";
    secp256k1_context* ctx = signingContext();
    secp256k1_pubkey pub;
    if (!secp256k1_ec_pubkey_create(ctx, &pub, secretKey.data())) return "";
    uint8_t ser[65]; size_t len = 65;
    secp256k1_ec_pubkey_serialize(ctx, ser, &len, &pub, SECP256K1_EC_UNCOMPRESSED);
    Hash h = keccak256(ser + 1, 64);
    return checksumAddress(toHex(h.data() + 12, 20));
}

std::string recoverAddress(const Hash& digest, const std::vector<uint8_t>& sig) {
    if (sig.size() != 65) return "";
    int v = sig[64];
    if (v >= 27) v -= 27;
    if (v != 0 && v != 1) return "";
    struct CtxDel { void operator()(secp256k1_context* c) const { secp256k1_context_destroy(c); } };
    static std::unique_ptr<secp256k1_context, CtxDel> ctx(secp256k1_context_create(SECP256K1_CONTEXT_NONE));
    secp256k1_ecdsa_recoverable_signature rsig;
    if (!secp256k1_ecdsa_recoverable_signature_parse_compact(ctx.get(), &rsig, sig.data(), v)) return "";
    secp256k1_pubkey pub;
    if (!secp256k1_ecdsa_recover(ctx.get(), &pub, &rsig, digest.data())) return "";
    uint8_t ser[65]; size_t len = 65;
    secp256k1_ec_pubkey_serialize(ctx.get(), ser, &len, &pub, SECP256K1_EC_UNCOMPRESSED);
    Hash h = keccak256(ser + 1, 64);
    return checksumAddress(toHex(h.data() + 12, 20));
}

bool verifyCurationList(const CurationFields& f, const std::string& maintainer, const std::string& signatureHex,
                        std::string* signerOut, std::string* err) {
    if (signerOut) signerOut->clear();
    Hash digest; if (!curationDigest(f, digest, err)) return false;
    std::vector<uint8_t> sig;
    if (!fromHex(signatureHex, sig)) { if (err) *err = "signature is not hex"; return false; }
    std::string signer = recoverAddress(digest, sig);
    if (signerOut) *signerOut = signer;
    if (signer.empty()) { if (err) *err = "signature recovery failed"; return false; }
    std::string a = signer, b = maintainer;
    std::transform(a.begin(), a.end(), a.begin(), ::tolower);
    std::transform(b.begin(), b.end(), b.begin(), ::tolower);
    if (b.rfind("0x", 0) != 0) b = "0x" + b;
    if (a != b) { if (err) *err = "signer " + signer + " is not the maintainer"; return false; }
    return true;
}

} // namespace pulseblockz::crypto
