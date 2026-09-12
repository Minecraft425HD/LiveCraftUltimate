#include "game/systems/crop_growth_system.h"

#include <gtest/gtest.h>

using game::systems::CropGrowthConfig;
using game::systems::harvest_wheat;
using game::systems::kMaxWheatGrowthState;
using game::systems::update_crop_growth;
using lcu::lighting::DefaultWorldLight;
using lcu::voxel::BlockId;
using lcu::voxel::Chunk;
using lcu::voxel::ChunkCoord;
using lcu::world::World;

namespace {

constexpr BlockId kWheatId = 42;
constexpr BlockId kOtherBlockId = 7;

lcu::world::ChunkGenerator empty_generator() {
    return [](Chunk&, ChunkCoord) {};
}

// A guaranteed-to-grow config: kCropRandomTickIntervalSeconds (1.0) /
// day_length_seconds (1.0) == 1.0, so every real roll in [0, 1) is
// always < the growth chance - deterministic without needing to fix
// the RNG's own seed/sequence.
CropGrowthConfig guaranteed_growth_config(BlockId wheat_id) {
    CropGrowthConfig config;
    config.wheat_id = wheat_id;
    config.day_length_seconds = 1.0f;
    return config;
}

// A guaranteed-NEVER-to-grow config, for isolating "light gate refused
// it" from "the roll refused it".
CropGrowthConfig guaranteed_no_growth_config(BlockId wheat_id) {
    CropGrowthConfig config;
    config.wheat_id = wheat_id;
    config.day_length_seconds = 1.0e9f;
    return config;
}

}  // namespace

TEST(CropGrowthSystem, WellLitWheatGrowsOneStage) {
    World world(1, empty_generator());
    world.load_chunk({0, 0, 0});
    world.chunk_at_mutable({0, 0, 0})->set_block_with_state(3, 3, 3, kWheatId, 0);

    DefaultWorldLight light;
    light.chunk_light({0, 0, 0}).set_sky_light(3, 3, 3, 15);

    std::mt19937 rng(1);
    const auto touched = update_crop_growth(world, light, guaranteed_growth_config(kWheatId), rng);

    EXPECT_EQ(world.chunk_at({0, 0, 0})->state_at(3, 3, 3), 1);
    EXPECT_EQ(touched.size(), 1u);
    EXPECT_EQ(touched[0], (ChunkCoord{0, 0, 0}));
}

TEST(CropGrowthSystem, FullyGrownWheatNeverAdvancesPastMaxState) {
    World world(1, empty_generator());
    world.load_chunk({0, 0, 0});
    world.chunk_at_mutable({0, 0, 0})->set_block_with_state(0, 0, 0, kWheatId, kMaxWheatGrowthState);

    DefaultWorldLight light;
    light.chunk_light({0, 0, 0}).set_sky_light(0, 0, 0, 15);

    std::mt19937 rng(1);
    const auto touched = update_crop_growth(world, light, guaranteed_growth_config(kWheatId), rng);

    EXPECT_EQ(world.chunk_at({0, 0, 0})->state_at(0, 0, 0), kMaxWheatGrowthState);
    EXPECT_TRUE(touched.empty());
}

TEST(CropGrowthSystem, InsufficientLightBlocksGrowthEvenWithAGuaranteedRoll) {
    World world(1, empty_generator());
    world.load_chunk({0, 0, 0});
    world.chunk_at_mutable({0, 0, 0})->set_block_with_state(5, 5, 5, kWheatId, 2);

    DefaultWorldLight light;
    light.chunk_light({0, 0, 0}).set_sky_light(5, 5, 5, 8);  // one below the real threshold

    std::mt19937 rng(1);
    const auto touched = update_crop_growth(world, light, guaranteed_growth_config(kWheatId), rng);

    EXPECT_EQ(world.chunk_at({0, 0, 0})->state_at(5, 5, 5), 2);
    EXPECT_TRUE(touched.empty());
}

TEST(CropGrowthSystem, ExactlyTheThresholdLightLevelIsSufficient) {
    World world(1, empty_generator());
    world.load_chunk({0, 0, 0});
    world.chunk_at_mutable({0, 0, 0})->set_block_with_state(1, 1, 1, kWheatId, 0);

    DefaultWorldLight light;
    light.chunk_light({0, 0, 0}).set_sky_light(1, 1, 1, 9);

    std::mt19937 rng(1);
    update_crop_growth(world, light, guaranteed_growth_config(kWheatId), rng);

    EXPECT_EQ(world.chunk_at({0, 0, 0})->state_at(1, 1, 1), 1);
}

TEST(CropGrowthSystem, BlockLightAloneAlsoSatisfiesTheGate) {
    // No real sky light at all (e.g. underground with a torch) - block
    // light alone meeting the threshold must still be enough, matching
    // real Minecraft's own "whichever is brighter" crop-growth rule.
    World world(1, empty_generator());
    world.load_chunk({0, 0, 0});
    world.chunk_at_mutable({0, 0, 0})->set_block_with_state(2, 2, 2, kWheatId, 0);

    DefaultWorldLight light;
    light.chunk_light({0, 0, 0}).set_sky_light(2, 2, 2, 0);
    light.chunk_light({0, 0, 0}).set_block_light(2, 2, 2, 10);

    std::mt19937 rng(1);
    update_crop_growth(world, light, guaranteed_growth_config(kWheatId), rng);

    EXPECT_EQ(world.chunk_at({0, 0, 0})->state_at(2, 2, 2), 1);
}

TEST(CropGrowthSystem, NonWheatBlocksAreNeverTouched) {
    World world(1, empty_generator());
    world.load_chunk({0, 0, 0});
    world.chunk_at_mutable({0, 0, 0})->set_block_with_state(4, 4, 4, kOtherBlockId, 0);

    DefaultWorldLight light;
    light.chunk_light({0, 0, 0}).set_sky_light(4, 4, 4, 15);

    std::mt19937 rng(1);
    const auto touched = update_crop_growth(world, light, guaranteed_growth_config(kWheatId), rng);

    EXPECT_EQ(world.chunk_at({0, 0, 0})->state_at(4, 4, 4), 0);
    EXPECT_TRUE(touched.empty());
}

TEST(CropGrowthSystem, ZeroChanceConfigNeverGrowsWellLitWheat) {
    World world(1, empty_generator());
    world.load_chunk({0, 0, 0});
    world.chunk_at_mutable({0, 0, 0})->set_block_with_state(6, 6, 6, kWheatId, 0);

    DefaultWorldLight light;
    light.chunk_light({0, 0, 0}).set_sky_light(6, 6, 6, 15);

    std::mt19937 rng(1);
    const auto touched = update_crop_growth(world, light, guaranteed_no_growth_config(kWheatId), rng);

    EXPECT_EQ(world.chunk_at({0, 0, 0})->state_at(6, 6, 6), 0);
    EXPECT_TRUE(touched.empty());
}

TEST(CropGrowthSystem, UnconfiguredWheatIdIsARealSafeNoOp) {
    World world(1, empty_generator());
    world.load_chunk({0, 0, 0});

    DefaultWorldLight light;
    CropGrowthConfig config;  // wheat_id defaults to kAirBlockId - never real.
    std::mt19937 rng(1);
    EXPECT_TRUE(update_crop_growth(world, light, config, rng).empty());
}

TEST(HarvestWheat, ImmatureWheatDropsExactlyOneSeedAndNoWheat) {
    std::mt19937 rng(1);
    for (lcu::u8 state = 0; state < kMaxWheatGrowthState; ++state) {
        const auto drops = harvest_wheat(state, rng);
        EXPECT_EQ(drops.wheat_count, 0u) << "state " << static_cast<int>(state);
        EXPECT_EQ(drops.seed_count, 1u) << "state " << static_cast<int>(state);
    }
}

TEST(HarvestWheat, MatureWheatDropsOneToThreeWheatAndOneToThreeSeeds) {
    std::mt19937 rng(1);
    for (int i = 0; i < 200; ++i) {
        const auto drops = harvest_wheat(kMaxWheatGrowthState, rng);
        EXPECT_GE(drops.wheat_count, 1u);
        EXPECT_LE(drops.wheat_count, 3u);
        EXPECT_GE(drops.seed_count, 1u);
        EXPECT_LE(drops.seed_count, 3u);
    }
}

TEST(HarvestWheat, MatureWheatDropCountsAreRealNotAlwaysTheSame) {
    // A real regression guard against an accidentally-constant "random"
    // roll (e.g. a broken distribution always returning its own min).
    std::mt19937 rng(1);
    bool saw_a_different_wheat_count = false;
    const auto first = harvest_wheat(kMaxWheatGrowthState, rng).wheat_count;
    for (int i = 0; i < 50; ++i) {
        if (harvest_wheat(kMaxWheatGrowthState, rng).wheat_count != first) {
            saw_a_different_wheat_count = true;
            break;
        }
    }
    EXPECT_TRUE(saw_a_different_wheat_count);
}
