#pragma once

#include <string>
#include <vector>

#include "lcu/voxel/chunk.h"

namespace lcu::serialization {

// Current on-disk/on-wire chunk format version (brief section 86 "World
// Version"). Bump this and add a migration path if the layout ever
// changes incompatibly - see DECISIONS.md.
constexpr u32 kChunkFormatVersion = 1;

enum class ChunkLoadResult {
    Ok,
    FileNotFound,
    CorruptHeader,       // bad magic, or the header itself is unreadable/truncated
    UnsupportedVersion,  // format_version or edge_length doesn't match this build
    CorruptData,         // decompression failed or its checksum didn't match
};

// Serializes `chunk`'s block data to zstd-compressed bytes (content
// checksum enabled, used for corruption detection on load) - the same
// header+payload layout `save_chunk_to_file` writes to disk, just as an
// in-memory buffer instead. Real second consumer beyond the file API:
// `engine/network` chunk streaming (Phase 13) sends these bytes,
// fragmented, over the network - see NETWORKING.md "Chunk network
// streaming". Returns an empty vector (logged) on compression failure.
std::vector<u8> serialize_chunk_to_bytes(const voxel::Chunk& chunk);

// Deserializes bytes produced by serialize_chunk_to_bytes (or read from
// a file `save_chunk_to_file` wrote) into `out_chunk`. On anything other
// than ChunkLoadResult::Ok, `out_chunk` is left completely untouched -
// brief section 87: never silently accept corrupt or version-mismatched
// data as if it were good.
ChunkLoadResult deserialize_chunk_from_bytes(const std::vector<u8>& bytes, voxel::Chunk& out_chunk);

// Serializes `chunk`'s block data (see serialize_chunk_to_bytes) to
// `path`. Returns false (logged) on any I/O or compression failure.
//
// Brief section 45's async DIRTY -> SAVE QUEUE -> SERIALIZE -> COMPRESS
// -> WRITE pipeline isn't wired up here - this is the synchronous
// primitive such a pipeline would call. Dispatching it off the main
// thread via engine/jobs::JobSystem is integration work for when there's
// an actual save-triggering event (world save, player quit - Phase 7+),
// not invented speculatively now with nothing to trigger it.
bool save_chunk_to_file(const voxel::Chunk& chunk, const std::string& path);

// Loads a chunk previously written by save_chunk_to_file (see
// deserialize_chunk_from_bytes for the byte-buffer equivalent).
ChunkLoadResult load_chunk_from_file(const std::string& path, voxel::Chunk& out_chunk);

}  // namespace lcu::serialization
