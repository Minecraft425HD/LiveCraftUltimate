#include "lcu/serialization/chunk_serializer.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <zstd.h>

using lcu::voxel::BlockId;
using lcu::voxel::Chunk;
using lcu::serialization::ChunkLoadResult;
using lcu::serialization::deserialize_chunk_from_bytes;
using lcu::serialization::load_chunk_from_file;
using lcu::serialization::save_chunk_to_file;
using lcu::serialization::serialize_chunk_to_bytes;

namespace {

std::string temp_file_path(const std::string& name) {
    return (std::filesystem::temp_directory_path() / ("lcu_test_" + name)).string();
}

void write_raw_bytes(const std::string& path, const std::vector<lcu::u8>& bytes) {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    ASSERT_NE(file, nullptr);
    std::fwrite(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
}

// Header is 5 consecutive u32 fields (magic, format_version,
// edge_length, uncompressed_size, compressed_size), no padding - see
// chunk_serializer.cpp. Used to poke corruption/version-mismatch
// scenarios directly rather than needing test-only hooks in the
// serializer itself.
void patch_byte_at(const std::string& path, long offset, lcu::u8 new_value) {
    std::FILE* file = std::fopen(path.c_str(), "r+b");
    ASSERT_NE(file, nullptr);
    std::fseek(file, offset, SEEK_SET);
    std::fwrite(&new_value, 1, 1, file);
    std::fclose(file);
}

// Builds a real, standalone v1-format payload (blocks only, no state
// array - the exact pre-Phase-63 layout) so LegacyV1FileLoadsWithState
// ZeroEverywhere below exercises the real backward-compatibility path
// against a byte-for-byte real legacy file, not a patched/faked one.
// Mirrors serialize_chunk_to_bytes' own pre-Phase-63 body exactly.
std::vector<lcu::u8> build_legacy_v1_bytes(const Chunk& chunk) {
    std::vector<BlockId> flat;
    flat.reserve(Chunk::kVolume);
    for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
        for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
            for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
                flat.push_back(chunk.block_at(x, y, z));
            }
        }
    }
    const lcu::usize uncompressed_size = flat.size() * sizeof(BlockId);

    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 9);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
    std::vector<lcu::u8> compressed(ZSTD_compressBound(uncompressed_size));
    const lcu::usize result =
        ZSTD_compress2(cctx, compressed.data(), compressed.size(), flat.data(), uncompressed_size);
    ZSTD_freeCCtx(cctx);

    struct LegacyHeader {
        lcu::u32 magic = 0x4C435543u;  // "LCUC"
        lcu::u32 format_version = 1;
        lcu::u32 edge_length = Chunk::kEdgeLength;
        lcu::u32 uncompressed_size = 0;
        lcu::u32 compressed_size = 0;
    };
    LegacyHeader header;
    header.uncompressed_size = static_cast<lcu::u32>(uncompressed_size);
    header.compressed_size = static_cast<lcu::u32>(result);

    std::vector<lcu::u8> bytes(sizeof(header) + result);
    std::memcpy(bytes.data(), &header, sizeof(header));
    std::memcpy(bytes.data() + sizeof(header), compressed.data(), result);
    return bytes;
}

}  // namespace

TEST(ChunkSerializer, RoundTripPreservesAllBlocks) {
    Chunk original;
    for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
        for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
            for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
                original.set_block(x, y, z, static_cast<BlockId>((x + y * 16 + z * 256) % 500));
            }
        }
    }

    const std::string path = temp_file_path("roundtrip.chunk");
    ASSERT_TRUE(save_chunk_to_file(original, path));

    Chunk loaded;
    ASSERT_EQ(load_chunk_from_file(path, loaded), ChunkLoadResult::Ok);

    for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
        for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
            for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
                ASSERT_EQ(loaded.block_at(x, y, z), original.block_at(x, y, z))
                    << "mismatch at (" << x << "," << y << "," << z << ")";
            }
        }
    }

    std::filesystem::remove(path);
}

TEST(ChunkSerializer, RoundTripPreservesAnEmptyChunk) {
    Chunk original;  // all air
    const std::string path = temp_file_path("empty.chunk");
    ASSERT_TRUE(save_chunk_to_file(original, path));

    Chunk loaded;
    ASSERT_EQ(load_chunk_from_file(path, loaded), ChunkLoadResult::Ok);
    EXPECT_TRUE(loaded.is_empty());

    std::filesystem::remove(path);
}

TEST(ChunkSerializer, LoadNonexistentFileReturnsFileNotFound) {
    Chunk chunk;
    EXPECT_EQ(load_chunk_from_file(temp_file_path("does_not_exist.chunk"), chunk), ChunkLoadResult::FileNotFound);
}

TEST(ChunkSerializer, LoadGarbageFileReturnsCorruptHeader) {
    const std::string path = temp_file_path("garbage.chunk");
    write_raw_bytes(path, {1, 2, 3, 4, 5, 6, 7, 8});

    Chunk chunk;
    EXPECT_EQ(load_chunk_from_file(path, chunk), ChunkLoadResult::CorruptHeader);

    std::filesystem::remove(path);
}

TEST(ChunkSerializer, LoadWrongVersionReturnsUnsupportedVersion) {
    Chunk chunk;
    const std::string path = temp_file_path("wrong_version.chunk");
    ASSERT_TRUE(save_chunk_to_file(chunk, path));

    // format_version is the second u32 field (byte offset 4).
    patch_byte_at(path, 4, 0xFF);

    Chunk loaded;
    EXPECT_EQ(load_chunk_from_file(path, loaded), ChunkLoadResult::UnsupportedVersion);

    std::filesystem::remove(path);
}

TEST(ChunkSerializer, LoadCorruptedPayloadReturnsCorruptData) {
    Chunk chunk;
    chunk.set_block(1, 1, 1, 42);
    const std::string path = temp_file_path("corrupt_payload.chunk");
    ASSERT_TRUE(save_chunk_to_file(chunk, path));

    // Header is 20 bytes; corrupt a byte inside the zstd frame's own
    // magic number so decompression reliably rejects it, regardless of
    // how well this particular chunk happened to compress.
    patch_byte_at(path, 21, 0xAB);

    Chunk loaded;
    EXPECT_EQ(load_chunk_from_file(path, loaded), ChunkLoadResult::CorruptData);

    std::filesystem::remove(path);
}

TEST(ChunkSerializer, FailedLoadLeavesOutputChunkUntouched) {
    Chunk out;
    out.set_block(3, 3, 3, 111);

    const auto result = load_chunk_from_file(temp_file_path("does_not_exist_2.chunk"), out);

    EXPECT_EQ(result, ChunkLoadResult::FileNotFound);
    EXPECT_EQ(out.block_at(3, 3, 3), 111);
}

// --- In-memory byte-buffer API (Phase 13's chunk network streaming) ----

TEST(ChunkSerializer, InMemoryRoundTripPreservesAllBlocks) {
    Chunk original;
    for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
        for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
            for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
                original.set_block(x, y, z, static_cast<BlockId>((x + y * 16 + z * 256) % 500));
            }
        }
    }

    const std::vector<lcu::u8> bytes = serialize_chunk_to_bytes(original);
    ASSERT_FALSE(bytes.empty());

    Chunk loaded;
    ASSERT_EQ(deserialize_chunk_from_bytes(bytes, loaded), ChunkLoadResult::Ok);
    for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
        for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
            for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
                ASSERT_EQ(loaded.block_at(x, y, z), original.block_at(x, y, z));
            }
        }
    }
}

TEST(ChunkSerializer, InMemoryBytesMatchFileBytes) {
    // serialize_chunk_to_bytes/save_chunk_to_file must produce the exact
    // same layout - save_chunk_to_file is now a thin wrapper around the
    // byte-buffer function, and this pins that down.
    Chunk chunk;
    chunk.set_block(5, 5, 5, 7);
    const std::vector<lcu::u8> bytes = serialize_chunk_to_bytes(chunk);

    const std::string path = temp_file_path("in_memory_matches_file.chunk");
    ASSERT_TRUE(save_chunk_to_file(chunk, path));

    std::FILE* file = std::fopen(path.c_str(), "rb");
    ASSERT_NE(file, nullptr);
    std::vector<lcu::u8> file_bytes(bytes.size());
    ASSERT_EQ(std::fread(file_bytes.data(), 1, file_bytes.size(), file), file_bytes.size());
    std::fclose(file);

    EXPECT_EQ(bytes, file_bytes);
    std::filesystem::remove(path);
}

TEST(ChunkSerializer, DeserializeRejectsTruncatedBytes) {
    Chunk chunk;
    chunk.set_block(1, 1, 1, 9);
    std::vector<lcu::u8> bytes = serialize_chunk_to_bytes(chunk);
    ASSERT_GT(bytes.size(), 20u);
    bytes.resize(15);  // shorter than the 20-byte header

    Chunk loaded;
    EXPECT_EQ(deserialize_chunk_from_bytes(bytes, loaded), ChunkLoadResult::CorruptHeader);
}

TEST(ChunkSerializer, DeserializeRejectsEmptyBytes) {
    Chunk loaded;
    EXPECT_EQ(deserialize_chunk_from_bytes({}, loaded), ChunkLoadResult::CorruptHeader);
}

// --- Real block-state persistence (Phase 63) ----------------------------

TEST(ChunkSerializer, RoundTripPreservesBlockStates) {
    Chunk original;
    original.set_block_with_state(2, 3, 4, 10, 7);
    original.set_block_with_state(6, 6, 6, 20, 255);

    const std::vector<lcu::u8> bytes = serialize_chunk_to_bytes(original);
    ASSERT_FALSE(bytes.empty());

    Chunk loaded;
    ASSERT_EQ(deserialize_chunk_from_bytes(bytes, loaded), ChunkLoadResult::Ok);
    EXPECT_EQ(loaded.block_at(2, 3, 4), 10);
    EXPECT_EQ(loaded.state_at(2, 3, 4), 7);
    EXPECT_EQ(loaded.block_at(6, 6, 6), 20);
    EXPECT_EQ(loaded.state_at(6, 6, 6), 255);
    // An untouched voxel round-trips as real state 0.
    EXPECT_EQ(loaded.state_at(0, 0, 0), 0);
}

TEST(ChunkSerializer, FileRoundTripPreservesBlockStates) {
    Chunk original;
    original.set_block_with_state(5, 5, 5, 3, 42);
    const std::string path = temp_file_path("states.chunk");
    ASSERT_TRUE(save_chunk_to_file(original, path));

    Chunk loaded;
    ASSERT_EQ(load_chunk_from_file(path, loaded), ChunkLoadResult::Ok);
    EXPECT_EQ(loaded.state_at(5, 5, 5), 42);

    std::filesystem::remove(path);
}

TEST(ChunkSerializer, LegacyV1FileLoadsWithStateZeroEverywhere) {
    Chunk original;
    original.set_block(1, 1, 1, 42);
    const std::vector<lcu::u8> legacy_bytes = build_legacy_v1_bytes(original);

    Chunk loaded;
    ASSERT_EQ(deserialize_chunk_from_bytes(legacy_bytes, loaded), ChunkLoadResult::Ok);
    EXPECT_EQ(loaded.block_at(1, 1, 1), 42);
    EXPECT_EQ(loaded.state_at(1, 1, 1), 0);
    EXPECT_EQ(loaded.state_at(5, 5, 5), 0);
}

TEST(ChunkSerializer, LegacyV1FileWithWrongPayloadSizeIsCorruptData) {
    // A real v1 header, but with uncompressed_size lying about the
    // real block-only payload size - must be rejected as corrupt, not
    // silently accepted with garbage/short data.
    Chunk original;
    std::vector<lcu::u8> legacy_bytes = build_legacy_v1_bytes(original);
    // uncompressed_size is the 4th u32 field (byte offset 12).
    legacy_bytes[12] = 0xFF;

    Chunk loaded;
    EXPECT_EQ(deserialize_chunk_from_bytes(legacy_bytes, loaded), ChunkLoadResult::CorruptData);
}
