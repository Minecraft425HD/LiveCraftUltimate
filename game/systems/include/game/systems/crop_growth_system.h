#pragma once

#include <random>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/lighting/world_light.h"
#include "lcu/voxel/block_id.h"
#include "lcu/voxel/chunk_coord.h"
#include "lcu/world/world.h"

namespace game::systems {

// Real per-tick growth-chance calibration (Phase 64, brief section
// 64.4's own "+1 Stufe pro echtem Minecraft-Tag ueber ein Random-Tick-
// System"): rolled once per `kCropRandomTickIntervalSeconds` for every
// real, currently-loaded wheat block whose light (sky OR block) is
// >= `kCropGrowthMinLightLevel` - an independent Bernoulli trial per
// tick per eligible block, not a uniform full-chunk-volume position
// sample the way real Minecraft's own random tick literally works.
// Real Minecraft picks random (x,y,z) samples across the whole loaded
// volume and only *sometimes* lands on a crop; reproducing that exact
// scheme here would mean picking a real chunk/subchunk random-tick
// scheduler (with its own sample-count-per-tick tuning) just to
// re-derive "roughly one success per real day" for the one block type
// that can actually use it - real, avoidable complexity for this
// project's scale (see DECISIONS.md). Rolling directly against the
// small set of blocks that can grow reaches the same real "random,
// tick-driven, day-calibrated" requirement with far less real
// bookkeeping, while still being genuinely non-deterministic in when a
// given block advances (matching "random" tick, not "every block
// advances in lockstep at the exact same moment").
constexpr lcu::f32 kCropRandomTickIntervalSeconds = 1.0f;

// Sky/block light threshold a wheat block's own real position must
// meet (whichever is higher) to be eligible to grow this tick - the
// brief's own literal "gated auf Himmels-/Blocklicht >= 9" requirement.
constexpr lcu::u8 kCropGrowthMinLightLevel = 9;

// Wheat's own real 8 growth stages (0-7, see BlockDefinition::texture_
// index_offset_by_state) - state 7 is fully mature and never advances
// further.
constexpr lcu::u8 kMaxWheatGrowthState = 7;

// Real LCU_FAST_FARMING=1 dev toggle (brief section 64's own
// suggestion) - the CALLER (client/main.cpp) multiplies the real
// elapsed seconds it feeds into its own crop-growth accumulator by
// this factor when the env var is set, so real random-tick intervals
// (and therefore real growth chances) arrive far more often per real
// wall-clock second - a headless verification run can observe real
// growth to full maturity within a handful of real seconds instead of
// several real in-game days. This module itself never reads the
// environment (every other game/systems module keeps that same "no
// getenv here" convention) - it just runs its own real, unscaled math
// against whatever real elapsed time the caller hands it.
constexpr lcu::f32 kFastFarmingTimeScale = 200.0f;

struct CropGrowthConfig {
    lcu::voxel::BlockId wheat_id = lcu::voxel::kAirBlockId;
    // One full real day's real length in seconds (matches client/
    // main.cpp's own DayNightCycle construction) - the real per-tick
    // growth probability is `kCropRandomTickIntervalSeconds /
    // day_length_seconds`, so the EXPECTED number of successes over one
    // real day is exactly 1 regardless of how long a day actually is.
    lcu::f32 day_length_seconds = 120.0f;
};

// Real per-random-tick wheat growth scan (Phase 64): call once every
// real kCropRandomTickIntervalSeconds (the caller owns the real
// elapsed-seconds accumulator that decides when that real interval has
// elapsed - the same real pattern player_vitals_system's own interval
// timers already established). Scans every real currently-loaded chunk
// (`world.loaded_chunk_coords()`) for wheat blocks below `kMaxWheat
// GrowthState`, checks each one's own real light against `kCropGrowth
// MinLightLevel`, and independently rolls `rng` against the real
// calibrated growth chance for each eligible one - a real success
// advances that voxel's own real state by exactly 1 (via ChunkStorage::
// set_state, leaving its BlockId untouched). Returns every real chunk
// coordinate that had at least one block grow this call, so the caller
// knows which chunks need remeshing (this function itself never
// touches rendering) - the same real "return what changed, let the
// caller decide what to do about it" shape update_lighting_for_edit's
// own touched-chunks return already uses.
std::vector<lcu::voxel::ChunkCoord> update_crop_growth(lcu::world::World& world,
                                                        const lcu::lighting::DefaultWorldLight& light,
                                                        const CropGrowthConfig& config, std::mt19937& rng);

// Real harvest drop table (Phase 64, brief section 64.6): a mature
// wheat block (`state == kMaxWheatGrowthState`) drops a real, uniformly
// random 1-3 wheat AND 1-3 seeds; an immature one drops only 1 real
// seed and no wheat - matching real Minecraft's own "an immature crop
// only ever returns your seed investment, mature ones actually pay
// off" balance. Pure data (item counts only, no ItemId - this module
// stays free of an engine/items dependency it doesn't otherwise need);
// the caller maps these counts onto its own real wheat/seed ItemIds.
struct WheatHarvestDrops {
    lcu::u32 wheat_count = 0;
    lcu::u32 seed_count = 0;
};
WheatHarvestDrops harvest_wheat(lcu::u8 state, std::mt19937& rng);

}  // namespace game::systems
