// PulseBlockz — resolving pblockz:// asset URIs against AssetStore (see ../../ASSETS.md).
// Engine-agnostic: no Godot here, and the host does the JSON-RPC / HTTP. This builds the
// calldata, decodes the results, assembles chunks and checks the content hash. Binding: gdextension/src/pulseblockz_chain.*.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pulseblockz::chain {

using Bytes = std::vector<uint8_t>;

struct AssetUri {
    std::string contentHash;   // 0x + 64 hex, lowercase
    bool        hasChain = false;
    uint64_t    chainId = 0;
    std::string store;         // AssetStore address as given
    uint64_t    blobId = 0;
    // A copy in transaction calldata, the manifest transaction listing the chunk
    // transactions: ~10x cheaper than AssetStore, but in block history, not state.
    bool        hasTx = false;
    uint64_t    txChainId = 0;
    std::string manifestTx;    // 0x + 64 hex
    std::string mime;
    std::vector<std::string> srcs;
};

/// Parse `pblockz://<hash>?chain=<id>:<store>:<blob>&mime=..&src=..`. False if malformed.
bool parseAssetUri(const std::string& uri, AssetUri& out, std::string* err = nullptr);
std::string formatAssetUri(const AssetUri& u);

// ---- calldata for eth_call { to: store, data: ... } -----------------------------------
std::string readCalldata(uint64_t blobId);        // AssetStore.read(uint256)      -> bytes
std::string readRangeCalldata(uint64_t blobId, uint64_t offset, uint64_t length);
std::string chunksOfCalldata(uint64_t blobId);    // AssetStore.chunksOf(uint256)  -> address[]
std::string blobCalldata(uint64_t blobId);        // AssetStore.blob(uint256)      -> (address,bytes32,uint32,string,address[])
std::string blobOfCalldata(const std::string& contentHash); // AssetStore.blobOf(bytes32) -> uint256

// ---- decoding eth_call results (0x-hex) --------------------------------------------------
bool decodeBytes(const std::string& hex, Bytes& out, std::string* err = nullptr);
bool decodeAddressArray(const std::string& hex, std::vector<std::string>& out, std::string* err = nullptr);
bool decodeUint(const std::string& hex, uint64_t& out, std::string* err = nullptr);

struct BlobInfo {
    std::string publisher;
    std::string contentHash;
    uint64_t    size = 0;
    std::string mime;
    std::vector<std::string> chunks;
};
bool decodeBlob(const std::string& hex, BlobInfo& out, std::string* err = nullptr);

// ---- calldata-published assets (the manifest/chunk scheme) ------------------------------
struct TxManifest {
    std::string mime;
    std::vector<std::string> chunkTxs;   // transaction hashes, in order
};
/// Parse a manifest transaction's calldata: {"mimeType":..,"chunkHashes":[..]}.
bool parseTxManifest(const std::string& json, TxManifest& out, std::string* err = nullptr);
/// Parse a chunk transaction's calldata: {"parentHash":..,"chunkData":"<base64>",..} -> the bytes.
bool parseTxChunk(const std::string& json, Bytes& out, std::string* err = nullptr);
bool base64Decode(const std::string& in, Bytes& out);
std::string base64Encode(const Bytes& in);
/// Build the calldata a manifest / chunk transaction carries (for a writer).
std::string txManifestJson(const std::string& mime, const std::vector<std::string>& chunkTxs);
std::string txChunkJson(const Bytes& chunk, const std::string& mime, const std::string& parentHash);

// ---- chunks -------------------------------------------------------------------------------
/// Concatenate chunk contracts' code (each `0x00 || data`, as eth_getCode returns) into the blob.
bool assembleChunks(const std::vector<Bytes>& codes, Bytes& out, std::string* err = nullptr);
/// Split bytes into <= 24,575-byte chunks (what a writer does before storeMany).
std::vector<Bytes> chunk(const Bytes& data);

/// keccak256(data) == contentHash (case-insensitive, with or without 0x).
bool verify(const Bytes& data, const std::string& contentHash);
std::string contentHashOf(const Bytes& data);   // 0x + 64 hex

// hex helpers shared with the binding
bool hexToBytes(const std::string& hex, Bytes& out);
std::string bytesToHex(const Bytes& b);

} // namespace pulseblockz::chain
