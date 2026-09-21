#include "pulseblockz_chain.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "chain_assets.h"

using namespace godot;
namespace ch = pulseblockz::chain;

static std::string toStd(const String& s) { return std::string(s.utf8().get_data()); }
static String fromStd(const std::string& s) { return String::utf8(s.c_str(), (int)s.size()); }
static PackedByteArray toPBA(const ch::Bytes& b) { PackedByteArray out; out.resize((int64_t)b.size()); if (!b.empty()) memcpy(out.ptrw(), b.data(), b.size()); return out; }
static ch::Bytes fromPBA(const PackedByteArray& a) { return ch::Bytes(a.ptr(), a.ptr() + a.size()); }
static void warn(const char* fn, const std::string& err) { UtilityFunctions::push_warning("PulseBlockzChain.", fn, ": ", err.c_str()); }

Dictionary PulseBlockzChain::parse_asset_uri(const String& uri) {
    Dictionary d; ch::AssetUri u; std::string err;
    bool ok = ch::parseAssetUri(toStd(uri), u, &err);
    d["ok"] = ok;
    if (!ok) { d["error"] = fromStd(err); return d; }
    d["content_hash"] = fromStd(u.contentHash);
    d["has_chain"] = u.hasChain;
    d["chain_id"] = (int64_t)u.chainId;
    d["store"] = fromStd(u.store);
    d["blob_id"] = (int64_t)u.blobId;
    d["has_tx"] = u.hasTx;
    d["tx_chain_id"] = (int64_t)u.txChainId;
    d["manifest_tx"] = fromStd(u.manifestTx);
    d["mime"] = fromStd(u.mime);
    PackedStringArray srcs; for (auto& s : u.srcs) srcs.push_back(fromStd(s));
    d["srcs"] = srcs;
    return d;
}

String PulseBlockzChain::format_asset_uri(const Dictionary& p) {
    ch::AssetUri u;
    u.contentHash = toStd(p.get("content_hash", ""));
    u.hasChain = (bool)p.get("has_chain", false);
    u.chainId = (uint64_t)(int64_t)p.get("chain_id", 0);
    u.store = toStd(p.get("store", ""));
    u.blobId = (uint64_t)(int64_t)p.get("blob_id", 0);
    u.hasTx = (bool)p.get("has_tx", false);
    u.txChainId = (uint64_t)(int64_t)p.get("tx_chain_id", 0);
    u.manifestTx = toStd(p.get("manifest_tx", ""));
    u.mime = toStd(p.get("mime", ""));
    PackedStringArray srcs = p.get("srcs", PackedStringArray());
    for (int i = 0; i < srcs.size(); i++) u.srcs.push_back(toStd(srcs[i]));
    return fromStd(ch::formatAssetUri(u));
}

String PulseBlockzChain::read_calldata(int64_t b) { return fromStd(ch::readCalldata((uint64_t)b)); }
String PulseBlockzChain::read_range_calldata(int64_t b, int64_t o, int64_t l) { return fromStd(ch::readRangeCalldata((uint64_t)b, (uint64_t)o, (uint64_t)l)); }
String PulseBlockzChain::chunks_of_calldata(int64_t b) { return fromStd(ch::chunksOfCalldata((uint64_t)b)); }
String PulseBlockzChain::blob_calldata(int64_t b) { return fromStd(ch::blobCalldata((uint64_t)b)); }
String PulseBlockzChain::blob_of_calldata(const String& h) { return fromStd(ch::blobOfCalldata(toStd(h))); }

PackedByteArray PulseBlockzChain::decode_bytes(const String& hex) {
    ch::Bytes out; std::string err;
    if (!ch::decodeBytes(toStd(hex), out, &err)) { warn("decode_bytes", err); return PackedByteArray(); }
    return toPBA(out);
}
PackedStringArray PulseBlockzChain::decode_addresses(const String& hex) {
    std::vector<std::string> v; std::string err; PackedStringArray out;
    if (!ch::decodeAddressArray(toStd(hex), v, &err)) { warn("decode_addresses", err); return out; }
    for (auto& a : v) out.push_back(fromStd(a));
    return out;
}
int64_t PulseBlockzChain::decode_uint(const String& hex) {
    uint64_t v; std::string err;
    if (!ch::decodeUint(toStd(hex), v, &err)) { warn("decode_uint", err); return -1; }
    return (int64_t)v;
}
Dictionary PulseBlockzChain::decode_blob(const String& hex) {
    ch::BlobInfo b; std::string err; Dictionary d;
    if (!ch::decodeBlob(toStd(hex), b, &err)) { warn("decode_blob", err); return d; }
    d["publisher"] = fromStd(b.publisher); d["content_hash"] = fromStd(b.contentHash);
    d["size"] = (int64_t)b.size; d["mime"] = fromStd(b.mime);
    PackedStringArray chunks; for (auto& c : b.chunks) chunks.push_back(fromStd(c));
    d["chunks"] = chunks;
    return d;
}

PackedByteArray PulseBlockzChain::assemble_chunks(const PackedStringArray& codes) {
    std::vector<ch::Bytes> cs; std::string err;
    for (int i = 0; i < codes.size(); i++) { ch::Bytes b; if (!ch::hexToBytes(toStd(codes[i]), b)) { warn("assemble_chunks", "chunk code is not hex"); return PackedByteArray(); } cs.push_back(std::move(b)); }
    ch::Bytes out;
    if (!ch::assembleChunks(cs, out, &err)) { warn("assemble_chunks", err); return PackedByteArray(); }
    return toPBA(out);
}
// Calldata may arrive as 0x-hex (straight from eth_getTransactionByHash) or as text.
static std::string calldataText(const String& in) {
    std::string s = toStd(in);
    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
        ch::Bytes b;
        if (ch::hexToBytes(s, b)) return std::string(b.begin(), b.end());
    }
    return s;
}

Dictionary PulseBlockzChain::parse_tx_manifest(const String& calldata) {
    ch::TxManifest m; std::string err; Dictionary d;
    if (!ch::parseTxManifest(calldataText(calldata), m, &err)) {
        warn("parse_tx_manifest", err);
        d["ok"] = false; d["error"] = fromStd(err);
        return d;
    }
    d["ok"] = true; d["mime"] = fromStd(m.mime);
    PackedStringArray chunks;
    for (auto& h : m.chunkTxs) chunks.push_back(fromStd(h));
    d["chunks"] = chunks;
    return d;
}

PackedByteArray PulseBlockzChain::parse_tx_chunk(const String& calldata) {
    ch::Bytes out; std::string err;
    if (!ch::parseTxChunk(calldataText(calldata), out, &err)) { warn("parse_tx_chunk", err); return PackedByteArray(); }
    return toPBA(out);
}

bool PulseBlockzChain::verify(const PackedByteArray& data, const String& h) { return ch::verify(fromPBA(data), toStd(h)); }
String PulseBlockzChain::content_hash_of(const PackedByteArray& data) { return fromStd(ch::contentHashOf(fromPBA(data))); }

void PulseBlockzChain::_bind_methods() {
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("parse_asset_uri", "uri"), &PulseBlockzChain::parse_asset_uri);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("format_asset_uri", "parts"), &PulseBlockzChain::format_asset_uri);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("read_calldata", "blob_id"), &PulseBlockzChain::read_calldata);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("read_range_calldata", "blob_id", "offset", "length"), &PulseBlockzChain::read_range_calldata);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("chunks_of_calldata", "blob_id"), &PulseBlockzChain::chunks_of_calldata);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("blob_calldata", "blob_id"), &PulseBlockzChain::blob_calldata);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("blob_of_calldata", "content_hash"), &PulseBlockzChain::blob_of_calldata);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("decode_bytes", "result_hex"), &PulseBlockzChain::decode_bytes);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("decode_addresses", "result_hex"), &PulseBlockzChain::decode_addresses);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("decode_uint", "result_hex"), &PulseBlockzChain::decode_uint);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("decode_blob", "result_hex"), &PulseBlockzChain::decode_blob);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("assemble_chunks", "codes_hex"), &PulseBlockzChain::assemble_chunks);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("parse_tx_manifest", "calldata"), &PulseBlockzChain::parse_tx_manifest);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("parse_tx_chunk", "calldata"), &PulseBlockzChain::parse_tx_chunk);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("verify", "data", "content_hash"), &PulseBlockzChain::verify);
    ClassDB::bind_static_method("PulseBlockzChain", D_METHOD("content_hash_of", "data"), &PulseBlockzChain::content_hash_of);
}
