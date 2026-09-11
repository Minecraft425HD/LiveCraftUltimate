#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "game/components/ai_wander.h"
#include "game/components/position.h"
#include "game/items/block_item_mapping.h"
#include "game/systems/ai_wander_system.h"
#include "game/systems/replication_protocol.h"
#include "lcu/core/log.h"
#include "lcu/core/quality_profile.h"
#include "lcu/core/types.h"
#include "lcu/ecs/registry.h"
#include "lcu/items/inventory.h"
#include "lcu/items/item_registry.h"
#include "lcu/network/connection.h"
#include "lcu/network/fragmentation.h"
#include "lcu/network/udp_socket.h"
#include "lcu/physics/collision.h"
#include "lcu/serialization/chunk_serializer.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"
#include "lcu/world/world.h"
#include "lcu/world/worldgen.h"

#if defined(LCU_ENABLE_SCRIPTING)
#include "lcu/modding/event_bus.h"
#include "lcu/modding/mod_loader.h"
#include "lcu/modding/registry_bindings.h"
#include "lcu/scripting/lua_state.h"
#endif

namespace {

namespace protocol = game::systems::protocol;

struct ServerConfig {
    std::string world = "world";
    lcu::u16 port = 25565;
};

// Minimal CLI parsing for `VoxelServer --world <name> --port <n>`. Full
// server config file support lands with a dedicated config system, not
// this phase's job.
ServerConfig parse_args(int argc, char** argv) {
    ServerConfig config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--world" && i + 1 < argc) {
            config.world = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<lcu::u16>(std::strtoul(argv[++i], nullptr, 10));
        }
    }
    return config;
}

std::optional<lcu::u64> max_ticks_from_env() {
    const char* value = std::getenv("LCU_MAX_TICKS");
    if (!value) {
        return std::nullopt;
    }
    return static_cast<lcu::u64>(std::strtoull(value, nullptr, 10));
}

constexpr lcu::u32 kWorldSeed = 1337;

// Real spawn placement (Phase 37) - mirrors VoxelClient's own find_dry_
// spawn_column exactly (see that file's comment): terrain_height() is
// now centered on sea level, so a fixed world (0,0) spawn column can
// legitimately land underwater by pure chance, and no swim mechanics
// exist yet. Same seed, same deterministic search, so client and
// server agree on where "spawn" is without needing to send it over
// the wire.
//
// kMaxRadius (Phase 38): worldgen's continental noise stage varies at
// a much lower frequency than this search radius was originally sized
// for (~666-block wavelength - see worldgen.cpp's kContinentalNoise
// Scale) - confirmed via a real search for seed 1337 needing radius 84
// to find any dry land at all. 1024 gives real headroom; an O(ring-
// perimeter) search (only the new ring's boundary, not a re-scanned
// square) keeps even the worst case a fast, one-time startup cost.
struct SpawnColumn {
    lcu::i32 x = 0;
    lcu::i32 z = 0;
};

SpawnColumn find_dry_spawn_column(lcu::u32 seed) {
    const auto is_dry = [seed](lcu::i32 x, lcu::i32 z) {
        return lcu::world::worldgen::terrain_height(seed, x, z) >= lcu::world::worldgen::kSeaLevel;
    };
    if (is_dry(0, 0)) {
        return {0, 0};
    }
    constexpr lcu::i32 kMaxRadius = 1024;
    for (lcu::i32 radius = 1; radius <= kMaxRadius; ++radius) {
        for (lcu::i32 x = -radius; x <= radius; ++x) {
            if (is_dry(x, -radius)) {
                return {x, -radius};
            }
            if (is_dry(x, radius)) {
                return {x, radius};
            }
        }
        for (lcu::i32 z = -radius + 1; z <= radius - 1; ++z) {
            if (is_dry(-radius, z)) {
                return {-radius, z};
            }
            if (is_dry(radius, z)) {
                return {radius, z};
            }
        }
    }
    return {0, 0};
}

// Sized by LCU_QUALITY_PROFILE (Phase 10, see lcu::core::QualityProfile) -
// defaults to Desktop, numerically identical to this vertical slice's
// original hardcoded 1/0/3 radius/min/max values.
lcu::core::ChunkLoadSettings load_settings_from_env() {
    const char* profile_name = std::getenv("LCU_QUALITY_PROFILE");
    lcu::core::QualityProfile profile = lcu::core::QualityProfile::Desktop;
    if (profile_name != nullptr) {
        profile = lcu::core::parse_quality_profile(profile_name).value_or(lcu::core::QualityProfile::Desktop);
    }
    return lcu::core::chunk_load_settings_for(profile);
}

constexpr int kAiEntityCount = 3;
constexpr lcu::u32 kAiRngSeed = 20260909;
constexpr int kTicksPerSecond = 20;

// A client-reported PlayerInput's `dt` is trusted for the movement math
// (brief section 64 doesn't require full movement validation/anti-cheat
// yet - see DECISIONS.md), but clamped to a sane ceiling so a bogus or
// malicious huge dt can't move a player an absurd distance in one input.
constexpr lcu::f32 kMaxAcceptedInputDt = 0.25f;

// How often (in ticks) each connection gets a PlayerCorrection - not
// every single tick, since the correction only matters when there's
// something to reconcile against, and this keeps the broadcast volume
// down (still real "authoritative periodic sync", just not maximal
// frequency - see DECISIONS.md).
constexpr lcu::u64 kCorrectionIntervalTicks = 4;

// Interest management (brief section 22/64): a client's EntityState
// broadcast only includes AI entities within this many blocks of that
// client's own (server-known) player position - real filtering logic,
// even though every entity in this vertical slice's small world
// currently falls within it (see DECISIONS.md).
constexpr lcu::f32 kInterestRadius = 24.0f;

// A requested block edit is rejected if its target is farther than this
// from the requesting client's own (server-known) player position -
// brief section 20's "never blindly accept client data": without this, a
// malicious client could send BlockAction for any coordinate in the
// loaded world regardless of where its player actually is. Deliberately
// looser than VoxelClient's own kInteractRange (6.0f, client/main.cpp) to
// tolerate normal client/server position drift under real latency, not
// tuned any tighter without real-world ping data to justify it.
constexpr lcu::f32 kMaxBlockActionRange = 10.0f;

// Real disconnect detection (Phase 20): a client that hasn't sent a
// single packet in this many real seconds is presumed gone and pruned
// from `clients` - UDP has no notion of a "connection" to signal this
// any other way (brief section 19/20). Deliberately short for this
// vertical slice's own real-run verification turnaround, not tuned
// against real-network jitter/packet-loss data (a live client sends
// PlayerInput every single frame while networked, unthrottled - see
// BUILD_STATUS.md - so a genuinely-connected client's last packet is
// always a small fraction of a second old; this only ever fires for a
// peer that's actually gone).
constexpr lcu::f32 kClientTimeoutSeconds = 5.0f;

struct ClientState {
    lcu::network::Connection connection;
    lcu::physics::PlayerPhysicsState player;
    lcu::u32 last_acknowledged_sequence = 0;
    // Distinguishes this client's concurrently in-flight fragmented
    // ChunkData messages from each other (see lcu::network::fragment_payload).
    // A per-client counter is enough - see fragmentation.h.
    lcu::u16 next_chunk_message_id = 0;
    // Server-authoritative item storage (brief section 20 / Phase 15) -
    // 9 slots, matching VoxelClient's own `player_inventory`. Starts
    // empty; populated only by validated `BlockAction`s (see
    // handle_block_action below), never by the client directly.
    lcu::items::Inventory inventory{9};
    // Per-movement chunk streaming (Phase 16): the chunk column this
    // client's loaded-chunk range was last computed around, re-checked
    // every tick against the client's current position. `nullopt` means
    // "never computed yet" - deliberately not pre-set to the spawn
    // column at connect time (Phase 20 fix): a client's own spawn-area
    // chunks might have been unloaded again by the time they connect
    // (interest-scoped unloading, see below), so the very first
    // movement-loop pass after insertion must always run the real
    // load-or-reload-from-disk logic for this client's own area, not be
    // skipped as "no change" just because the coincidental center value
    // matched.
    std::optional<lcu::voxel::ChunkCoord> last_streamed_center;
    // Interest-scoped chunk unloading (Phase 20): every chunk currently
    // within this client's own load radius, recomputed in full
    // whenever `last_streamed_center` changes. The union of every
    // connected client's `interest_set` is what a chunk needs to be
    // absent from before the server unloads it - see DECISIONS.md
    // "interest-scoped chunk unloading".
    std::unordered_set<lcu::voxel::ChunkCoord> interest_set;
    // Real disconnect detection (Phase 20): when a UDP peer stops
    // sending anything at all, this is how the server ever notices -
    // updated on every packet received from this address, checked each
    // tick against `kClientTimeoutSeconds`. Without this, `clients`
    // (and every chunk a departed player's `interest_set` was holding
    // open) would grow and never shrink for the server process's
    // entire lifetime.
    std::chrono::steady_clock::time_point last_packet_time;
};

}  // namespace

int main(int argc, char** argv) {
    const ServerConfig config = parse_args(argc, argv);

    LCU_LOG_INFO("VoxelServer starting: world=\"{}\" port={}", config.world, config.port);
    LCU_LOG_INFO("Headless dedicated server: no SDL, no bgfx, no GPU (see ARCHITECTURE.md)");

    // --- Real world simulation (replacing the Phase 0 sleep-only placeholder) ---
    // Registered in the exact same order as VoxelClient's own block
    // registration (stone, then grass, then dirt) - BlockId assignment
    // is sequential, and every wire message carrying a raw block_id
    // (BlockAction/BlockChange/ChunkData) relies on both sides agreeing
    // on what each id means, the same simplification already
    // documented for item ids (see DECISIONS.md "server-side inventory
    // (Phase 15)").
    lcu::voxel::BlockRegistry block_registry;
    lcu::voxel::BlockDefinition stone_def;
    stone_def.namespaced_id = "game:stone";
    stone_def.display_name = "Stone";
    stone_def.is_transparent = false;
    stone_def.has_collision = true;
    const lcu::voxel::BlockId stone_id = block_registry.register_block(stone_def);

    lcu::voxel::BlockDefinition grass_def;
    grass_def.namespaced_id = "game:grass";
    grass_def.display_name = "Grass";
    grass_def.is_transparent = false;
    grass_def.has_collision = true;
    const lcu::voxel::BlockId grass_id = block_registry.register_block(grass_def);

    lcu::voxel::BlockDefinition dirt_def;
    dirt_def.namespaced_id = "game:dirt";
    dirt_def.display_name = "Dirt";
    dirt_def.is_transparent = false;
    dirt_def.has_collision = true;
    const lcu::voxel::BlockId dirt_id = block_registry.register_block(dirt_def);

    // Mirrors VoxelClient's own registration exactly, same position in
    // the sequence (Phase 39: real climate/biome content) - BlockId
    // alignment across both sides depends on registering in the exact
    // same order, same as every other block here.
    lcu::voxel::BlockDefinition sand_def;
    sand_def.namespaced_id = "game:sand";
    sand_def.display_name = "Sand";
    sand_def.is_transparent = false;
    sand_def.has_collision = true;
    const lcu::voxel::BlockId sand_id = block_registry.register_block(sand_def);

    lcu::voxel::BlockDefinition snow_def;
    snow_def.namespaced_id = "game:snow";
    snow_def.display_name = "Snow";
    snow_def.is_transparent = false;
    snow_def.has_collision = true;
    const lcu::voxel::BlockId snow_id = block_registry.register_block(snow_def);

    // Mirrors VoxelClient's own registration exactly, same as every
    // other block above (Phase 34) - the server needs its own
    // authoritative copy of game:torch's light_emission/is_transparent/
    // has_collision so a real placed torch validates and replicates
    // correctly (handle_block_action below already works generically
    // off whatever's in block_registry, no torch-specific code needed
    // there). is_transparent=false (a solid glowing cube, not a
    // cross/billboard shape) for the same reason VoxelClient's own copy
    // is - see that file's comment on this exact field.
    lcu::voxel::BlockDefinition torch_def;
    torch_def.namespaced_id = "game:torch";
    torch_def.display_name = "Torch";
    torch_def.is_transparent = false;
    torch_def.has_collision = true;
    torch_def.light_emission = 14;
    const lcu::voxel::BlockId torch_id = block_registry.register_block(torch_def);

    // Mirrors VoxelClient's own registration exactly (Phase 37: sea
    // level + water) - the server needs its own authoritative copy so
    // a chunk it generates (and streams to clients) has water where
    // it should, and so has_collision=false is honored server-side
    // too if server-authoritative collision against water is ever
    // added. is_transparent=false for the same "no transparent-layer
    // meshing exists yet" reason VoxelClient's own comment gives - the
    // server never renders anything, but BlockId alignment across both
    // registrations matters more than this specific field for it.
    lcu::voxel::BlockDefinition water_def;
    water_def.namespaced_id = "game:water";
    water_def.display_name = "Water";
    water_def.is_transparent = false;
    water_def.has_collision = false;
    const lcu::voxel::BlockId water_id = block_registry.register_block(water_def);

    // Mirrors VoxelClient's own registration exactly (Phase 40: real
    // caves/ores pipeline stage) - the server needs its own authoritative
    // copy so a chunk it generates has the same ore blocks a client
    // generating the same seed/coord independently would compute, and so
    // BlockId alignment across both sides holds the same as every block
    // above.
    lcu::voxel::BlockDefinition coal_ore_def;
    coal_ore_def.namespaced_id = "game:coal_ore";
    coal_ore_def.display_name = "Coal Ore";
    coal_ore_def.is_transparent = false;
    coal_ore_def.has_collision = true;
    const lcu::voxel::BlockId coal_ore_id = block_registry.register_block(coal_ore_def);

    lcu::voxel::BlockDefinition iron_ore_def;
    iron_ore_def.namespaced_id = "game:iron_ore";
    iron_ore_def.display_name = "Iron Ore";
    iron_ore_def.is_transparent = false;
    iron_ore_def.has_collision = true;
    const lcu::voxel::BlockId iron_ore_id = block_registry.register_block(iron_ore_def);

    // Mirrors VoxelClient's own registration exactly (Phase 41: real
    // vegetation pipeline stage) - the server needs its own
    // authoritative copy so a chunk it generates has the same
    // trees/cacti a client generating the same seed/coord independently
    // would compute, and so BlockId alignment across both sides holds
    // the same as every block above.
    lcu::voxel::BlockDefinition wood_def;
    wood_def.namespaced_id = "game:wood";
    wood_def.display_name = "Wood";
    wood_def.is_transparent = false;
    wood_def.has_collision = true;
    const lcu::voxel::BlockId wood_id = block_registry.register_block(wood_def);

    lcu::voxel::BlockDefinition leaves_def;
    leaves_def.namespaced_id = "game:leaves";
    leaves_def.display_name = "Leaves";
    leaves_def.is_transparent = false;
    leaves_def.has_collision = true;
    const lcu::voxel::BlockId leaves_id = block_registry.register_block(leaves_def);

    lcu::voxel::BlockDefinition cactus_def;
    cactus_def.namespaced_id = "game:cactus";
    cactus_def.display_name = "Cactus";
    cactus_def.is_transparent = false;
    cactus_def.has_collision = true;
    const lcu::voxel::BlockId cactus_id = block_registry.register_block(cactus_def);

    // Mirrors VoxelClient's own registration exactly (brief section 20:
    // server-side inventory, Phase 15) - both sides independently
    // register the same one item in the same order, so their ItemIds
    // coincide by construction, the same simplification block/item ids
    // already carry for mod content (see DECISIONS.md "server-side
    // inventory (Phase 15)").
    lcu::items::ItemRegistry item_registry;
    lcu::items::ItemDefinition stone_item_def;
    stone_item_def.namespaced_id = "game:stone";
    stone_item_def.display_name = "Stone";
    stone_item_def.max_stack_size = 64;
    const lcu::items::ItemId stone_item_id = item_registry.register_item(stone_item_def);

    // Registered in the same order as VoxelClient, same as game:stone's
    // own comment above - both sides' ItemIds coincide by construction.
    // Now tracked server-side too (Phase 19: extends Phase 15's
    // server-side inventory past game:stone alone - see DECISIONS.md).
    lcu::items::ItemDefinition grass_item_def;
    grass_item_def.namespaced_id = "game:grass";
    grass_item_def.display_name = "Grass";
    grass_item_def.max_stack_size = 64;
    const lcu::items::ItemId grass_item_id = item_registry.register_item(grass_item_def);

    lcu::items::ItemDefinition dirt_item_def;
    dirt_item_def.namespaced_id = "game:dirt";
    dirt_item_def.display_name = "Dirt";
    dirt_item_def.max_stack_size = 64;
    const lcu::items::ItemId dirt_item_id = item_registry.register_item(dirt_item_def);

    lcu::items::ItemDefinition torch_item_def;
    torch_item_def.namespaced_id = "game:torch";
    torch_item_def.display_name = "Torch";
    torch_item_def.max_stack_size = 64;
    const lcu::items::ItemId torch_item_id = item_registry.register_item(torch_item_def);

#if defined(LCU_ENABLE_SCRIPTING)
    // Mods run here too (Phase 9) so a mod's registered blocks/items exist
    // in the server's authoritative registries, not just the client's -
    // ids otherwise silently drift between the two (registries aren't
    // synced over the wire yet, so both sides still have to load the same
    // mods locally to agree). EventBus is constructed and exposed here
    // for the same reason emit_block_broken *is* called below (Phase 13
    // made block edits server-authoritative, so this is where a break
    // actually happens) - a mod script is shared between client and
    // server, so lcu.subscribe(...) has to exist on both or a mod that
    // calls it unconditionally fails to load on whichever host lacks it.
    // (item_crafted, Phase 23, is the opposite case: purely client-side,
    // so only VoxelClient ever calls emit_item_crafted - the server still
    // needs EventBus/lcu.subscribe to exist so a mod subscribing to it
    // doesn't fail to load here, it just never actually fires server-side.)
    lcu::scripting::LuaState mod_lua;
    lcu::modding::EventBus mod_event_bus(mod_lua);
    mod_event_bus.expose_to_lua();
    lcu::modding::bind_block_registry(mod_lua, block_registry);
    lcu::modding::bind_item_registry(mod_lua, item_registry);
    lcu::modding::ModLoader mod_loader(mod_lua);
    mod_loader.load_all("mods");
#endif

    const lcu::world::worldgen::BiomeBlocks biome_blocks{
        /*plains_surface=*/grass_id, /*plains_subsurface=*/dirt_id,
        /*desert_surface=*/sand_id, /*desert_subsurface=*/sand_id,
        /*snowy_surface=*/snow_id,  /*snowy_subsurface=*/dirt_id,
    };
    const lcu::world::worldgen::OreBlocks ore_blocks{
        /*coal_ore=*/coal_ore_id,
        /*iron_ore=*/iron_ore_id,
    };
    const lcu::world::worldgen::VegetationBlocks vegetation_blocks{
        /*wood=*/wood_id,
        /*leaves=*/leaves_id,
        /*cactus=*/cactus_id,
    };
    lcu::world::World world(kWorldSeed, [&](lcu::voxel::Chunk& chunk, lcu::voxel::ChunkCoord coord) {
        lcu::world::worldgen::generate_terrain_chunk(chunk, coord, kWorldSeed, biome_blocks, stone_id, water_id,
                                                       ore_blocks, vegetation_blocks);
    });

    // Real chunk persistence trigger (Phase 20) - `engine/serialization::
    // chunk_serializer` existed since Phase 3 but nothing ever actually
    // called it from a live session (see PROJECT_STATE.md "Known
    // Limitations"). Interest-scoped unloading (below) is that real
    // trigger: without saving an edited chunk before it's evicted and
    // loading it back instead of regenerating pristine terrain, a chunk
    // a player edited then wandered away from would silently revert the
    // moment they (or anyone) wandered back - a real correctness bug,
    // not just a missed optimization. Scoped to this one server
    // process's own session (not full restart persistence - a save
    // directory from a previous run is never consulted for the initial
    // static area below, only for unload/reload within this run) - see
    // DECISIONS.md "interest-scoped chunk unloading".
    const std::filesystem::path chunk_save_dir = std::filesystem::path(config.world) / "chunks";
    std::filesystem::create_directories(chunk_save_dir);
    const auto chunk_file_path = [&](lcu::voxel::ChunkCoord coord) {
        return (chunk_save_dir / fmt::format("{}_{}_{}.chunk", coord.x, coord.y, coord.z)).string();
    };

    const lcu::core::ChunkLoadSettings load_settings = load_settings_from_env();
    // Phase 37: centers on the real dry spawn column, not always chunk
    // (0,0) - see find_dry_spawn_column's doc comment.
    const SpawnColumn spawn_column = find_dry_spawn_column(kWorldSeed);
    const lcu::voxel::ChunkCoord spawn_chunk =
        lcu::voxel::world_to_chunk_and_local({spawn_column.x, 0, spawn_column.z}, lcu::voxel::Chunk::kEdgeLength)
            .chunk;
    for (lcu::i32 cx = spawn_chunk.x - load_settings.radius_xz; cx <= spawn_chunk.x + load_settings.radius_xz;
         ++cx) {
        for (lcu::i32 cz = spawn_chunk.z - load_settings.radius_xz; cz <= spawn_chunk.z + load_settings.radius_xz;
             ++cz) {
            for (lcu::i32 cy = load_settings.min_chunk_y; cy <= load_settings.max_chunk_y; ++cy) {
                world.load_chunk({cx, cy, cz});
            }
        }
    }
    LCU_LOG_INFO("Loaded {} chunks (seed={}) around spawn column ({},{})", world.loaded_chunk_count(), kWorldSeed,
                 spawn_column.x, spawn_column.z);

    const auto is_solid = [&](lcu::voxel::BlockId id) { return block_registry.definition_of(id).has_collision; };
    const lcu::physics::PlayerPhysicsConfig physics_config;
    const lcu::i32 spawn_ground_y =
        lcu::world::worldgen::terrain_height(kWorldSeed, spawn_column.x, spawn_column.z) + 1;

    // Per-movement chunk streaming (Phase 16, see DECISIONS.md): which
    // chunk column a world-space position falls in, for deciding when a
    // client has moved far enough to need its loaded-chunk range
    // re-checked.
    const auto chunk_coord_of_position = [](const lcu::math::Vec3& pos) {
        const lcu::voxel::BlockWorldCoord block{static_cast<lcu::i64>(std::floor(pos.x)),
                                                 static_cast<lcu::i64>(std::floor(pos.y)),
                                                 static_cast<lcu::i64>(std::floor(pos.z))};
        return lcu::voxel::world_to_chunk_and_local(block, lcu::voxel::Chunk::kEdgeLength).chunk;
    };

    // Interest-scoped chunk unloading (Phase 20, see DECISIONS.md):
    // every chunk coordinate within `load_settings`' radius of `center`
    // - the exact same shape the per-movement streaming loop already
    // walks to decide what to *load*, reused here to decide what a
    // client still needs kept loaded at all.
    const auto compute_interest_set = [&](lcu::voxel::ChunkCoord center) {
        std::unordered_set<lcu::voxel::ChunkCoord> interest;
        for (lcu::i32 cx = center.x - load_settings.radius_xz; cx <= center.x + load_settings.radius_xz; ++cx) {
            for (lcu::i32 cz = center.z - load_settings.radius_xz; cz <= center.z + load_settings.radius_xz; ++cz) {
                for (lcu::i32 cy = load_settings.min_chunk_y; cy <= load_settings.max_chunk_y; ++cy) {
                    interest.insert({cx, cy, cz});
                }
            }
        }
        return interest;
    };

    auto make_spawn_aabb = [&](lcu::math::Vec3 feet) {
        constexpr lcu::f32 kHalfWidth = 0.3f;
        constexpr lcu::f32 kHeight = 1.8f;
        return lcu::physics::AABB{{feet.x - kHalfWidth, feet.y, feet.z - kHalfWidth},
                                   {feet.x + kHalfWidth, feet.y + kHeight, feet.z + kHalfWidth}};
    };

    // Same wandering AI simulation VoxelClient runs, now driven
    // server-side - a dedicated server needs to simulate entities
    // whether or not any client is even connected to watch them (brief
    // section 64's "server-authoritative state").
    lcu::ecs::Registry entity_registry;
    std::mt19937 ai_rng(kAiRngSeed);
    for (int i = 0; i < kAiEntityCount; ++i) {
        const lcu::f32 angle = static_cast<lcu::f32>(i) * (6.28318f / static_cast<lcu::f32>(kAiEntityCount));
        const lcu::math::Vec3 spawn_pos{static_cast<lcu::f32>(spawn_column.x) + 4.0f * std::cos(angle),
                                         static_cast<lcu::f32>(spawn_ground_y),
                                         static_cast<lcu::f32>(spawn_column.z) + 4.0f * std::sin(angle)};
        const lcu::ecs::EntityId entity = entity_registry.create_entity();
        entity_registry.add_component<game::components::Position>(entity, {spawn_pos});
        entity_registry.add_component<game::components::AIWander>(entity, {spawn_pos, 1.5f, 0.0f});
    }
    game::systems::AIWanderConfig ai_wander_config;
    LCU_LOG_INFO("Spawned {} wandering AI entities", entity_registry.entity_count());

    // --- Real network transport (replacing "no networking at all") ---
    lcu::network::UdpSocket socket;
    if (!socket.bind(config.port)) {
        LCU_LOG_ERROR("Failed to bind UDP port {}, exiting", config.port);
        return 1;
    }
    LCU_LOG_INFO("Listening for connections on UDP port {}", config.port);

    // UDP has no built-in notion of "a connection" - the server treats
    // any address it's received at least one datagram from as a known
    // client, keyed by that address (the same technique
    // tests/network/loopback_integration_test.cpp uses to "learn" a
    // peer's address). No handshake/auth beyond that exists yet - see
    // DECISIONS.md; nothing gatekeeps who can send a packet and be
    // treated as connected, which is fine for this vertical slice with
    // no untrusted network exposure, not for a real public server.
    std::unordered_map<lcu::network::Address, ClientState> clients;

    // Every successfully applied block edit, in order - replayed in full
    // to each newly connecting client right after its Welcome (see the
    // `inserted` handling below) so it catches up on edits that happened
    // before it joined, not just ones that happen from now on. Unbounded
    // for the lifetime of this process - fine for a vertical slice's
    // session lengths; a real long-running server would need to
    // periodically compact this against actual persisted chunk state
    // (once chunk save/load is wired to a server trigger - see
    // PROJECT_STATE.md "Known Limitations") rather than keep every edit
    // forever.
    std::vector<protocol::BlockChange> block_change_history;

    // Data-driven block->item mapping (Phase 22, closing this file's
    // own remaining honest gap: `item_for_block` used to be three
    // explicit `if` checks, one per block, that had to be extended by
    // hand every time a new item-backed block was added - and kept in
    // sync with VoxelClient's own identical chain by hand too. Same
    // `game::items::BlockItemMapping` table VoxelClient now uses,
    // populated with the same three pairs (this server's own
    // stone/grass/dirt ItemIds, not shared with the client process -
    // see DECISIONS.md "server connection model"). Returns kNoItemId
    // for anything else (mod content included) - not every block has
    // to be item-backed.
    game::items::BlockItemMapping block_item_mapping;
    block_item_mapping.register_pair(stone_id, stone_item_id);
    block_item_mapping.register_pair(grass_id, grass_item_id);
    block_item_mapping.register_pair(dirt_id, dirt_item_id);
    block_item_mapping.register_pair(torch_id, torch_item_id);
    const auto item_for_block = [&](lcu::voxel::BlockId block_id) {
        return block_item_mapping.item_for_block(block_id);
    };

    // Every item this server tracks a per-client authoritative count
    // for - used to correct a client's optimistic guess after every
    // BlockAction, not just the one item (if any) that request actually
    // touched, so a stale guess for an *unrelated* tracked item (e.g.
    // from an earlier request that arrived out of order) also gets
    // corrected eventually.
    const std::array<lcu::items::ItemId, 4> tracked_items{stone_item_id, grass_item_id, dirt_item_id,
                                                            torch_item_id};

    // Sends `client`'s current authoritative count for every tracked
    // item - called after every BlockAction that could have affected
    // one, accepted or rejected, so the client's own optimistic local
    // guess (see DECISIONS.md) gets corrected the instant it diverges
    // from what the server actually did.
    auto send_inventory_updates = [&](ClientState& client) {
        for (lcu::items::ItemId item_id : tracked_items) {
            client.connection.send(
                lcu::network::Channel::ReliableOrdered,
                protocol::encode_inventory_update({item_id, client.inventory.count_item(item_id)}));
        }
    };

    // Server-authoritative block editing (brief section 8/19/20): validates
    // and applies a client's requested break/place against this server's
    // own World, then broadcasts the result to every connected client
    // (including the requester - no client mutates its own World
    // speculatively for a block edit, see DECISIONS.md). Rejects a
    // request whose target chunk isn't loaded, whose target block isn't
    // actually breakable/placeable given its current state, whose
    // requested block_id isn't a registered block, whose target is too
    // far from the requesting client's own known position, or (for a
    // place whose requested block_id has an item mapping) whose
    // requester doesn't actually hold one server-side - logged, no reply
    // sent beyond the inventory correction above (the requester's world
    // simply doesn't change).
    auto handle_block_action = [&](const lcu::network::Address& from, ClientState& client,
                                    const protocol::BlockAction& action) {
        const lcu::voxel::BlockWorldCoord target{action.x, action.y, action.z};
        const lcu::math::Vec3 target_center{static_cast<lcu::f32>(target.x) + 0.5f,
                                             static_cast<lcu::f32>(target.y) + 0.5f,
                                             static_cast<lcu::f32>(target.z) + 0.5f};
        const lcu::f32 distance = lcu::math::length(target_center - client.player.aabb.center());
        if (distance > kMaxBlockActionRange) {
            LCU_LOG_WARN("Rejected BlockAction from {}: target ({},{},{}) is {:.1f} blocks away (max {})",
                         from.to_string(), target.x, target.y, target.z, distance, kMaxBlockActionRange);
            send_inventory_updates(client);
            return;
        }

        const auto split = lcu::voxel::world_to_chunk_and_local(target, lcu::voxel::Chunk::kEdgeLength);
        lcu::voxel::Chunk* chunk = world.chunk_at_mutable(split.chunk);
        if (chunk == nullptr) {
            LCU_LOG_WARN("Rejected BlockAction from {}: chunk ({},{},{}) isn't loaded", from.to_string(),
                         split.chunk.x, split.chunk.y, split.chunk.z);
            send_inventory_updates(client);
            return;
        }

        const lcu::voxel::BlockId current = chunk->block_at(split.local.x, split.local.y, split.local.z);
        lcu::voxel::BlockId new_id = current;
        bool valid = false;
        if (action.action == protocol::BlockActionType::Break) {
            valid = current != lcu::voxel::kAirBlockId;
            new_id = lcu::voxel::kAirBlockId;
        } else {
            // Placing an item-backed block requires the client to
            // actually hold one, server-side (brief section 20 applied
            // to item accounting, not just block edits - see
            // DECISIONS.md "server-side inventory"). A block with no
            // item mapping (e.g. mod content) isn't gated here.
            const lcu::items::ItemId required_item = item_for_block(action.block_id);
            const bool has_required_item =
                required_item == lcu::items::kNoItemId || client.inventory.count_item(required_item) > 0;
            valid = current == lcu::voxel::kAirBlockId && action.block_id < block_registry.count() &&
                    has_required_item;
            new_id = action.block_id;
        }
        if (!valid) {
            LCU_LOG_WARN("Rejected BlockAction from {}: target ({},{},{}) current_block={} requested_block={}",
                         from.to_string(), target.x, target.y, target.z, current, action.block_id);
            send_inventory_updates(client);
            return;
        }

        chunk->set_block(split.local.x, split.local.y, split.local.z, new_id);
        LCU_LOG_INFO("Applied BlockAction from {}: ({},{},{}) {} -> {}", from.to_string(), target.x, target.y,
                     target.z, current, new_id);
#if defined(LCU_ENABLE_SCRIPTING)
        if (new_id == lcu::voxel::kAirBlockId) {
            mod_event_bus.emit_block_broken(target.x, target.y, target.z, current);
        }
#endif
        if (action.action == protocol::BlockActionType::Break) {
            const lcu::items::ItemId dropped_item = item_for_block(current);
            if (dropped_item != lcu::items::kNoItemId) {
                client.inventory.add_item(item_registry, {dropped_item, 1});
            }
        } else {
            const lcu::items::ItemId consumed_item = item_for_block(new_id);
            if (consumed_item != lcu::items::kNoItemId) {
                client.inventory.remove_item(consumed_item, 1);
            }
        }
        send_inventory_updates(client);

        const protocol::BlockChange change{target.x, target.y, target.z, new_id};
        block_change_history.push_back(change);
        const auto change_bytes = protocol::encode_block_change(change);
        for (auto& [broadcast_addr, broadcast_client] : clients) {
            broadcast_client.connection.send(lcu::network::Channel::ReliableOrdered, change_bytes);
        }
    };

    constexpr auto kTickDuration = std::chrono::milliseconds(1000 / kTicksPerSecond);
    constexpr lcu::f32 kTickDt = 1.0f / static_cast<lcu::f32>(kTicksPerSecond);

    const std::optional<lcu::u64> max_ticks = max_ticks_from_env();
    lcu::u64 tick = 0;

    while (true) {
        ++tick;

        lcu::network::Address from;
        while (auto packet = socket.try_receive(from)) {
            auto [it, inserted] = clients.try_emplace(from);
            ClientState& client = it->second;
            client.last_packet_time = std::chrono::steady_clock::now();
            if (inserted) {
                client.player.aabb = make_spawn_aabb({static_cast<lcu::f32>(spawn_column.x),
                                                        static_cast<lcu::f32>(spawn_ground_y),
                                                        static_cast<lcu::f32>(spawn_column.z)});
                // last_streamed_center/interest_set deliberately left
                // unset here - see their own declarations above. The
                // per-movement streaming loop below always treats a
                // newly-inserted client as "changed" this same tick,
                // which both computes its real interest_set and
                // loads/reloads-from-disk whatever in it isn't already
                // loaded.
                LCU_LOG_INFO("New client connection from {}", from.to_string());
            }

            const auto messages = client.connection.on_packet_received(*packet);
            for (const auto& message : messages) {
                if (const auto input = protocol::decode_player_input(message.payload)) {
                    const lcu::f32 dt = std::min(input->dt, kMaxAcceptedInputDt);
                    lcu::physics::apply_gravity(client.player, physics_config, dt);
                    lcu::physics::integrate_player(world, client.player, input->horizontal_delta, physics_config, dt,
                                                    is_solid);
                    client.last_acknowledged_sequence = input->sequence;
                    continue;
                }
                if (const auto action = protocol::decode_block_action(message.payload)) {
                    handle_block_action(from, client, *action);
                    continue;
                }
                // Neither a PlayerInput nor a BlockAction (or a malformed
                // one) - nothing else expected client->server yet.
            }

            if (inserted) {
                // The real handshake: greet a newly-seen connection with
                // the world seed and tick rate over the reliable channel.
                client.connection.send(lcu::network::Channel::ReliableOrdered,
                                        protocol::encode_welcome({kWorldSeed, static_cast<lcu::u8>(kTicksPerSecond)}));
                LCU_LOG_INFO("Sent Welcome (seed={}, tick_rate={}) to {}", kWorldSeed, kTicksPerSecond,
                             from.to_string());

                // Catch this client up on every block edit that happened
                // before it connected - without this, a late joiner's
                // World would silently disagree with everyone else's
                // (see NETWORKING.md "no world-diff catch-up", now fixed).
                for (const auto& change : block_change_history) {
                    client.connection.send(lcu::network::Channel::ReliableOrdered,
                                            protocol::encode_block_change(change));
                }
                if (!block_change_history.empty()) {
                    LCU_LOG_INFO("Replayed {} historical block change(s) to {}", block_change_history.size(),
                                 from.to_string());
                }

                // Full authoritative world sync (brief section 19 "chunk
                // streaming"): send every chunk this server has loaded, so
                // the new client's world matches this server's actual
                // (possibly edited) state instead of trusting its own
                // independently-generated terrain to happen to agree (see
                // NETWORKING.md "Chunk network streaming"). Each chunk's
                // compressed bytes are typically a few KB - too big for one
                // UDP datagram - so they're split via fragment_payload and
                // sent as a run of ChunkDataFragment messages, reassembled
                // client-side before being applied.
                //
                // Known simplification, documented (see DECISIONS.md): this
                // is a one-shot full sync on connect, not interest-managed
                // by distance (unlike kInterestRadius for entities) and not
                // re-streamed as the client moves - both loaded worlds are
                // small enough in this vertical slice for that gap not to
                // matter yet.
                lcu::usize chunks_sent = 0;
                lcu::usize fragments_sent = 0;
                for (const lcu::voxel::ChunkCoord& coord : world.loaded_chunk_coords()) {
                    const lcu::voxel::Chunk* chunk_data = world.chunk_at(coord);
                    if (chunk_data == nullptr) {
                        continue;
                    }
                    const protocol::ChunkData chunk_message{coord.x, coord.y, coord.z,
                                                              lcu::serialization::serialize_chunk_to_bytes(*chunk_data)};
                    const auto encoded = protocol::encode_chunk_data(chunk_message);
                    const auto fragments =
                        lcu::network::fragment_payload(encoded, client.next_chunk_message_id++,
                                                        lcu::network::kMaxFragmentDataSize);
                    for (const auto& fragment : fragments) {
                        client.connection.send(lcu::network::Channel::ReliableOrdered,
                                                protocol::encode_chunk_data_fragment(fragment));
                    }
                    ++chunks_sent;
                    fragments_sent += fragments.size();
                }
                LCU_LOG_INFO("Sent {} chunk(s) ({} fragment(s)) to {}", chunks_sent, fragments_sent, from.to_string());
            }
        }

        game::systems::update_ai_wander(entity_registry, ai_wander_config, ai_rng, kTickDt);

        // Real disconnect detection (Phase 20, see DECISIONS.md): a UDP
        // peer that has gone silent for kClientTimeoutSeconds is pruned
        // from `clients` before anything below reasons about who's
        // still connected - a client's `interest_set` (below) must stop
        // holding chunks open the instant it's actually gone, not on
        // whatever later tick happens to touch that coordinate again.
        bool any_client_changed = false;
        {
            const auto now = std::chrono::steady_clock::now();
            for (auto it = clients.begin(); it != clients.end();) {
                const lcu::f32 idle_seconds = std::chrono::duration<lcu::f32>(now - it->second.last_packet_time).count();
                if (idle_seconds > kClientTimeoutSeconds) {
                    LCU_LOG_INFO("Client {} timed out after {:.1f}s of silence, disconnecting", it->first.to_string(),
                                 idle_seconds);
                    it = clients.erase(it);
                    any_client_changed = true;
                } else {
                    ++it;
                }
            }
        }

        // Per-movement chunk streaming (brief section 19/22, Phase 16 -
        // closes Phase 14's "connect-time-only" gap, see DECISIONS.md):
        // the server's loaded-chunk set follows any client that has
        // crossed into a new chunk column since last checked, then
        // broadcasts every newly-loaded chunk to every connected client
        // (not just the one that triggered it - anyone already
        // connected is equally missing a chunk that didn't exist yet a
        // moment ago). Also recomputes that client's `interest_set` in
        // full (Phase 20), consumed by the unload sweep below.
        std::vector<lcu::voxel::ChunkCoord> newly_loaded_chunks;
        for (auto& [addr, client] : clients) {
            const lcu::voxel::ChunkCoord current_center = chunk_coord_of_position(client.player.aabb.center());
            if (current_center == client.last_streamed_center) {
                continue;
            }
            client.last_streamed_center = current_center;
            client.interest_set = compute_interest_set(current_center);
            any_client_changed = true;
            for (const lcu::voxel::ChunkCoord& coord : client.interest_set) {
                if (world.state_of(coord) != lcu::world::ChunkLifecycleState::Unloaded) {
                    continue;
                }
                world.load_chunk(coord);  // placeholder slot - about to be overwritten below if a save exists.
                lcu::voxel::Chunk loaded_from_disk;
                const auto load_result = lcu::serialization::load_chunk_from_file(chunk_file_path(coord), loaded_from_disk);
                if (load_result == lcu::serialization::ChunkLoadResult::Ok) {
                    *world.chunk_at_mutable(coord) = loaded_from_disk;
                    LCU_LOG_INFO("Reloaded chunk ({},{},{}) from disk (not regenerated)", coord.x, coord.y, coord.z);
                } else if (load_result != lcu::serialization::ChunkLoadResult::FileNotFound) {
                    // FileNotFound is the ordinary case (nothing was ever
                    // saved here - freshly generated instead). Anything
                    // else means a real, unexpected corruption/version
                    // problem with a save this exact process wrote -
                    // worth knowing about even though the freshly
                    // regenerated pristine chunk (from world.load_chunk
                    // above) is still a safe fallback.
                    LCU_LOG_WARN("Chunk ({},{},{}) has a save file that failed to load, using regenerated terrain instead",
                                 coord.x, coord.y, coord.z);
                }
                newly_loaded_chunks.push_back(coord);
            }
        }

        // Interest-scoped chunk unloading (Phase 20, see DECISIONS.md):
        // once nobody connected still has a loaded chunk in their own
        // `interest_set`, it's safe to drop - only run this O(loaded
        // chunks) sweep on a tick where something actually could have
        // changed who needs what (a client moved, connected, or just
        // got pruned above), not unconditionally every tick.
        if (any_client_changed) {
            std::unordered_set<lcu::voxel::ChunkCoord> still_needed;
            for (const auto& [addr, client] : clients) {
                still_needed.insert(client.interest_set.begin(), client.interest_set.end());
            }
            std::vector<lcu::voxel::ChunkCoord> to_unload;
            for (const lcu::voxel::ChunkCoord& coord : world.loaded_chunk_coords()) {
                if (still_needed.find(coord) == still_needed.end()) {
                    to_unload.push_back(coord);
                }
            }
            for (const lcu::voxel::ChunkCoord& coord : to_unload) {
                if (const lcu::voxel::Chunk* chunk_to_save = world.chunk_at(coord)) {
                    // Persist first (Phase 20's real save/load trigger -
                    // see comment by chunk_save_dir above): without
                    // this, any block edit in this chunk would silently
                    // revert to pristine worldgen the moment someone
                    // wandered back into range.
                    if (lcu::serialization::save_chunk_to_file(*chunk_to_save, chunk_file_path(coord))) {
                        LCU_LOG_INFO("Saved chunk ({},{},{}) to disk before unloading", coord.x, coord.y, coord.z);
                    } else {
                        LCU_LOG_WARN("Failed to save chunk ({},{},{}) before unloading - any edits in it will be "
                                     "lost if it's ever reloaded",
                                     coord.x, coord.y, coord.z);
                    }
                }
                world.unload_chunk(coord);
            }
            if (!to_unload.empty()) {
                LCU_LOG_INFO("Unloaded {} chunk(s) no connected client still needs (total {} loaded)",
                             to_unload.size(), world.loaded_chunk_count());
            }
        }

        if (!newly_loaded_chunks.empty()) {
            LCU_LOG_INFO("Streamed {} newly-loaded chunk(s) into range (total {} loaded)", newly_loaded_chunks.size(),
                         world.loaded_chunk_count());
            for (const lcu::voxel::ChunkCoord& coord : newly_loaded_chunks) {
                const lcu::voxel::Chunk* streamed_chunk = world.chunk_at(coord);
                if (streamed_chunk == nullptr) {
                    continue;
                }
                const protocol::ChunkData chunk_message{coord.x, coord.y, coord.z,
                                                          lcu::serialization::serialize_chunk_to_bytes(*streamed_chunk)};
                const auto encoded = protocol::encode_chunk_data(chunk_message);
                for (auto& [broadcast_addr, broadcast_client] : clients) {
                    const auto fragments = lcu::network::fragment_payload(
                        encoded, broadcast_client.next_chunk_message_id++, lcu::network::kMaxFragmentDataSize);
                    for (const auto& fragment : fragments) {
                        broadcast_client.connection.send(lcu::network::Channel::ReliableOrdered,
                                                          protocol::encode_chunk_data_fragment(fragment));
                    }
                }
            }
        }

        // Snapshot every AI entity's current position once per tick -
        // shared across all clients' (interest-filtered) EntityState
        // messages below rather than re-walking the registry per client.
        std::vector<protocol::EntitySnapshot> all_entities;
        for (const lcu::ecs::EntityId& entity : entity_registry.pool_for<game::components::AIWander>().dense_entities()) {
            const auto* pos = entity_registry.get_component<game::components::Position>(entity);
            if (pos != nullptr) {
                all_entities.push_back({entity.index, pos->value});
            }
        }

        for (auto& [addr, client] : clients) {
            // A per-tick heartbeat - real, live proof the unreliable
            // path works in an actual running server loop, not just in
            // isolated tests.
            client.connection.send(
                lcu::network::Channel::UnreliableSequenced,
                protocol::encode_heartbeat(
                    {static_cast<lcu::u32>(tick), static_cast<lcu::u16>(entity_registry.entity_count())}));

            // Interest management: only entities within kInterestRadius
            // of this client's own (server-known) player position.
            const lcu::math::Vec3 client_pos = client.player.aabb.center();
            std::vector<protocol::EntitySnapshot> visible_entities;
            for (const auto& snapshot : all_entities) {
                if (lcu::math::length(snapshot.position - client_pos) <= kInterestRadius) {
                    visible_entities.push_back(snapshot);
                }
            }
            client.connection.send(lcu::network::Channel::UnreliableSequenced,
                                    protocol::encode_entity_state(visible_entities));

            if (tick % kCorrectionIntervalTicks == 0) {
                client.connection.send(
                    lcu::network::Channel::UnreliableSequenced,
                    protocol::encode_player_correction({client.last_acknowledged_sequence, client.player.aabb.min}));
            }
        }

        for (auto& [addr, client] : clients) {
            client.connection.update(kTickDt);
            for (auto& packet : client.connection.take_outgoing_packets()) {
                socket.send_to(addr, packet);
            }
        }

        if (max_ticks && tick >= *max_ticks) {
            LCU_LOG_INFO("LCU_MAX_TICKS reached ({} ticks), shutting down", tick);
            break;
        }
        std::this_thread::sleep_for(kTickDuration);
    }

    LCU_LOG_INFO("VoxelServer shut down cleanly after {} ticks, {} client connection(s) seen", tick, clients.size());
    return 0;
}
