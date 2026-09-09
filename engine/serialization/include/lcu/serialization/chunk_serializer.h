#pragma once

#include <string>

#include "lcu/voxel/chunk.h"

namespace lcu::serialization {

// Current on-disk chunk format version (brief section 86 "World
// Version"). Bump this and add a migration path if the on-disk layout
// ever changes incompatibly - see DECISIONS.md.
constexpr u32 kChunkFormatVersion = 1;

enum class ChunkLoadResult {
    Ok,
    FileNotFound,
    CorruptHeader,       // bad magic, or the header itself is unreadable/truncated
    UnsupportedVersion,  // format_version or edge_length doesn't match this build
    CorruptData,         // decompression failed or its checksum didn't match
};

// Serializes `chunk`'s block data, zstd-compressed with its content
// checksum enabled (used for corruption detection on load), to `path`.
// Returns false (logged) on any I/O or compression failure.
//
// Brief section 45's async DIRTY -> SAVE QUEUE -> SERIALIZE -> COMPRESS
// -> WRITE pipeline isn't wired up here - this is the synchronous
// primitive such a pipeline would call. Dispatching it off the main
// thread via engine/jobs::JobSystem is integration work for when there's
// an actual save-triggering event (world save, player quit - Phase 7+),
// not invented speculatively now with nothing to trigger it.
bool save_chunk_to_file(const voxel::Chunk& chunk, const std::string& path);

// Loads a chunk previously written by save_chunk_to_file. On anything
// other than ChunkLoadResult::Ok, `out_chunk` is left completely
// untouched - brief section 87: never silently accept corrupt or
// version-mismatched data as if it were good.
ChunkLoadResult load_chunk_from_file(const std::string& path, voxel::Chunk& out_chunk);

}  // namespace lcu::serialization
