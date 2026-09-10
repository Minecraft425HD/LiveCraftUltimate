#include "lcu/serialization/chunk_serializer.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using lcu::voxel::BlockId;
using lcu::voxel::Chunk;
using lcu::serialization::ChunkLoadResult;
using lcu::serialization::load_chunk_from_file;
using lcu::serialization::save_chunk_to_file;

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
