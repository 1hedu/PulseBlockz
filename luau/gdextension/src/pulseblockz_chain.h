// Godot binding for pblockz:// asset URIs (../../../ASSETS.md). Stateless statics: ChainAssets.gd
// does the JSON-RPC and calls these to build calldata, decode results and check the content hash.
#pragma once
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class PulseBlockzChain : public Object {
    GDCLASS(PulseBlockzChain, Object)

public:
    /// {ok: false, error}, or the parts: content_hash, has_chain (gates chain_id/store/blob_id),
    /// manifest_tx, mime, srcs
    static Dictionary parse_asset_uri(const String& uri);
    static String format_asset_uri(const Dictionary& parts);

    static String read_calldata(int64_t blob_id);
    static String read_range_calldata(int64_t blob_id, int64_t offset, int64_t length);
    static String chunks_of_calldata(int64_t blob_id);
    static String blob_calldata(int64_t blob_id);
    static String blob_of_calldata(const String& content_hash);

    static PackedByteArray decode_bytes(const String& result_hex);      // empty on error (warning pushed)
    static PackedStringArray decode_addresses(const String& result_hex);
    static int64_t decode_uint(const String& result_hex);               // -1 on error
    /// {publisher, content_hash, size, mime, chunks}
    static Dictionary decode_blob(const String& result_hex);

    /// eth_getCode results (0x-hex, each `0x00 || data`) -> the blob bytes. Empty on error.
    static PackedByteArray assemble_chunks(const PackedStringArray& codes_hex);

    // Assets published as transaction calldata (the manifest/chunk scheme).
    /// {ok, mime, chunks} from a manifest transaction's calldata (hex or text).
    static Dictionary parse_tx_manifest(const String& calldata);
    /// Empty on error.
    static PackedByteArray parse_tx_chunk(const String& calldata);
    static bool verify(const PackedByteArray& data, const String& content_hash);
    static String content_hash_of(const PackedByteArray& data);

protected:
    static void _bind_methods();
};

} // namespace godot
