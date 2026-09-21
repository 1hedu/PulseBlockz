#include "chain_assets.h"
#include "eip712.h"   // keccak256, fromHex, toHex, checksumAddress

#include <algorithm>
#include <cctype>
#include <cstring>

namespace pulseblockz::chain {
namespace cr = pulseblockz::crypto;

static constexpr size_t kMaxChunk = 24'575;

// ---- hex ------------------------------------------------------------------------------------
bool hexToBytes(const std::string& hex, Bytes& out) { return cr::fromHex(hex, out); }
std::string bytesToHex(const Bytes& b) { return cr::toHex(b.data(), b.size()); }

static std::string lower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
static bool isHex64(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s) if (!std::isxdigit((unsigned char)c)) return false;
    return true;
}
static std::string urlDecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) && std::isxdigit((unsigned char)s[i + 2])) {
            o += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16); i += 2;
        } else if (s[i] == '+') o += ' ';
        else o += s[i];
    }
    return o;
}
static std::string urlEncode(const std::string& s) {
    static const char* d = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
        else { o += '%'; o += d[c >> 4]; o += d[c & 15]; }
    }
    return o;
}
static bool parseU64(const std::string& s, uint64_t& v) {
    if (s.empty() || s.size() > 20) return false;
    v = 0;
    for (char c : s) { if (!std::isdigit((unsigned char)c)) return false; uint64_t n = v * 10 + (c - '0'); if (n / 10 != v) return false; v = n; }
    return true;
}

// ---- URI ------------------------------------------------------------------------------------
// The one scheme name read or written; scripts/audit-scheme.js checks nothing on chain uses another.
const std::string kUriSchemes[] = {"pblockz://"};

bool parseAssetUri(const std::string& uri, AssetUri& out, std::string* err) {
    auto fail = [&](const char* m) { if (err) *err = m; return false; };
    std::string scheme;
    for (const std::string& s : kUriSchemes) {
        if (uri.compare(0, s.size(), s) == 0) { scheme = s; break; }
    }
    if (scheme.empty()) return fail("not a pblockz:// URI");
    size_t q = uri.find('?', scheme.size());
    std::string host = uri.substr(scheme.size(), q == std::string::npos ? std::string::npos : q - scheme.size());
    if (!isHex64(host)) return fail("host must be a 64-hex keccak256");
    out = AssetUri();
    out.contentHash = "0x" + lower(host);
    if (q == std::string::npos) return true;
    std::string query = uri.substr(q + 1);
    size_t pos = 0;
    while (pos <= query.size()) {
        size_t amp = query.find('&', pos);
        std::string kv = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        pos = (amp == std::string::npos) ? query.size() + 1 : amp + 1;
        if (kv.empty()) continue;
        size_t eq = kv.find('=');
        std::string k = kv.substr(0, eq), v = eq == std::string::npos ? "" : urlDecode(kv.substr(eq + 1));
        if (k == "chain") {
            size_t a = v.find(':'), b = v.rfind(':');
            if (a == std::string::npos || b == a) return fail("chain= must be <chainId>:<store>:<blobId>");
            if (!parseU64(v.substr(0, a), out.chainId)) return fail("bad chainId");
            out.store = v.substr(a + 1, b - a - 1);
            Bytes addr; if (!cr::fromHex(out.store, addr) || addr.size() != 20) return fail("bad store address");
            if (!parseU64(v.substr(b + 1), out.blobId) || out.blobId == 0) return fail("bad blobId");
            out.hasChain = true;
        } else if (k == "tx") {
            size_t c = v.find(':');
            if (c == std::string::npos) return fail("tx= must be <chainId>:<manifestTxHash>");
            if (!parseU64(v.substr(0, c), out.txChainId)) return fail("bad tx chainId");
            std::string h = v.substr(c + 1);
            if (h.rfind("0x", 0) == 0 || h.rfind("0X", 0) == 0) h = h.substr(2);
            if (!isHex64(h)) return fail("tx hash must be 32 bytes of hex");
            out.manifestTx = "0x" + lower(h);
            out.hasTx = true;
        } else if (k == "mime") out.mime = v;
        else if (k == "src") out.srcs.push_back(v);
        // unknown keys are ignored, so the scheme can grow
    }
    return true;
}

std::string formatAssetUri(const AssetUri& u) {
    std::string s = kUriSchemes[0] + (u.contentHash.rfind("0x", 0) == 0 ? u.contentHash.substr(2) : u.contentHash);
    std::vector<std::string> q;
    if (u.hasChain) q.push_back("chain=" + std::to_string(u.chainId) + ":" + u.store + ":" + std::to_string(u.blobId));
    if (u.hasTx) q.push_back("tx=" + std::to_string(u.txChainId) + ":" + u.manifestTx);
    if (!u.mime.empty()) q.push_back("mime=" + urlEncode(u.mime));
    for (auto& src : u.srcs) q.push_back("src=" + urlEncode(src));
    for (size_t i = 0; i < q.size(); i++) s += (i ? "&" : "?") + q[i];
    return s;
}

// ---- ABI encode -----------------------------------------------------------------------------
static std::string selector(const char* sig) { return cr::toHex(cr::keccak256(std::string(sig))).substr(0, 10); }
static std::string word(uint64_t v) {
    std::string s(64, '0');
    static const char* d = "0123456789abcdef";
    for (int i = 0; i < 16; i++) s[63 - i] = d[(v >> (4 * i)) & 15];
    return s;
}
std::string readCalldata(uint64_t blobId) { return selector("read(uint256)") + word(blobId); }
std::string readRangeCalldata(uint64_t blobId, uint64_t offset, uint64_t length) { return selector("readRange(uint256,uint256,uint256)") + word(blobId) + word(offset) + word(length); }
std::string chunksOfCalldata(uint64_t blobId) { return selector("chunksOf(uint256)") + word(blobId); }
std::string blobCalldata(uint64_t blobId) { return selector("blob(uint256)") + word(blobId); }
std::string blobOfCalldata(const std::string& contentHash) {
    std::string h = lower(contentHash); if (h.rfind("0x", 0) == 0) h = h.substr(2);
    return selector("blobOf(bytes32)") + (isHex64(h) ? h : std::string(64, '0'));
}

// ---- ABI decode -----------------------------------------------------------------------------
namespace {
struct Dec {
    Bytes b; std::string err;
    bool load(const std::string& hex) { if (!cr::fromHex(hex, b)) { err = "result is not hex"; return false; } return true; }
    bool wordAt(size_t off, uint64_t& v) {
        if (off > b.size() || b.size() - off < 32) { err = "truncated result"; return false; }
        for (size_t i = 0; i < 24; i++) if (b[off + i]) { err = "value does not fit in 64 bits"; return false; }
        v = 0; for (size_t i = 24; i < 32; i++) v = (v << 8) | b[off + i];
        return true;
    }
    bool addressAt(size_t off, std::string& a) {
        if (off > b.size() || b.size() - off < 32) { err = "truncated result"; return false; }
        a = cr::checksumAddress(cr::toHex(b.data() + off + 12, 20));
        return true;
    }
    bool hash32At(size_t off, std::string& h) {
        if (off > b.size() || b.size() - off < 32) { err = "truncated result"; return false; }
        h = cr::toHex(b.data() + off, 32); return true;
    }
    bool bytesAt(size_t off, Bytes& out) {   // off points at the length word
        uint64_t len; if (!wordAt(off, len)) return false;
        if (len > b.size() - off - 32) { err = "truncated bytes"; return false; }   // wordAt left off + 32 <= size
        out.assign(b.begin() + off + 32, b.begin() + off + 32 + len);
        return true;
    }
    bool addressArrayAt(size_t off, std::vector<std::string>& out) {
        uint64_t n; if (!wordAt(off, n)) return false;
        if (n > (1u << 20)) { err = "absurd array length"; return false; }
        out.clear();
        for (uint64_t i = 0; i < n; i++) { std::string a; if (!addressAt(off + 32 + 32 * i, a)) return false; out.push_back(a); }
        return true;
    }
};
} // namespace

bool decodeBytes(const std::string& hex, Bytes& out, std::string* err) {
    Dec d; uint64_t off;
    bool ok = d.load(hex) && d.wordAt(0, off) && d.bytesAt(off, out);
    if (!ok && err) *err = d.err;
    return ok;
}
bool decodeAddressArray(const std::string& hex, std::vector<std::string>& out, std::string* err) {
    Dec d; uint64_t off;
    bool ok = d.load(hex) && d.wordAt(0, off) && d.addressArrayAt(off, out);
    if (!ok && err) *err = d.err;
    return ok;
}
bool decodeUint(const std::string& hex, uint64_t& out, std::string* err) {
    Dec d; bool ok = d.load(hex) && d.wordAt(0, out);
    if (!ok && err) *err = d.err;
    return ok;
}
bool decodeBlob(const std::string& hex, BlobInfo& out, std::string* err) {
    Dec d; uint64_t strOff, arrOff; Bytes mime;
    bool ok = d.load(hex) && d.addressAt(0, out.publisher) && d.hash32At(32, out.contentHash) && d.wordAt(64, out.size)
           && d.wordAt(96, strOff) && d.wordAt(128, arrOff) && d.bytesAt(strOff, mime) && d.addressArrayAt(arrOff, out.chunks);
    if (ok) out.mime.assign(mime.begin(), mime.end());
    if (!ok && err) *err = d.err;
    return ok;
}

// ---- calldata-published assets --------------------------------------------------------------
static const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool base64Decode(const std::string& in, Bytes& out) {
    static int8_t rev[256];
    static bool built = false;
    if (!built) { for (int i = 0; i < 256; i++) rev[i] = -1; for (int i = 0; i < 64; i++) rev[(unsigned char)kB64[i]] = (int8_t)i; built = true; }
    out.clear();
    int acc = 0, bits = 0;
    for (unsigned char c : in) {
        if (c == '=' ) break;
        if (c == 10 || c == 13 || c == 32 || c == 9) continue;   // newline, CR, space, tab
        int8_t v = rev[c];
        if (v < 0) return false;
        acc = (acc << 6) | v; bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((uint8_t)((acc >> bits) & 0xff)); }
    }
    return true;
}

std::string base64Encode(const Bytes& in) {
    std::string o;
    o.reserve((in.size() + 2) / 3 * 4);
    for (size_t i = 0; i < in.size(); i += 3) {
        unsigned v = (unsigned)in[i] << 16;
        if (i + 1 < in.size()) v |= (unsigned)in[i + 1] << 8;
        if (i + 2 < in.size()) v |= (unsigned)in[i + 2];
        o += kB64[(v >> 18) & 63];
        o += kB64[(v >> 12) & 63];
        o += i + 1 < in.size() ? kB64[(v >> 6) & 63] : '=';
        o += i + 2 < in.size() ? kB64[v & 63] : '=';
    }
    return o;
}

// Machine-written JSON of a known shape: a targeted reader, rather than a general parser
// handed whatever a transaction carries.
static bool jsonString(const std::string& j, const std::string& key, std::string& out, size_t* endOut = nullptr, size_t from = 0) {
    std::string pat = "\"" + key + "\"";
    size_t k = j.find(pat, from);
    if (k == std::string::npos) return false;
    size_t c = j.find(':', k + pat.size());
    if (c == std::string::npos) return false;
    size_t q = j.find('"', c + 1);
    if (q == std::string::npos) return false;
    size_t e = j.find('"', q + 1);
    if (e == std::string::npos) return false;
    out = j.substr(q + 1, e - q - 1);
    if (out.find((char)92) != std::string::npos) return false;   // no escapes in these fields
    if (endOut) *endOut = e;
    return true;
}

bool parseTxManifest(const std::string& json, TxManifest& out, std::string* err) {
    out = TxManifest();
    jsonString(json, "mimeType", out.mime);
    size_t k = json.find("\"chunkHashes\"");
    if (k == std::string::npos) { if (err) *err = "no chunkHashes in the manifest"; return false; }
    size_t lb = json.find('[', k), rb = json.find(']', lb == std::string::npos ? k : lb);
    if (lb == std::string::npos || rb == std::string::npos) { if (err) *err = "malformed chunkHashes"; return false; }
    size_t p = lb;
    for (;;) {
        size_t q = json.find('"', p);
        if (q == std::string::npos || q > rb) break;
        size_t e = json.find('"', q + 1);
        if (e == std::string::npos || e > rb) break;
        std::string h = json.substr(q + 1, e - q - 1);
        if (h.rfind("0x", 0) == 0) h = h.substr(2);
        if (!isHex64(h)) { if (err) *err = "a chunk hash is not 32 bytes of hex"; return false; }
        out.chunkTxs.push_back("0x" + lower(h));
        p = e + 1;
    }
    if (out.chunkTxs.empty()) { if (err) *err = "the manifest lists no chunks"; return false; }
    return true;
}

bool parseTxChunk(const std::string& json, Bytes& out, std::string* err) {
    std::string data;
    if (!jsonString(json, "chunkData", data)) { if (err) *err = "no chunkData in the chunk"; return false; }
    if (!base64Decode(data, out)) { if (err) *err = "chunkData is not base64"; return false; }
    return true;
}

std::string txManifestJson(const std::string& mime, const std::vector<std::string>& chunkTxs) {
    std::string j = "{\"mimeType\":\"" + mime + "\",\"chunkHashes\":[";
    for (size_t i = 0; i < chunkTxs.size(); i++) { if (i) j += ','; j += "\"" + chunkTxs[i] + "\""; }
    return j + "]}";
}

std::string txChunkJson(const Bytes& chunk, const std::string& mime, const std::string& parentHash) {
    return "{\"parentHash\":" + (parentHash.empty() ? std::string("0") : "\"" + parentHash + "\"")
         + ",\"chunkData\":\"" + base64Encode(chunk) + "\",\"mimeType\":\"" + mime + "\"}";
}

// ---- chunks -----------------------------------------------------------------------------------
bool assembleChunks(const std::vector<Bytes>& codes, Bytes& out, std::string* err) {
    out.clear();
    size_t total = 0;
    for (auto& c : codes) {
        if (c.empty()) { if (err) *err = "a chunk has no code"; return false; }
        if (c[0] != 0x00) { if (err) *err = "a chunk's code does not start with STOP"; return false; }
        total += c.size() - 1;
    }
    out.reserve(total);
    for (auto& c : codes) out.insert(out.end(), c.begin() + 1, c.end());
    return true;
}

std::vector<Bytes> chunk(const Bytes& data) {
    std::vector<Bytes> out;
    for (size_t i = 0; i < data.size(); i += kMaxChunk)
        out.emplace_back(data.begin() + i, data.begin() + std::min(data.size(), i + kMaxChunk));
    return out;
}

std::string contentHashOf(const Bytes& data) { return cr::toHex(cr::keccak256(data.data(), data.size())); }

bool verify(const Bytes& data, const std::string& contentHash) {
    std::string h = lower(contentHash); if (h.rfind("0x", 0) != 0) h = "0x" + h;
    return contentHashOf(data) == h;
}

} // namespace pulseblockz::chain
