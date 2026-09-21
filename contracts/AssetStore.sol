// SPDX-License-Identifier: LicenseRef-PulseBlockz
pragma solidity ^0.8.24;

/// @title AssetStore — asset bytes on chain, forever, readable by anyone for free.
/// @notice Nobody owns this contract. Anyone stores bytes; nothing can be changed
///         or removed afterwards.
///
///         Bytes live in contract *code* (the SSTORE2 pattern), not in events or
///         calldata: code is state that every node keeps as long as the chain
///         exists, while history can be pruned. Each chunk is its own contract of
///         at most 24,575 bytes (EIP-170 minus the leading STOP byte that stops
///         anyone executing the data). A blob is an ordered list of chunks plus the
///         keccak-256 of the whole, which `publish` recomputes from the chunks on
///         chain, so a blob's hash is guaranteed, not claimed.
///
///         Reads are `eth_call`s: `read(blobId)` hands back the whole thing in one
///         call; `chunksOf` + `eth_getCode` per chunk works for clients that prefer
///         it. A client verifies keccak(data) == contentHash either way, so any
///         mirror can serve the same bytes and be trusted.
///
///         Asset URIs point here as  pblockz://<contentHash>?chain=<chainId>:<this>:<blobId>
///         (see ASSETS.md). The 1155 registry never changes: its `uri` string is
///         one of these.
contract AssetStore {
    uint256 public constant MAX_CHUNK = 24_575;         // EIP-170 (24,576) minus the STOP prefix
    uint256 public constant MAX_BLOB = 64 * 1024 * 1024; // sanity cap on a single blob

    struct Blob {
        address publisher;
        bytes32 contentHash;   // keccak256 of the concatenated chunk data
        uint32  size;
        string  mime;          // e.g. "model/gltf-binary", "application/x-rbxm", "application/json"
        address[] chunks;
    }

    Blob[] private _blobs;                      // blobId = index + 1
    mapping(bytes32 => uint256) public blobOf;   // contentHash -> first blobId with it (0 = none)

    event ChunkStored(address indexed chunk, uint32 size);
    event BlobPublished(uint256 indexed blobId, address indexed publisher, bytes32 indexed contentHash, uint32 size, string mime, uint256 chunkCount);

    error ChunkTooLarge();
    error ChunkEmpty();
    error DeployFailed();
    error NotAChunk(address chunk);
    error HashMismatch(bytes32 computed);
    error SizeMismatch(uint256 computed);
    error TooLarge();
    error NoChunks();
    error UnknownBlob();

    // ---- chunks ----------------------------------------------------------------
    /// @notice Store up to MAX_CHUNK bytes as a new contract's code. Returns its address.
    function store(bytes calldata data) public returns (address chunk) {
        if (data.length == 0) revert ChunkEmpty();
        if (data.length > MAX_CHUNK) revert ChunkTooLarge();
        // Init code: copy everything after the 11-byte header to memory and return it.
        //   60 0B  PUSH1 0x0B         (header length)
        //   59     MSIZE
        //   81     DUP2
        //   38     CODESIZE
        //   03     SUB               (codesize - 11 = runtime length)
        //   80     DUP1
        //   92     SWAP3
        //   59     MSIZE
        //   39     CODECOPY          (mem[0..len] = code[11..])
        //   F3     RETURN
        // Runtime = 0x00 (STOP) || data, so calling the chunk does nothing.
        bytes memory init = abi.encodePacked(hex"600B5981380380925939F3", hex"00", data);
        assembly {
            chunk := create(0, add(init, 0x20), mload(init))
        }
        if (chunk == address(0)) revert DeployFailed();
        emit ChunkStored(chunk, uint32(data.length));
    }

    function storeMany(bytes[] calldata data) external returns (address[] memory chunks) {
        chunks = new address[](data.length);
        for (uint256 i = 0; i < data.length; i++) chunks[i] = store(data[i]);
    }

    // ---- blobs -----------------------------------------------------------------
    /// @notice Register an ordered list of chunks as one blob. The hash and size are
    ///         recomputed from the chunks' code here, so they cannot be wrong.
    function publish(bytes32 contentHash, uint32 size, string calldata mime, address[] calldata chunks)
        external returns (uint256 blobId)
    {
        return _publish(contentHash, size, mime, chunks);
    }

    /// @notice Store the chunks and publish the blob in one transaction (small assets).
    function storeAndPublish(bytes32 contentHash, uint32 size, string calldata mime, bytes[] calldata data)
        external returns (uint256 blobId, address[] memory chunks)
    {
        chunks = new address[](data.length);
        for (uint256 i = 0; i < data.length; i++) chunks[i] = store(data[i]);
        blobId = _publish(contentHash, size, mime, chunks);
    }

    function _publish(bytes32 contentHash, uint32 size, string memory mime, address[] memory chunks) internal returns (uint256 blobId) {
        if (chunks.length == 0) revert NoChunks();
        bytes memory all = _assemble(chunks);
        if (all.length != size) revert SizeMismatch(all.length);
        bytes32 h = keccak256(all);
        if (h != contentHash) revert HashMismatch(h);

        _blobs.push(Blob({publisher: msg.sender, contentHash: contentHash, size: size, mime: mime, chunks: chunks}));
        blobId = _blobs.length;
        if (blobOf[contentHash] == 0) blobOf[contentHash] = blobId;
        emit BlobPublished(blobId, msg.sender, contentHash, size, mime, chunks.length);
    }

    // ---- views -----------------------------------------------------------------
    function blobCount() external view returns (uint256) { return _blobs.length; }

    function blob(uint256 blobId) external view returns (address publisher, bytes32 contentHash, uint32 size, string memory mime, address[] memory chunks) {
        Blob storage b = _get(blobId);
        return (b.publisher, b.contentHash, b.size, b.mime, b.chunks);
    }

    function chunksOf(uint256 blobId) external view returns (address[] memory) { return _get(blobId).chunks; }

    /// @notice The whole blob, assembled from its chunks. One eth_call.
    function read(uint256 blobId) external view returns (bytes memory) { return _assemble(_get(blobId).chunks); }

    /// @notice A window of the blob, for clients that stream large assets.
    function readRange(uint256 blobId, uint256 offset, uint256 length) external view returns (bytes memory out) {
        bytes memory all = _assemble(_get(blobId).chunks);
        if (offset >= all.length) return out;
        if (offset + length > all.length) length = all.length - offset;
        out = new bytes(length);
        assembly {
            let src := add(add(all, 0x20), offset)
            let dst := add(out, 0x20)
            for { let i := 0 } lt(i, length) { i := add(i, 0x20) } { mstore(add(dst, i), mload(add(src, i))) }
        }
    }

    /// @notice The data in one chunk contract (its code minus the STOP byte).
    function chunkData(address chunk) public view returns (bytes memory out) {
        uint256 len = chunk.code.length;
        if (len == 0) revert NotAChunk(chunk);
        out = new bytes(len - 1);
        assembly { extcodecopy(chunk, add(out, 0x20), 1, sub(len, 1)) }
    }

    // ---- internals -------------------------------------------------------------
    function _get(uint256 blobId) internal view returns (Blob storage) {
        if (blobId == 0 || blobId > _blobs.length) revert UnknownBlob();
        return _blobs[blobId - 1];
    }

    function _assemble(address[] memory chunks) internal view returns (bytes memory all) {
        uint256 total = 0;
        for (uint256 i = 0; i < chunks.length; i++) {
            uint256 len = chunks[i].code.length;
            if (len == 0) revert NotAChunk(chunks[i]);
            total += len - 1;
        }
        if (total > MAX_BLOB) revert TooLarge();
        all = new bytes(total);
        uint256 off = 0;
        for (uint256 i = 0; i < chunks.length; i++) {
            address c = chunks[i];
            uint256 len = c.code.length - 1;
            assembly { extcodecopy(c, add(add(all, 0x20), off), 1, len) }
            off += len;
        }
    }
}
