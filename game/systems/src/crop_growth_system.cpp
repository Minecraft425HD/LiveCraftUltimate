#include "game/systems/crop_growth_system.h"

#include <algorithm>

namespace game::systems {

std::vector<lcu::voxel::ChunkCoord> update_crop_growth(lcu::world::World& world,
                                                        const lcu::lighting::DefaultWorldLight& light,
                                                        const CropGrowthConfig& config, std::mt19937& rng) {
    std::vector<lcu::voxel::ChunkCoord> touched_chunks;
    if (config.wheat_id == lcu::voxel::kAirBlockId) {
        return touched_chunks;
    }

    const lcu::f32 growth_chance = std::min(1.0f, kCropRandomTickIntervalSeconds / config.day_length_seconds);
    std::uniform_real_distribution<lcu::f32> roll(0.0f, 1.0f);

    for (const lcu::voxel::ChunkCoord& coord : world.loaded_chunk_coords()) {
        lcu::voxel::Chunk* chunk = world.chunk_at_mutable(coord);
        if (chunk == nullptr) {
            continue;
        }

        bool any_grew = false;
        for (lcu::u32 z = 0; z < lcu::voxel::Chunk::kEdgeLength; ++z) {
            for (lcu::u32 y = 0; y < lcu::voxel::Chunk::kEdgeLength; ++y) {
                for (lcu::u32 x = 0; x < lcu::voxel::Chunk::kEdgeLength; ++x) {
                    if (chunk->block_at(x, y, z) != config.wheat_id) {
                        continue;
                    }
                    const lcu::u8 state = chunk->state_at(x, y, z);
                    if (state >= kMaxWheatGrowthState) {
                        continue;
                    }

                    const auto ix = static_cast<lcu::i32>(x);
                    const auto iy = static_cast<lcu::i32>(y);
                    const auto iz = static_cast<lcu::i32>(z);
                    const lcu::u8 sky = light.sky_light_at(coord, ix, iy, iz).value_or(0);
                    const lcu::u8 block_light = light.block_light_at(coord, ix, iy, iz).value_or(0);
                    if (std::max(sky, block_light) < kCropGrowthMinLightLevel) {
                        continue;
                    }

                    if (roll(rng) < growth_chance) {
                        chunk->set_state(x, y, z, static_cast<lcu::u8>(state + 1));
                        any_grew = true;
                    }
                }
            }
        }

        if (any_grew) {
            touched_chunks.push_back(coord);
        }
    }

    return touched_chunks;
}

WheatHarvestDrops harvest_wheat(lcu::u8 state, std::mt19937& rng) {
    WheatHarvestDrops drops;
    if (state >= kMaxWheatGrowthState) {
        std::uniform_int_distribution<lcu::u32> count_roll(1, 3);
        drops.wheat_count = count_roll(rng);
        drops.seed_count = count_roll(rng);
    } else {
        drops.seed_count = 1;
    }
    return drops;
}

}  // namespace game::systems
