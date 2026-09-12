#include "lcu/world/world.h"

#include <vector>

#include "lcu/core/assert.h"

namespace lcu::world {

namespace {

i32 abs_i32(i32 v) { return v < 0 ? -v : v; }

u32 chebyshev_distance(const voxel::ChunkCoord& a, const voxel::ChunkCoord& b) {
    const i32 dx = abs_i32(a.x - b.x);
    const i32 dy = abs_i32(a.y - b.y);
    const i32 dz = abs_i32(a.z - b.z);
    i32 max_axis = dx;
    if (dy > max_axis) {
        max_axis = dy;
    }
    if (dz > max_axis) {
        max_axis = dz;
    }
    return static_cast<u32>(max_axis);
}

}  // namespace

World::World(u32 seed, ChunkGenerator generator) : seed_(seed), generator_(std::move(generator)) {
    LCU_ASSERT(generator_ != nullptr);
}

ChunkLifecycleState World::state_of(voxel::ChunkCoord coord) const {
    const auto it = chunks_.find(coord);
    if (it == chunks_.end()) {
        return ChunkLifecycleState::Unloaded;
    }
    return it->second.state;
}

void World::request_chunk(voxel::ChunkCoord coord) {
    if (chunks_.find(coord) != chunks_.end()) {
        return;
    }
    ChunkEntry entry;
    entry.state = ChunkLifecycleState::Requested;
    chunks_.emplace(coord, std::move(entry));
}

void World::generate_chunk(voxel::ChunkCoord coord) {
    const auto it = chunks_.find(coord);
    LCU_ASSERT(it != chunks_.end());
    LCU_ASSERT(it->second.state == ChunkLifecycleState::Requested);

    it->second.state = ChunkLifecycleState::Generating;
    generator_(it->second.chunk, coord);
    it->second.state = ChunkLifecycleState::Generated;
}

void World::load_chunk(voxel::ChunkCoord coord) {
    request_chunk(coord);
    if (state_of(coord) == ChunkLifecycleState::Requested) {
        generate_chunk(coord);
    }
}

void World::adopt_generated_chunk(voxel::ChunkCoord coord, voxel::Chunk chunk) {
    if (chunks_.find(coord) != chunks_.end()) {
        return;
    }
    ChunkEntry entry;
    entry.state = ChunkLifecycleState::Generated;
    entry.chunk = std::move(chunk);
    chunks_.emplace(coord, std::move(entry));
}

const voxel::Chunk* World::chunk_at(voxel::ChunkCoord coord) const {
    const auto it = chunks_.find(coord);
    if (it == chunks_.end() || it->second.state < ChunkLifecycleState::Generated) {
        return nullptr;
    }
    return &it->second.chunk;
}

voxel::Chunk* World::chunk_at_mutable(voxel::ChunkCoord coord) {
    const auto it = chunks_.find(coord);
    if (it == chunks_.end() || it->second.state < ChunkLifecycleState::Generated) {
        return nullptr;
    }
    return &it->second.chunk;
}

void World::unload_chunk(voxel::ChunkCoord coord) { chunks_.erase(coord); }

std::vector<voxel::ChunkCoord> World::loaded_chunk_coords() const {
    std::vector<voxel::ChunkCoord> coords;
    coords.reserve(chunks_.size());
    for (const auto& [coord, entry] : chunks_) {
        if (entry.state >= ChunkLifecycleState::Generated) {
            coords.push_back(coord);
        }
    }
    return coords;
}

void World::update_streaming(voxel::ChunkCoord center, u32 load_radius, u32 unload_radius) {
    LCU_ASSERT(unload_radius >= load_radius);

    const i32 radius = static_cast<i32>(load_radius);
    for (i32 dx = -radius; dx <= radius; ++dx) {
        for (i32 dy = -radius; dy <= radius; ++dy) {
            for (i32 dz = -radius; dz <= radius; ++dz) {
                const voxel::ChunkCoord coord{center.x + dx, center.y + dy, center.z + dz};
                if (state_of(coord) == ChunkLifecycleState::Unloaded) {
                    load_chunk(coord);
                }
            }
        }
    }

    std::vector<voxel::ChunkCoord> to_unload;
    for (const auto& [coord, entry] : chunks_) {
        if (chebyshev_distance(coord, center) > unload_radius) {
            to_unload.push_back(coord);
        }
    }
    for (const voxel::ChunkCoord& coord : to_unload) {
        unload_chunk(coord);
    }
}

}  // namespace lcu::world
