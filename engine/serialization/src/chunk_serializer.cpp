#include "lcu/serialization/chunk_serializer.h"

#include <cstdio>
#include <vector>

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

bool save_chunk_to_file(const voxel::Chunk& chunk, const std::string& path) {
    const std::vector<voxel::BlockId> flat = flatten_chunk(chunk);
    const usize uncompressed_size = flat.size() * sizeof(voxel::BlockId);

    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 9);
    // Content checksum in the frame footer - this is what turns bit-flip
    // corruption into a detectable load failure instead of silently
    // wrong block data (brief section 87).
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);

    std::vector<u8> compressed(ZSTD_compressBound(uncompressed_size));
    const usize result =
        ZSTD_compress2(cctx, compressed.data(), compressed.size(), flat.data(), uncompressed_size);
    ZSTD_freeCCtx(cctx);

    if (ZSTD_isError(result)) {
        LCU_LOG_ERROR("save_chunk_to_file: zstd compression failed for \"{}\": {}", path,
                      ZSTD_getErrorName(result));
        return false;
    }

    ChunkFileHeader header;
    header.magic = kMagic;
    header.format_version = kChunkFormatVersion;
    header.edge_length = voxel::Chunk::kEdgeLength;
    header.uncompressed_size = static_cast<u32>(uncompressed_size);
    header.compressed_size = static_cast<u32>(result);

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        LCU_LOG_ERROR("save_chunk_to_file: could not open \"{}\" for writing", path);
        return false;
    }

    const bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1 &&
                     std::fwrite(compressed.data(), 1, result, file) == result;
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

    ChunkFileHeader header;
    if (std::fread(&header, sizeof(header), 1, file) != 1 || header.magic != kMagic) {
        std::fclose(file);
        return ChunkLoadResult::CorruptHeader;
    }
    if (header.format_version != kChunkFormatVersion || header.edge_length != voxel::Chunk::kEdgeLength) {
        // A different chunk edge length is treated as an unsupported
        // version too, not an attempted resize - this build can only
        // produce/consume voxel::Chunk (16^3).
        std::fclose(file);
        return ChunkLoadResult::UnsupportedVersion;
    }

    std::vector<u8> compressed(header.compressed_size);
    const usize read = std::fread(compressed.data(), 1, compressed.size(), file);
    std::fclose(file);
    if (read != compressed.size()) {
        return ChunkLoadResult::CorruptData;
    }

    std::vector<voxel::BlockId> flat(header.uncompressed_size / sizeof(voxel::BlockId));
    const usize decompressed_size =
        ZSTD_decompress(flat.data(), header.uncompressed_size, compressed.data(), compressed.size());

    if (ZSTD_isError(decompressed_size) || decompressed_size != header.uncompressed_size) {
        LCU_LOG_WARN("load_chunk_from_file: corrupt data in \"{}\"", path);
        return ChunkLoadResult::CorruptData;
    }

    unflatten_chunk(flat, out_chunk);
    return ChunkLoadResult::Ok;
}

}  // namespace lcu::serialization
