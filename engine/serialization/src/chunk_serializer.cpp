#include "lcu/serialization/chunk_serializer.h"

#include <cstdio>
#include <cstring>

#include <zstd.h>

#include "lcu/core/log.h"

namespace lcu::serialization {

namespace {

constexpr u32 kMagic = 0x4C435543u;  // "LCUC", identifies a LiveCraftUltimate chunk file

// All-u32 fields: no compiler-specific padding pragma needed, every
// field is naturally 4-byte aligned with no gaps on any common ABI.
struct ChunkFileHeader {
    u32 magic = 0;
    u32 format_version = 0;
    u32 edge_length = 0;
    u32 uncompressed_size = 0;
    u32 compressed_size = 0;
};

// Chunk storage doesn't expose its internal array directly (keeps
// ChunkStorage's layout an implementation detail) - flatten/unflatten
// via the public block_at/set_block API instead. 4096 elements for the
// default chunk size: trivial cost next to the I/O and compression this
// sits next to.
std::vector<voxel::BlockId> flatten_chunk(const voxel::Chunk& chunk) {
    std::vector<voxel::BlockId> flat;
    flat.reserve(voxel::Chunk::kVolume);
    for (u32 z = 0; z < voxel::Chunk::kEdgeLength; ++z) {
        for (u32 y = 0; y < voxel::Chunk::kEdgeLength; ++y) {
            for (u32 x = 0; x < voxel::Chunk::kEdgeLength; ++x) {
                flat.push_back(chunk.block_at(x, y, z));
            }
        }
    }
    return flat;
}

void unflatten_chunk(const std::vector<voxel::BlockId>& flat, voxel::Chunk& chunk) {
    usize i = 0;
    for (u32 z = 0; z < voxel::Chunk::kEdgeLength; ++z) {
        for (u32 y = 0; y < voxel::Chunk::kEdgeLength; ++y) {
            for (u32 x = 0; x < voxel::Chunk::kEdgeLength; ++x) {
                chunk.set_block(x, y, z, flat[i++]);
            }
        }
    }
}

}  // namespace

std::vector<u8> serialize_chunk_to_bytes(const voxel::Chunk& chunk) {
    const std::vector<voxel::BlockId> flat = flatten_chunk(chunk);
    const usize uncompressed_size = flat.size() * sizeof(voxel::BlockId);

    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 9);
    // Content checksum in the frame footer - this is what turns bit-flip
    // corruption into a detectable load failure instead of silently
    // wrong block data (brief section 87).
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);

    std::vector<u8> compressed(ZSTD_compressBound(uncompressed_size));
    const usize result = ZSTD_compress2(cctx, compressed.data(), compressed.size(), flat.data(), uncompressed_size);
    ZSTD_freeCCtx(cctx);

    if (ZSTD_isError(result)) {
        LCU_LOG_ERROR("serialize_chunk_to_bytes: zstd compression failed: {}", ZSTD_getErrorName(result));
        return {};
    }

    ChunkFileHeader header;
    header.magic = kMagic;
    header.format_version = kChunkFormatVersion;
    header.edge_length = voxel::Chunk::kEdgeLength;
    header.uncompressed_size = static_cast<u32>(uncompressed_size);
    header.compressed_size = static_cast<u32>(result);

    std::vector<u8> bytes(sizeof(header) + result);
    std::memcpy(bytes.data(), &header, sizeof(header));
    std::memcpy(bytes.data() + sizeof(header), compressed.data(), result);
    return bytes;
}

ChunkLoadResult deserialize_chunk_from_bytes(const std::vector<u8>& bytes, voxel::Chunk& out_chunk) {
    ChunkFileHeader header;
    if (bytes.size() < sizeof(header)) {
        return ChunkLoadResult::CorruptHeader;
    }
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.magic != kMagic) {
        return ChunkLoadResult::CorruptHeader;
    }
    if (header.format_version != kChunkFormatVersion || header.edge_length != voxel::Chunk::kEdgeLength) {
        // A different chunk edge length is treated as an unsupported
        // version too, not an attempted resize - this build can only
        // produce/consume voxel::Chunk (16^3).
        return ChunkLoadResult::UnsupportedVersion;
    }
    if (bytes.size() < sizeof(header) + header.compressed_size) {
        return ChunkLoadResult::CorruptData;
    }

    std::vector<voxel::BlockId> flat(header.uncompressed_size / sizeof(voxel::BlockId));
    const usize decompressed_size = ZSTD_decompress(flat.data(), header.uncompressed_size,
                                                      bytes.data() + sizeof(header), header.compressed_size);

    if (ZSTD_isError(decompressed_size) || decompressed_size != header.uncompressed_size) {
        LCU_LOG_WARN("deserialize_chunk_from_bytes: corrupt data");
        return ChunkLoadResult::CorruptData;
    }

    unflatten_chunk(flat, out_chunk);
    return ChunkLoadResult::Ok;
}

bool save_chunk_to_file(const voxel::Chunk& chunk, const std::string& path) {
    const std::vector<u8> bytes = serialize_chunk_to_bytes(chunk);
    if (bytes.empty()) {
        return false;  // serialize_chunk_to_bytes already logged the reason
    }

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        LCU_LOG_ERROR("save_chunk_to_file: could not open \"{}\" for writing", path);
        return false;
    }
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);

    if (!ok) {
        LCU_LOG_ERROR("save_chunk_to_file: write failed for \"{}\"", path);
    }
    return ok;
}

ChunkLoadResult load_chunk_from_file(const std::string& path, voxel::Chunk& out_chunk) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return ChunkLoadResult::FileNotFound;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size < 0) {
        std::fclose(file);
        return ChunkLoadResult::CorruptHeader;
    }

    std::vector<u8> bytes(static_cast<usize>(size));
    const usize read = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (read != bytes.size()) {
        return ChunkLoadResult::CorruptHeader;
    }

    return deserialize_chunk_from_bytes(bytes, out_chunk);
}

}  // namespace lcu::serialization
