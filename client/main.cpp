#include <chrono>
#include <cstdlib>
#include <optional>
#include <unordered_map>
#include <vector>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

#include "game/components/ai_wander.h"
#include "game/components/position.h"
#include "game/items/block_item_mapping.h"
#include "game/systems/ai_wander_system.h"
#include "game/systems/day_night_cycle.h"
#include "game/systems/replication_protocol.h"
#include "lcu/audio/audio_engine.h"
#include "lcu/audio/positional.h"
#include "lcu/audio/waveform.h"
#include "lcu/core/log.h"
#include "lcu/core/quality_profile.h"
#include "lcu/debug/frame_stats.h"
#include "lcu/ecs/registry.h"
#include "lcu/items/inventory.h"
#include "lcu/items/item_registry.h"
#include "lcu/items/recipe_registry.h"
#include "lcu/jobs/job_system.h"
#include "lcu/lighting/light_storage.h"
#include "lcu/lighting/propagation.h"
#include "lcu/network/address.h"
#include "lcu/network/connection.h"
#include "lcu/network/fragmentation.h"
#include "lcu/network/udp_socket.h"
#include "lcu/physics/collision.h"
#include "lcu/physics/raycast.h"
#include "lcu/platform/input.h"
#include "lcu/platform/window.h"
#include "lcu/player/camera.h"
#include "lcu/player/movement_input.h"
#include "lcu/replication/position_interpolator.h"
#include "lcu/replication/prediction_buffer.h"
#include "lcu/serialization/chunk_serializer.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"
#include "lcu/voxel/chunk_coord.h"
#include "lcu/voxel/greedy_mesher.h"
#include "lcu/world/world.h"
#include "lcu/world/worldgen.h"

#if defined(LCU_ENABLE_SCRIPTING)
#include "lcu/modding/event_bus.h"
#include "lcu/modding/mod_loader.h"
#include "lcu/modding/registry_bindings.h"
#include "lcu/scripting/lua_state.h"
#endif

#if defined(LCU_ENABLE_BGFX)
#include "lcu/math/mat4.h"
#include "lcu/platform/native_handle.h"
#include "lcu/rendering/chunk_mesh_upload.h"
#include "lcu/rendering/renderer.h"
#include "lcu/rendering/shader_program.h"
#include "lcu/ui/debug_overlay.h"
#endif

namespace {

// For headless verification (this sandbox has no display/GPU): if
// LCU_MAX_FRAMES is set, the loop exits after that many frames instead of
// waiting for a window-close event. Real interactive runs never set this.
std::optional<lcu::u64> max_frames_from_env() {
    const char* value = std::getenv("LCU_MAX_FRAMES");
    if (!value) {
        return std::nullopt;
    }
    return static_cast<lcu::u64>(std::strtoull(value, nullptr, 10));
}

// Fixed for now - a real game exposes this via a world-creation UI/config
// that doesn't exist yet; deterministic and reproducible either way (see
// worldgen.h).
constexpr lcu::u32 kWorldSeed = 1337;

// How far around spawn (in chunks) to keep loaded, and the vertical chunk
// range (covering worldgen's height range: kBaseHeight=32 +/-
// kHeightVariation=24 => world Y in [8,56]; edge length 16 means chunk Y
// in [0,3] covers world Y in [0,63]). Real streaming driven by the
// player's current position/view direction (brief section 22) is a later
// refinement once World::update_streaming has a moving center to react to
// every frame; this client loads a static area once at startup, sized by
// LCU_QUALITY_PROFILE (Phase 10, see lcu::core::QualityProfile) -
// defaults to Desktop, which is numerically identical to this vertical
// slice's original hardcoded 1/0/3 values.
lcu::core::ChunkLoadSettings load_settings_from_env() {
    const char* profile_name = std::getenv("LCU_QUALITY_PROFILE");
    lcu::core::QualityProfile profile = lcu::core::QualityProfile::Desktop;
    if (profile_name != nullptr) {
        profile = lcu::core::parse_quality_profile(profile_name).value_or(lcu::core::QualityProfile::Desktop);
    }
    return lcu::core::chunk_load_settings_for(profile);
}

constexpr lcu::f32 kPlayerHalfWidth = 0.3f;   // 0.6-block-wide AABB, Minecraft-like
constexpr lcu::f32 kPlayerHeight = 1.8f;
constexpr lcu::f32 kEyeHeight = 1.62f;
constexpr lcu::f32 kMoveSpeed = 4.3f;    // blocks/s
constexpr lcu::f32 kLookSpeed = 2.0f;    // radians/s, arrow-key look (see platform/input.h)
constexpr lcu::f32 kInteractRange = 6.0f;

// Headless verification hook (this sandbox has no real keyboard/mouse
// input): if LCU_VERIFY_BREAK_PLACE is set, synthesizes an Interact press
// at frame kVerifyBreakFrame and a PlaceBlock press at kVerifyPlaceFrame.
// This drives the exact same edge-detected InputState path a real key
// press would - a real exercise of the mutate-world -> remesh ->
// re-upload pipeline, not a mock of it.
constexpr lcu::u64 kVerifyBreakFrame = 3;
constexpr lcu::u64 kVerifyPlaceFrame = 6;
// Between the break and place frames above: exercises the real
// CycleHotbar selection path (Phase 21) end to end, so the same hook
// proves PlaceBlock now places whatever's selected, not just the
// hardcoded game:stone default - see "Hotbar item selection" below.
constexpr lcu::u64 kVerifyCycleHotbarFrame = 5;

// A second, independent headless hook (LCU_VERIFY_CRAFT, Phase 23):
// breaks the grass block the player spawns on, then the dirt block
// beneath it (two real Interact presses, an edge each), then presses
// Craft - exercising the real quick-craft path end to end (see
// "Quick-craft" below). Kept separate from LCU_VERIFY_BREAK_PLACE
// above (different frame numbers, not meant to run in the same
// process) since the two exercise unrelated inventory states.
// Real elapsed-time gates, not frame numbers, for the same reason
// LCU_VERIFY_MOVE_SECONDS (Phase 16) uses wall-clock time: this main
// loop is unthrottled and can run many thousands of iterations before
// a real network round trip completes. In networked mode a break
// doesn't mutate this client's own World until the server's
// BlockChange broadcast round-trips back (block edits are never
// client-predicted - see DECISIONS.md), so the second break's raycast
// needs the first break's round trip to have genuinely settled in real
// time, or it would still see the old (unbroken) grass block and
// double-request breaking the same position - confirmed by an earlier,
// frame-count-gated version of this hook actually hitting exactly that
// race in a real networked run. Single-player mutates instantly, so
// these delays cost it nothing but a bit of wall-clock time.
constexpr lcu::f32 kVerifyCraftSecondBreakDelaySeconds = 1.0f;
constexpr lcu::f32 kVerifyCraftFirstCraftDelaySeconds = 1.2f;
// A second Craft press after the first one succeeds: by now the player
// holds only game:compost (grass/dirt were fully consumed) - a single
// distinct item type matches no registered recipe, so this exercises
// the real rejection path ("No recipe matches...") in the same run,
// not just the match path.
constexpr lcu::f32 kVerifyCraftRejectDelaySeconds = 1.5f;

// Hotbar-sized (Minecraft-like); the rest of a real inventory (a
// separate main storage grid, armor slots, ...) has no consumer yet -
// nothing reads/writes them - so isn't built speculatively (brief
// section 98).
constexpr lcu::usize kInventorySlotCount = 9;

// A handful of wandering AI entities near spawn - a real (if minimal)
// consumer of engine/ecs and game/systems::update_ai_wander, not just
// unit tests (brief section 60). Fixed seed for a deterministic,
// reproducible headless run.
constexpr int kAiEntityCount = 3;
constexpr lcu::u32 kAiRngSeed = 20260909;

// Arbitrary (see DayNightCycle's own doc comment) - short enough that a
// short headless verification run can actually observe the sky light
// scale change across a handful of frames.
constexpr lcu::f32 kDayLengthSeconds = 120.0f;

// If LCU_CONNECT_PORT is set, VoxelClient connects to a VoxelServer on
// 127.0.0.1:<port> at startup (brief section 64/Phase 8) instead of
// running fully single-player/local. Only loopback IPv4 is supported
// today - there is no hostname/IP-string parser in engine/network yet
// (see DECISIONS.md); real connect-to-a-remote-host UI/config is later
// work once there's an actual server browser or "join by address"
// screen to drive it.

lcu::physics::AABB make_player_aabb(lcu::math::Vec3 feet_position) {
    return lcu::physics::AABB{
        {feet_position.x - kPlayerHalfWidth, feet_position.y, feet_position.z - kPlayerHalfWidth},
        {feet_position.x + kPlayerHalfWidth, feet_position.y + kPlayerHeight, feet_position.z + kPlayerHalfWidth},
    };
}

// A block mutation at a chunk-boundary local coordinate can uncover or
// hide a face in the *adjacent* chunk's greedy mesh too (that chunk's own
// mesh was built treating the neighbor as solid/air based on its state at
// the time). Returns which loaded neighbors (if any) also need
// remeshing - up to 3 at a chunk corner.
std::vector<lcu::voxel::ChunkCoord> neighbors_sharing_boundary(lcu::voxel::ChunkCoord coord,
                                                                 lcu::voxel::LocalBlockCoord local) {
    std::vector<lcu::voxel::ChunkCoord> neighbors;
    constexpr lcu::u32 kMax = lcu::voxel::Chunk::kEdgeLength - 1;
    if (local.x == 0) {
        neighbors.push_back({coord.x - 1, coord.y, coord.z});
    }
    if (local.x == kMax) {
        neighbors.push_back({coord.x + 1, coord.y, coord.z});
    }
    if (local.y == 0) {
        neighbors.push_back({coord.x, coord.y - 1, coord.z});
    }
    if (local.y == kMax) {
        neighbors.push_back({coord.x, coord.y + 1, coord.z});
    }
    if (local.z == 0) {
        neighbors.push_back({coord.x, coord.y, coord.z - 1});
    }
    if (local.z == kMax) {
        neighbors.push_back({coord.x, coord.y, coord.z + 1});
    }
    return neighbors;
}

}  // namespace

int main() {
    LCU_LOG_INFO("LiveCraftUltimate client starting (Phase 4: world -> player -> camera -> raycast -> break/place)");

    lcu::platform::WindowDesc desc;
    desc.title = "LiveCraftUltimate";
    desc.width = 1280;
    desc.height = 720;

    lcu::platform::Window window(desc);

#if defined(LCU_ENABLE_BGFX)
    lcu::rendering::RendererDesc renderer_desc;
    renderer_desc.window_handle = lcu::platform::get_native_window_handle(window);
    renderer_desc.width = static_cast<lcu::u32>(window.width());
    renderer_desc.height = static_cast<lcu::u32>(window.height());
    // LCU_FORCE_HEADLESS_RENDERER lets CI/sandboxes without a GPU force
    // bgfx's Noop backend explicitly rather than relying on a null native
    // handle (belt-and-suspenders; a real display normally makes this
    // unnecessary since get_native_window_handle already falls back).
    renderer_desc.force_headless = std::getenv("LCU_FORCE_HEADLESS_RENDERER") != nullptr;

    lcu::rendering::Renderer renderer;
    if (!renderer.init(renderer_desc)) {
        LCU_LOG_ERROR("Renderer init failed, exiting");
        return 1;
    }
#endif

    lcu::voxel::BlockRegistry block_registry;
    lcu::voxel::BlockDefinition stone_def;
    stone_def.namespaced_id = "game:stone";
    stone_def.display_name = "Stone";
    stone_def.is_transparent = false;
    stone_def.has_collision = true;
    const lcu::voxel::BlockId stone_id = block_registry.register_block(stone_def);

    // Surface/subsurface terrain content (brief section 21) - a real
    // grass-over-dirt-over-stone column instead of a single block type
    // filling everything below the terrain height (see
    // lcu::world::worldgen::generate_terrain_chunk below and
    // DECISIONS.md). Both breakable/collidable/opaque like stone; no
    // item mapping exists for either yet (only "game:stone" has one -
    // see the break/place handling below), so breaking one currently
    // removes it without granting an item, same as any other
    // not-yet-item-backed block.
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

    // Block-break's first real item consumer (brief section 55): the
    // item a broken "game:stone" block hands the player. Item drops go
    // straight into the inventory rather than spawning a physical
    // dropped-item world entity - that needs entities to exist first
    // (Phase 6) - see DECISIONS.md.
    lcu::items::ItemRegistry item_registry;
    lcu::items::ItemDefinition stone_item_def;
    stone_item_def.namespaced_id = "game:stone";
    stone_item_def.display_name = "Stone";
    stone_item_def.max_stack_size = 64;
    const lcu::items::ItemId stone_item_id = item_registry.register_item(stone_item_def);

    // Phase 17's grass/dirt terrain content gets the same direct 1:1
    // block->item mapping stone already has (see DECISIONS.md "Phase
    // 17"), not a shared/loot-table drop - breaking game:grass yields
    // game:grass, breaking game:dirt yields game:dirt. Placing either is
    // now wired up too (Phase 21, see "Hotbar item selection" below) -
    // CycleHotbar picks which of the three PlaceBlock places next.
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

    // First crafted-only item (Phase 23, closing brief section 55's
    // "no crafting-grid caller anywhere" gap): game:compost has no
    // corresponding block - it exists purely as RecipeRegistry's first
    // real content, not obtainable by breaking anything. Combining the
    // two organic surface materials (grass + dirt) into compost is a
    // real, if simple, recipe, not a placeholder pairing.
    lcu::items::ItemDefinition compost_item_def;
    compost_item_def.namespaced_id = "game:compost";
    compost_item_def.display_name = "Compost";
    compost_item_def.max_stack_size = 64;
    const lcu::items::ItemId compost_item_id = item_registry.register_item(compost_item_def);

    lcu::items::Inventory player_inventory(kInventorySlotCount);

    // Crafting (Phase 23, closing RecipeRegistry's long-standing "no
    // crafting-grid caller anywhere" gap - Phase 5 built and unit
    // tested it, nothing ever called it). One real shapeless recipe:
    // 1 game:grass + 1 game:dirt -> 1 game:compost. Purely client-side
    // local inventory bookkeeping, same as item pickup itself
    // (DECISIONS.md "Item pickup/consumption stays client-authoritative")
    // - crafting never touches the World or needs server validation, so
    // it behaves identically in single-player and networked mode with
    // no protocol involvement.
    lcu::items::RecipeRegistry recipe_registry;
    recipe_registry.add_shapeless({{grass_item_id, dirt_item_id}, {compost_item_id, 1}});

    // Hotbar item selection (Phase 21, closing Phase 18/19's remaining
    // honest gap): PlaceBlock used to always place game:stone regardless
    // of what the player actually held, since there was no way to choose
    // otherwise. This is a real, minimal selection mechanism - a plain
    // index cycled by the new CycleHotbar action - not a graphical hotbar
    // (no on-screen slot rendering/highlight exists yet, needs
    // engine/ui's texture-atlas work first - see DECISIONS.md). Order
    // matches every other block/item list in this file (stone, grass,
    // dirt); index 0 (stone) is the default, preserving pre-Phase-21
    // behavior for anyone who never presses CycleHotbar.
    struct PlaceableItem {
        lcu::voxel::BlockId block_id;
        lcu::items::ItemId item_id;
        const char* name;
    };
    const std::array<PlaceableItem, 3> placeable_items{{
        {stone_id, stone_item_id, "game:stone"},
        {grass_id, grass_item_id, "game:grass"},
        {dirt_id, dirt_item_id, "game:dirt"},
    }};
    lcu::usize selected_placeable_index = 0;

    // Data-driven block->item mapping (Phase 22, closing Phase 19's
    // remaining honest gap): replaces the hardcoded if/else chain this
    // lambda used to carry (one `if (broken_block == X)` per block,
    // Phase 17-19) with a single table populated once, right after each
    // block/item pair is registered above. Adding a new item-backed
    // block from here on is one register_pair call, not a new branch
    // here and a matching one in VoxelServer's own item_for_block - see
    // game/items/block_item_mapping.h and DECISIONS.md.
    game::items::BlockItemMapping block_item_mapping;
    block_item_mapping.register_pair(stone_id, stone_item_id);
    block_item_mapping.register_pair(grass_id, grass_item_id);
    block_item_mapping.register_pair(dirt_id, dirt_item_id);

    // Block-break's item drop (brief section 55) - still a direct 1:1
    // block->item mapping (Phase 17), not a loot-table system, just
    // data-driven now instead of hardcoded (Phase 22). Shared by both
    // the networked (optimistic, client-authoritative - see
    // DECISIONS.md) and single-player break paths below so the two
    // don't drift out of sync with each other.
    const auto grant_item_for_broken_block = [&](lcu::voxel::BlockId broken_block) {
        const lcu::items::ItemId item_id = block_item_mapping.item_for_block(broken_block);
        if (item_id == lcu::items::kNoItemId) {
            return;
        }
        const std::string& item_name = item_registry.definition_of(item_id).namespaced_id;
        const lcu::u32 leftover = player_inventory.add_item(item_registry, {item_id, 1});
        if (leftover == 0) {
            LCU_LOG_INFO("Picked up 1 {} (inventory: {})", item_name, player_inventory.count_item(item_id));
        } else {
            LCU_LOG_INFO("Inventory full, {} drop lost", item_name);
        }
    };

#if defined(LCU_ENABLE_SCRIPTING)
    // Modding stack (Phase 9, brief section 84): one Lua VM shared by every
    // loaded mod. Bound to block_registry/item_registry before mods load,
    // so a mod's register_block/register_item calls land in the same
    // registries the base game content above just populated - mod content
    // and base content are otherwise indistinguishable (brief section 52).
    lcu::scripting::LuaState mod_lua;
    lcu::modding::EventBus mod_event_bus(mod_lua);
    mod_event_bus.expose_to_lua();
    lcu::modding::bind_block_registry(mod_lua, block_registry);
    lcu::modding::bind_item_registry(mod_lua, item_registry);
    lcu::modding::ModLoader mod_loader(mod_lua);
    mod_loader.load_all("mods");
#endif

    lcu::jobs::JobSystem job_system;

    // Real, own-created audio content (Phase 12, brief section 12's
    // GPL-3.0/own-IP-only requirement) - a procedurally generated tone,
    // not a checked-in asset (see waveform.h). Failing to open a device
    // (no speakers in this sandbox, most CI) is expected and non-fatal -
    // play() below is simply a no-op when that happens.
    lcu::audio::AudioEngine audio_engine;
    audio_engine.init();
    const std::vector<lcu::f32> break_sound = lcu::audio::generate_sine_wave(220.0f, 0.08f, 44100);
    const std::vector<lcu::f32> place_sound = lcu::audio::generate_sine_wave(330.0f, 0.08f, 44100);

    namespace protocol = game::systems::protocol;
    const char* connect_port_env = std::getenv("LCU_CONNECT_PORT");
    const bool networked = connect_port_env != nullptr;

    lcu::network::UdpSocket network_socket;
    lcu::network::Connection server_connection;
    lcu::network::Address server_address{};
    // Reassembles ChunkDataFragment pieces back into full ChunkData
    // payloads (see fragmentation.h) - one instance covers this client's
    // single server connection, since concurrent fragmented messages are
    // already distinguished by message_id within it.
    lcu::network::FragmentReassembler chunk_reassembler;
    if (networked) {
        network_socket.bind(0);  // ephemeral local port - this client only ever initiates.
        server_address = lcu::network::Address::loopback(
            static_cast<lcu::u16>(std::strtoul(connect_port_env, nullptr, 10)));
        // An empty UnreliableUnordered packet is enough to make the
        // server learn this client's address (see VoxelServer's own
        // connection model in NETWORKING.md) - there's no separate
        // "connect" handshake below the application-level Welcome the
        // server sends back once it sees this.
        server_connection.send(lcu::network::Channel::UnreliableUnordered, {});
        for (auto& packet : server_connection.take_outgoing_packets()) {
            network_socket.send_to(server_address, packet);
        }
        LCU_LOG_INFO("Connecting to VoxelServer at {}", server_address.to_string());
    }

    lcu::world::World world(kWorldSeed, [&](lcu::voxel::Chunk& chunk, lcu::voxel::ChunkCoord coord) {
        lcu::world::worldgen::generate_terrain_chunk(chunk, coord, kWorldSeed, grass_id, dirt_id, stone_id);
    });

#if defined(LCU_ENABLE_BGFX)
    std::unordered_map<lcu::voxel::ChunkCoord, lcu::rendering::GpuChunkMesh> gpu_meshes;
#endif

    // Per-chunk light data (brief section 24) - CPU-side only, not
    // rendering-gated like gpu_meshes: nothing samples this into the
    // shader yet (the chunk shader is flat directional+ambient lit, no
    // per-vertex/per-voxel light lookup - see DECISIONS.md), but the
    // data itself is real and kept correct through every block edit
    // below, ready for a renderer to consume once one exists.
    std::unordered_map<lcu::voxel::ChunkCoord, lcu::lighting::Light> chunk_light;

    // Full initial light computation for one just-loaded chunk (block
    // light from any emitters, plus a straight top-down sky light pass -
    // see compute_sky_light's own doc comment for the single-chunk-scope
    // simplification this carries).
    const auto compute_initial_light = [&](lcu::voxel::ChunkCoord coord) {
        const lcu::voxel::Chunk* chunk = world.chunk_at(coord);
        if (!chunk) {
            return;
        }
        lcu::lighting::Light& light = chunk_light[coord];
        lcu::lighting::compute_block_light(*chunk, block_registry, light);
        lcu::lighting::compute_sky_light(*chunk, block_registry, light);
    };

    // Incremental local lighting update after a single block at
    // `local` (within `coord`) changed from `old_id` to `new_id` - the
    // actual point of propagate_added_block_light/unpropagate_block_light
    // existing (brief section 24 "local updates, not full recompute"):
    // this never re-floods the whole chunk, only the region the edit
    // actually affects.
    const auto update_lighting_for_edit = [&](lcu::voxel::ChunkCoord coord, lcu::voxel::LocalBlockCoord local,
                                               lcu::voxel::BlockId old_id, lcu::voxel::BlockId new_id) {
        const lcu::voxel::Chunk* chunk = world.chunk_at(coord);
        auto light_it = chunk_light.find(coord);
        if (!chunk || light_it == chunk_light.end()) {
            return;
        }
        lcu::lighting::Light& light = light_it->second;

        const lcu::u8 old_emission = block_registry.definition_of(old_id).light_emission;
        const lcu::u8 new_emission = block_registry.definition_of(new_id).light_emission;
        const bool new_is_opaque = !block_registry.definition_of(new_id).is_transparent;

        if (old_emission > 0) {
            const lcu::u8 old_level = light.block_light(local.x, local.y, local.z);
            lcu::lighting::unpropagate_block_light(*chunk, block_registry, light, local.x, local.y, local.z,
                                                    old_level);
        }

        if (new_emission > 0) {
            light.set_block_light(local.x, local.y, local.z, new_emission);
            lcu::lighting::propagate_added_block_light(*chunk, block_registry, light, local.x, local.y, local.z);
        } else if (new_is_opaque) {
            // The new block blocks light - retract whatever was there
            // before (a no-op if it was already dark).
            const lcu::u8 stale_level = light.block_light(local.x, local.y, local.z);
            if (stale_level > 0) {
                lcu::lighting::unpropagate_block_light(*chunk, block_registry, light, local.x, local.y, local.z,
                                                        stale_level);
            }
        } else {
            // The cell is now open (air, or another transparent block)
            // and wasn't a light source itself - let light flow back in
            // from whichever neighbor is currently brightest, the same
            // way removing a wall lets a hallway's existing torchlight
            // spill into the newly opened room.
            lcu::u8 best_neighbor_level = 0;
            constexpr lcu::i32 kOffsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
            for (const auto& offset : kOffsets) {
                const lcu::i32 nx = static_cast<lcu::i32>(local.x) + offset[0];
                const lcu::i32 ny = static_cast<lcu::i32>(local.y) + offset[1];
                const lcu::i32 nz = static_cast<lcu::i32>(local.z) + offset[2];
                constexpr lcu::i32 kEdge = static_cast<lcu::i32>(lcu::voxel::Chunk::kEdgeLength);
                if (nx < 0 || ny < 0 || nz < 0 || nx >= kEdge || ny >= kEdge || nz >= kEdge) {
                    continue;
                }
                best_neighbor_level = std::max(
                    best_neighbor_level,
                    light.block_light(static_cast<lcu::u32>(nx), static_cast<lcu::u32>(ny), static_cast<lcu::u32>(nz)));
            }
            if (best_neighbor_level > 1) {
                light.set_block_light(local.x, local.y, local.z, static_cast<lcu::u8>(best_neighbor_level - 1));
                lcu::lighting::propagate_added_block_light(*chunk, block_registry, light, local.x, local.y, local.z);
            }
        }

        // Sky light is genuinely local to its own (x,z) column - no
        // whole-chunk work needed here either.
        lcu::lighting::compute_sky_light_column(*chunk, block_registry, light, local.x, local.z);
    };

    // Meshes (and, under bgfx, uploads) one loaded chunk's current block
    // data. Called once per chunk at startup, and again for any chunk
    // touched by a block mutation - there is no "dirty chunk" queue yet
    // (Phase 11 territory if profiling ever shows synchronous
    // main-thread remeshing on every edit is a problem; not needed for
    // this vertical slice's edit rate).
    const auto remesh_and_upload = [&](lcu::voxel::ChunkCoord coord) {
        const lcu::voxel::Chunk* chunk = world.chunk_at(coord);
        if (!chunk) {
            return;
        }
        lcu::voxel::ChunkMesh mesh;
        const auto job = job_system.submit([&] { mesh = lcu::voxel::mesh_chunk_greedy(*chunk, block_registry); },
                                            lcu::jobs::JobPriority::High);
        job_system.wait(job);
#if defined(LCU_ENABLE_BGFX)
        if (auto it = gpu_meshes.find(coord); it != gpu_meshes.end()) {
            lcu::rendering::destroy_gpu_chunk_mesh(it->second);
            gpu_meshes.erase(it);
        }
        lcu::rendering::GpuChunkMesh gpu_mesh = lcu::rendering::upload_chunk_mesh_layer(mesh.opaque);
        if (gpu_mesh.is_valid()) {
            gpu_meshes.emplace(coord, gpu_mesh);
        }
#else
        (void)mesh;
#endif
    };

    const lcu::core::ChunkLoadSettings load_settings = load_settings_from_env();
    LCU_LOG_INFO("Loading world (seed={}) around spawn (radius_xz={}, chunk_y=[{},{}])...", kWorldSeed,
                 load_settings.radius_xz, load_settings.min_chunk_y, load_settings.max_chunk_y);
    for (lcu::i32 cx = -load_settings.radius_xz; cx <= load_settings.radius_xz; ++cx) {
        for (lcu::i32 cz = -load_settings.radius_xz; cz <= load_settings.radius_xz; ++cz) {
            for (lcu::i32 cy = load_settings.min_chunk_y; cy <= load_settings.max_chunk_y; ++cy) {
                const lcu::voxel::ChunkCoord coord{cx, cy, cz};
                world.load_chunk(coord);
                compute_initial_light(coord);
                remesh_and_upload(coord);
            }
        }
    }
    LCU_LOG_INFO("Loaded {} chunks", world.loaded_chunk_count());
    {
        // A concrete, observable confirmation that lighting actually ran
        // (not just "no crash"): a point well above the terrain surface
        // (guaranteed open air, regardless of the terrain height's exact
        // solid/air boundary convention) should read full sky light.
        const auto open_air_split = lcu::voxel::world_to_chunk_and_local(
            {0, lcu::world::worldgen::terrain_height(kWorldSeed, 0, 0) + 5, 0}, lcu::voxel::Chunk::kEdgeLength);
        if (const auto it = chunk_light.find(open_air_split.chunk); it != chunk_light.end()) {
            LCU_LOG_INFO(
                "Sky light 5 blocks above spawn column: {}",
                it->second.sky_light(open_air_split.local.x, open_air_split.local.y, open_air_split.local.z));
        }
    }

#if defined(LCU_ENABLE_BGFX)
    // Only present when LCU_BUILD_SHADER_TOOLS compiled shaders into
    // <exe_dir>/shaders/chunk (see client/CMakeLists.txt and BUILDING.md).
    // "shaders/chunk" is relative to the current working directory, which
    // every verification run in this repo has been the executable's own
    // directory - a real asset system (brief section 33) will replace
    // this raw path with an ID-based lookup once one exists.
    bgfx::ProgramHandle chunk_program = BGFX_INVALID_HANDLE;
#if defined(LCU_HAS_CHUNK_SHADERS)
    chunk_program = lcu::rendering::load_chunk_program("shaders/chunk", "chunk");
#endif
    LCU_LOG_INFO("Chunk shader program valid={}", bgfx::isValid(chunk_program));
#endif

    // --- Player: spawns resting on the terrain surface at world (0, *, 0) ---
    // terrain_height() returns the topmost *solid* block's Y (worldgen.cpp:
    // world_y <= height is solid) - the first open-air cell to stand in is
    // one above that, not terrain_height() itself (an off-by-one that would
    // otherwise spawn the player embedded in the top layer of solid ground).
    const lcu::i32 spawn_ground_y = lcu::world::worldgen::terrain_height(kWorldSeed, 0, 0) + 1;
    lcu::physics::PlayerPhysicsState player;
    player.aabb = make_player_aabb({0.0f, static_cast<lcu::f32>(spawn_ground_y), 0.0f});
    lcu::physics::PlayerPhysicsConfig physics_config;

    // Per-movement chunk streaming (brief section 22, Phase 16 - the gap
    // Phase 14's connect-time-only ChunkData sync honestly left open,
    // see DECISIONS.md): as the player crosses into a new chunk column,
    // load whatever's now newly in range around it, the same
    // generate-then-light-then-mesh sequence the initial spawn-area load
    // above already runs (world.load_chunk is idempotent - already-
    // loaded coordinates are skipped by the state_of() check before ever
    // calling it, so this stays cheap on every frame that *doesn't*
    // cross a chunk boundary). Locally generated content is a
    // placeholder exactly like the initial spawn area is - a
    // subsequently-arriving server ChunkData (see the ChunkDataFragment
    // case below) overwrites it with the authoritative version, same
    // mechanism, no new code path.
    const auto stream_chunks_around = [&](lcu::voxel::ChunkCoord center) {
        for (lcu::i32 cx = center.x - load_settings.radius_xz; cx <= center.x + load_settings.radius_xz; ++cx) {
            for (lcu::i32 cz = center.z - load_settings.radius_xz; cz <= center.z + load_settings.radius_xz; ++cz) {
                for (lcu::i32 cy = load_settings.min_chunk_y; cy <= load_settings.max_chunk_y; ++cy) {
                    const lcu::voxel::ChunkCoord coord{cx, cy, cz};
                    if (world.state_of(coord) != lcu::world::ChunkLifecycleState::Unloaded) {
                        continue;
                    }
                    world.load_chunk(coord);
                    compute_initial_light(coord);
                    remesh_and_upload(coord);
                }
            }
        }
    };
    const auto chunk_coord_of_position = [](const lcu::math::Vec3& pos) {
        const lcu::voxel::BlockWorldCoord block{static_cast<lcu::i64>(std::floor(pos.x)),
                                                 static_cast<lcu::i64>(std::floor(pos.y)),
                                                 static_cast<lcu::i64>(std::floor(pos.z))};
        return lcu::voxel::world_to_chunk_and_local(block, lcu::voxel::Chunk::kEdgeLength).chunk;
    };
    lcu::voxel::ChunkCoord last_streamed_center = chunk_coord_of_position(player.aabb.center());

    const auto is_solid = [&](lcu::voxel::BlockId id) { return block_registry.definition_of(id).has_collision; };

    lcu::player::FirstPersonCamera camera;
    // Start looking mostly straight down so the raycast this vertical
    // slice exercises has a guaranteed target (the ground right below
    // spawn) without needing any look input first - see DECISIONS.md.
    camera.pitch = -1.4f;

    // A handful of wandering AI entities (brief section 60) - a real
    // engine/ecs + game/systems consumer, not just a unit test. Spawned
    // in a ring around the player's spawn column so they start on solid
    // ground (the same terrain height sampled for the player).
    //
    // When networked, AI is server-authoritative (VoxelServer runs this
    // exact same simulation - see server/main.cpp) - this client doesn't
    // also simulate it locally, it renders the server's replicated
    // positions instead (see remote_entity_interpolators below).
    lcu::ecs::Registry entity_registry;
    std::mt19937 ai_rng(kAiRngSeed);
    if (!networked) {
        for (int i = 0; i < kAiEntityCount; ++i) {
            const lcu::f32 angle = static_cast<lcu::f32>(i) * (6.28318f / static_cast<lcu::f32>(kAiEntityCount));
            const lcu::math::Vec3 spawn_pos{4.0f * std::cos(angle), static_cast<lcu::f32>(spawn_ground_y),
                                             4.0f * std::sin(angle)};
            const lcu::ecs::EntityId entity = entity_registry.create_entity();
            entity_registry.add_component<game::components::Position>(entity, {spawn_pos});
            entity_registry.add_component<game::components::AIWander>(entity, {spawn_pos, 1.5f, 0.0f});
        }
        LCU_LOG_INFO("Spawned {} wandering AI entities", entity_registry.entity_count());
    }
    game::systems::AIWanderConfig ai_wander_config;

    // One interpolator per remote AI entity (keyed by the EntityState
    // wire format's entity_index - see replication_protocol.h), fed by
    // EntityState messages below. network_clock is this client's own
    // local time base for sample timestamps (see PositionInterpolator's
    // doc comment on why it must be the receiver's clock, not the
    // sender's).
    std::unordered_map<lcu::u32, lcu::replication::PositionInterpolator> remote_entity_interpolators;
    lcu::f32 network_clock = 0.0f;

    // Client-side prediction + reconciliation (brief section 64) for the
    // local player: apply_player_input mirrors exactly what
    // VoxelServer's own PlayerInput handling does (apply_gravity then
    // integrate_player) so replaying it during reconciliation reproduces
    // what the server would have computed. Only actually used when
    // networked - in single-player mode movement is already fully local
    // and authoritative, nothing to reconcile against.
    const auto apply_player_input = [&](const lcu::physics::PlayerPhysicsState& state,
                                         const lcu::math::Vec3& horizontal_delta, lcu::f32 dt) {
        lcu::physics::PlayerPhysicsState next = state;
        lcu::physics::apply_gravity(next, physics_config, dt);
        lcu::physics::integrate_player(world, next, horizontal_delta, physics_config, dt, is_solid);
        return next;
    };
    lcu::replication::PredictionBuffer<lcu::physics::PlayerPhysicsState, lcu::math::Vec3> player_predictor(
        apply_player_input);
    lcu::u32 input_sequence = 0;

    // Day/night cycle (brief section 60) - a real, ticking, tested
    // system. Nothing renders it yet (no sky/lighting shader input - see
    // DECISIONS.md), so its output is only observed via log lines below,
    // same honesty as every other "logic verified, visuals not" system
    // in this sandbox.
    game::systems::DayNightCycle day_night_cycle(kDayLengthSeconds);

    lcu::platform::KeyboardInputBackend keyboard;
    lcu::platform::InputState input;
    lcu::platform::InputState previous_input;
    lcu::debug::FrameStats frame_stats;
#if defined(LCU_ENABLE_BGFX)
    lcu::f32 last_known_fps = 0.0f;  // updated only on frame_stats' periodic reports (see below); drives the on-screen debug overlay
#endif

    const bool verify_break_place = std::getenv("LCU_VERIFY_BREAK_PLACE") != nullptr;
    const bool verify_craft = std::getenv("LCU_VERIFY_CRAFT") != nullptr;
    int verify_craft_step = 0;
    const auto verify_craft_start = std::chrono::steady_clock::now();

    // Headless verification hook for per-movement chunk streaming
    // (Phase 16): if set, holds MoveForward down for this many real
    // (wall-clock) seconds - frame-count-indexed like
    // LCU_VERIFY_BREAK_PLACE above doesn't work here, since the main
    // loop is unthrottled (see BUILD_STATUS.md) and how much simulated
    // distance a fixed number of frames covers depends on real elapsed
    // time, not frame count. A real, sustained PlayerInput stream (not
    // a position teleport) is what's needed here specifically because
    // the *server's* own streaming trigger only reacts to positions it
    // actually received and simulated from PlayerInput - a client-only
    // position change would never move the server-authoritative
    // position this feature's server-side half depends on.
    const char* verify_move_seconds_env = std::getenv("LCU_VERIFY_MOVE_SECONDS");
    const lcu::f32 verify_move_seconds =
        verify_move_seconds_env != nullptr ? std::strtof(verify_move_seconds_env, nullptr) : 0.0f;
    const auto verify_move_start = std::chrono::steady_clock::now();

    const std::optional<lcu::u64> max_frames = max_frames_from_env();
    lcu::u64 frame = 0;
    auto last_tick = std::chrono::steady_clock::now();

    while (window.pump_events()) {
        keyboard.update(input);

        if (verify_break_place) {
            input.set_down(lcu::platform::Action::Interact, frame == kVerifyBreakFrame);
            input.set_down(lcu::platform::Action::CycleHotbar, frame == kVerifyCycleHotbarFrame);
            input.set_down(lcu::platform::Action::PlaceBlock, frame == kVerifyPlaceFrame);
        }
        if (verify_craft) {
            const lcu::f32 verify_craft_elapsed =
                std::chrono::duration<lcu::f32>(std::chrono::steady_clock::now() - verify_craft_start).count();
            bool interact_now = false;
            bool craft_now = false;
            // Each branch fires for exactly one frame (the frame its
            // threshold is first crossed) since verify_craft_step
            // advances immediately, giving InputState a clean edge each
            // time rather than holding the action down indefinitely.
            if (verify_craft_step == 0) {
                interact_now = true;  // break the grass block the player spawns on
                verify_craft_step = 1;
            } else if (verify_craft_step == 1 && verify_craft_elapsed >= kVerifyCraftSecondBreakDelaySeconds) {
                interact_now = true;  // break the dirt block beneath it
                verify_craft_step = 2;
            } else if (verify_craft_step == 2 && verify_craft_elapsed >= kVerifyCraftFirstCraftDelaySeconds) {
                craft_now = true;  // should match: 1 grass + 1 dirt held
                verify_craft_step = 3;
            } else if (verify_craft_step == 3 && verify_craft_elapsed >= kVerifyCraftRejectDelaySeconds) {
                craft_now = true;  // should reject: only compost held now
                verify_craft_step = 4;
            }
            input.set_down(lcu::platform::Action::Interact, interact_now);
            input.set_down(lcu::platform::Action::Craft, craft_now);
        }
        if (verify_move_seconds > 0.0f) {
            const lcu::f32 elapsed =
                std::chrono::duration<lcu::f32>(std::chrono::steady_clock::now() - verify_move_start).count();
            input.set_down(lcu::platform::Action::MoveForward, elapsed < verify_move_seconds);
        }

        const auto now = std::chrono::steady_clock::now();
        const lcu::f32 delta_seconds = std::chrono::duration<lcu::f32>(now - last_tick).count();
        last_tick = now;

        if (networked) {
            network_clock += delta_seconds;

            lcu::network::Address from;
            while (auto packet = network_socket.try_receive(from)) {
                for (const auto& message : server_connection.on_packet_received(*packet)) {
                    const auto type = protocol::peek_type(message.payload);
                    if (!type) {
                        continue;
                    }
                    switch (*type) {
                        case protocol::MessageType::Welcome: {
                            if (const auto welcome = protocol::decode_welcome(message.payload)) {
                                // Logged, not yet acted on - this client
                                // still generates its own world from the
                                // compile-time kWorldSeed rather than
                                // waiting on this round-trip before
                                // generating anything. Both happen to be
                                // 1337 today (see DECISIONS.md); a real
                                // "use the server's authoritative seed"
                                // needs world generation deferred until
                                // after this message arrives, a bigger
                                // structural change than this phase's
                                // scope.
                                LCU_LOG_INFO("Received Welcome: world_seed={} tick_rate={}", welcome->world_seed,
                                             welcome->tick_rate);
                            }
                            break;
                        }
                        case protocol::MessageType::EntityState: {
                            if (const auto entities = protocol::decode_entity_state(message.payload)) {
                                for (const auto& snapshot : *entities) {
                                    remote_entity_interpolators[snapshot.entity_index].add_sample(
                                        network_clock, snapshot.position);
                                }
                            }
                            break;
                        }
                        case protocol::MessageType::PlayerCorrection: {
                            if (const auto correction = protocol::decode_player_correction(message.payload)) {
                                // The server sends its player AABB's min
                                // corner directly (see server/main.cpp),
                                // the same point client-side code already
                                // treats as the AABB's identity - not a
                                // "feet center" point like
                                // make_player_aabb() takes, so it's
                                // reconstructed directly here instead of
                                // going through that helper.
                                lcu::physics::PlayerPhysicsState authoritative = player;
                                authoritative.aabb.min = correction->position;
                                authoritative.aabb.max = {correction->position.x + kPlayerHalfWidth * 2.0f,
                                                           correction->position.y + kPlayerHeight,
                                                           correction->position.z + kPlayerHalfWidth * 2.0f};
                                player = player_predictor.reconcile(authoritative, correction->acknowledged_sequence);
                            }
                            break;
                        }
                        case protocol::MessageType::BlockChange: {
                            if (const auto change = protocol::decode_block_change(message.payload)) {
                                const lcu::voxel::BlockWorldCoord world_pos{change->x, change->y, change->z};
                                const auto split =
                                    lcu::voxel::world_to_chunk_and_local(world_pos, lcu::voxel::Chunk::kEdgeLength);
                                if (lcu::voxel::Chunk* target = world.chunk_at_mutable(split.chunk)) {
                                    const lcu::voxel::BlockId old_id =
                                        target->block_at(split.local.x, split.local.y, split.local.z);
                                    const lcu::voxel::BlockId new_id = change->block_id;
                                    if (old_id != new_id) {
                                        target->set_block(split.local.x, split.local.y, split.local.z, new_id);
                                        update_lighting_for_edit(split.chunk, split.local, old_id, new_id);
                                        remesh_and_upload(split.chunk);
                                        for (const lcu::voxel::ChunkCoord& neighbor :
                                             neighbors_sharing_boundary(split.chunk, split.local)) {
                                            remesh_and_upload(neighbor);
                                        }
#if defined(LCU_ENABLE_SCRIPTING)
                                        if (new_id == lcu::voxel::kAirBlockId) {
                                            mod_event_bus.emit_block_broken(change->x, change->y, change->z, old_id);
                                        }
#endif
                                        const lcu::math::Vec3 block_center{static_cast<lcu::f32>(change->x) + 0.5f,
                                                                             static_cast<lcu::f32>(change->y) + 0.5f,
                                                                             static_cast<lcu::f32>(change->z) + 0.5f};
                                        const lcu::audio::StereoGain pan = lcu::audio::compute_stereo_pan(
                                            camera.position, camera.right(), block_center);
                                        const lcu::f32 attenuation = lcu::audio::distance_attenuation(
                                            lcu::math::length(block_center - camera.position), 16.0f);
                                        const auto& sound =
                                            new_id == lcu::voxel::kAirBlockId ? break_sound : place_sound;
                                        audio_engine.play(sound, {pan.left * attenuation, pan.right * attenuation});
                                        LCU_LOG_INFO("Applied server BlockChange at world ({}, {}, {}): block_id={}",
                                                     change->x, change->y, change->z, new_id);
                                    }
                                } else {
                                    LCU_LOG_DEBUG("BlockChange target's chunk isn't loaded, ignoring");
                                }
                            }
                            break;
                        }
                        case protocol::MessageType::ChunkDataFragment: {
                            if (const auto fragment_bytes = protocol::decode_chunk_data_fragment(message.payload)) {
                                const auto reassembled = chunk_reassembler.add_fragment(*fragment_bytes);
                                if (!reassembled.has_value()) {
                                    break;  // still waiting on more fragments of this ChunkData.
                                }
                                const auto chunk_message = protocol::decode_chunk_data(*reassembled);
                                if (!chunk_message.has_value()) {
                                    LCU_LOG_WARN("Discarding malformed reassembled ChunkData");
                                    break;
                                }
                                const lcu::voxel::ChunkCoord coord{chunk_message->chunk_x, chunk_message->chunk_y,
                                                                    chunk_message->chunk_z};
                                // A chunk the server has loaded but this
                                // client hasn't streamed to locally yet
                                // (Phase 16: the server's loaded set can
                                // now grow from another client's movement,
                                // or from this client's own movement
                                // arriving here before its local
                                // stream_chunks_around trigger did) still
                                // needs a real chunk slot to overwrite -
                                // load_chunk's placeholder content is
                                // about to be replaced below regardless.
                                if (world.state_of(coord) == lcu::world::ChunkLifecycleState::Unloaded) {
                                    world.load_chunk(coord);
                                }
                                lcu::voxel::Chunk* target = world.chunk_at_mutable(coord);
                                if (target == nullptr) {
                                    LCU_LOG_WARN("ChunkData target ({},{},{}) couldn't be loaded locally, ignoring",
                                                 coord.x, coord.y, coord.z);
                                    break;
                                }
                                lcu::voxel::Chunk server_chunk;
                                const auto result =
                                    lcu::serialization::deserialize_chunk_from_bytes(chunk_message->compressed_bytes,
                                                                                      server_chunk);
                                if (result != lcu::serialization::ChunkLoadResult::Ok) {
                                    LCU_LOG_WARN("Discarding corrupt ChunkData for ({},{},{})", coord.x, coord.y,
                                                  coord.z);
                                    break;
                                }
                                // Full authoritative overwrite - this
                                // client's own locally-generated terrain
                                // for `coord` (deterministic, so usually
                                // already identical) is replaced with the
                                // server's actual chunk, including any
                                // edits applied before this client
                                // connected.
                                *target = server_chunk;
                                compute_initial_light(coord);
                                remesh_and_upload(coord);
                                for (const lcu::voxel::ChunkCoord& neighbor :
                                     {lcu::voxel::ChunkCoord{coord.x - 1, coord.y, coord.z},
                                      lcu::voxel::ChunkCoord{coord.x + 1, coord.y, coord.z},
                                      lcu::voxel::ChunkCoord{coord.x, coord.y - 1, coord.z},
                                      lcu::voxel::ChunkCoord{coord.x, coord.y + 1, coord.z},
                                      lcu::voxel::ChunkCoord{coord.x, coord.y, coord.z - 1},
                                      lcu::voxel::ChunkCoord{coord.x, coord.y, coord.z + 1}}) {
                                    remesh_and_upload(neighbor);
                                }
                                LCU_LOG_INFO("Applied server ChunkData for chunk ({}, {}, {})", coord.x, coord.y,
                                             coord.z);
                            }
                            break;
                        }
                        case protocol::MessageType::InventoryUpdate: {
                            if (const auto update = protocol::decode_inventory_update(message.payload)) {
                                // Reconciles this client's own optimistic,
                                // client-authoritative item guess (fired
                                // at request-send time - see
                                // DECISIONS.md) against the server's real
                                // outcome, the same way PlayerCorrection
                                // reconciles predicted movement: only
                                // visibly changes anything when the
                                // optimistic guess and the server's
                                // authoritative count actually disagree
                                // (a rejected BlockAction, or a race).
                                const lcu::items::ItemId item_id = update->item_id;
                                const lcu::u32 current = player_inventory.count_item(item_id);
                                if (update->count > current) {
                                    player_inventory.add_item(item_registry, {item_id, update->count - current});
                                } else if (update->count < current) {
                                    player_inventory.remove_item(item_id, current - update->count);
                                }
                                if (update->count != current) {
                                    LCU_LOG_INFO(
                                        "Reconciled inventory item {} to authoritative count {} (was {})", item_id,
                                        update->count, current);
                                }
                            }
                            break;
                        }
                        case protocol::MessageType::Heartbeat:
                        case protocol::MessageType::PlayerInput:
                        case protocol::MessageType::BlockAction:
                        case protocol::MessageType::ChunkData:
                            break;  // Heartbeat: nothing to act on. PlayerInput/BlockAction/ChunkData:
                                     // server->client never sends these directly (ChunkData only ever
                                     // arrives fragmented, see ChunkDataFragment above).
                    }
                }
            }
        } else {
            game::systems::update_ai_wander(entity_registry, ai_wander_config, ai_rng, delta_seconds);
        }
        day_night_cycle.update(delta_seconds);

        if (input.is_down(lcu::platform::Action::LookLeft)) {
            camera.add_yaw_pitch(-kLookSpeed * delta_seconds, 0.0f);
        }
        if (input.is_down(lcu::platform::Action::LookRight)) {
            camera.add_yaw_pitch(kLookSpeed * delta_seconds, 0.0f);
        }
        if (input.is_down(lcu::platform::Action::LookUp)) {
            camera.add_yaw_pitch(0.0f, kLookSpeed * delta_seconds);
        }
        if (input.is_down(lcu::platform::Action::LookDown)) {
            camera.add_yaw_pitch(0.0f, -kLookSpeed * delta_seconds);
        }

        const lcu::math::Vec3 move_dir = lcu::player::movement_direction_from_input(input, camera);
        const lcu::math::Vec3 horizontal_delta = move_dir * (kMoveSpeed * delta_seconds);

        if (input.is_down(lcu::platform::Action::Jump)) {
            lcu::physics::try_jump(player, physics_config);
        }

        if (networked) {
            // Predict locally (so movement feels instant, not delayed by
            // a round-trip to the server) and record the input for
            // later reconciliation against the server's PlayerCorrection
            // - see the PlayerCorrection handling above.
            ++input_sequence;
            player = player_predictor.predict_and_record(player, input_sequence, horizontal_delta, delta_seconds);
            server_connection.send(lcu::network::Channel::UnreliableSequenced,
                                    protocol::encode_player_input({input_sequence, horizontal_delta, delta_seconds}));
        } else {
            lcu::physics::apply_gravity(player, physics_config, delta_seconds);
            lcu::physics::integrate_player(world, player, horizontal_delta, physics_config, delta_seconds, is_solid);
        }

        camera.position = {player.aabb.center().x, player.aabb.min.y + kEyeHeight, player.aabb.center().z};

        const lcu::voxel::ChunkCoord current_center = chunk_coord_of_position(player.aabb.center());
        if (current_center != last_streamed_center) {
            stream_chunks_around(current_center);
            last_streamed_center = current_center;
        }

        const auto hit = lcu::physics::raycast(world, camera.position, camera.forward(), kInteractRange, is_solid);

        const bool interact_pressed = input.is_down(lcu::platform::Action::Interact) &&
                                       !previous_input.is_down(lcu::platform::Action::Interact);
        const bool place_pressed = input.is_down(lcu::platform::Action::PlaceBlock) &&
                                    !previous_input.is_down(lcu::platform::Action::PlaceBlock);
        const bool cycle_hotbar_pressed = input.is_down(lcu::platform::Action::CycleHotbar) &&
                                           !previous_input.is_down(lcu::platform::Action::CycleHotbar);
        const bool craft_pressed =
            input.is_down(lcu::platform::Action::Craft) && !previous_input.is_down(lcu::platform::Action::Craft);

        if (cycle_hotbar_pressed) {
            selected_placeable_index = (selected_placeable_index + 1) % placeable_items.size();
            LCU_LOG_INFO("Selected placeable item: {}", placeable_items[selected_placeable_index].name);
        }

        if (craft_pressed) {
            // Quick-craft (Phase 23): auto-assembles a query grid from
            // one of each *distinct* item type currently held (dedup by
            // slot scan), then asks RecipeRegistry for a real match -
            // not a graphical crafting-grid UI (no way to arrange items
            // into specific cells exists yet - see DECISIONS.md). This
            // only correctly represents a recipe needing exactly one of
            // each distinct ingredient type (true of the one recipe
            // registered above); it isn't a stand-in for a real grid
            // that could hold >1 of the same item in different cells.
            std::vector<lcu::items::ItemId> craft_grid;
            for (lcu::usize slot = 0; slot < player_inventory.slot_count(); ++slot) {
                const lcu::items::ItemId slot_item = player_inventory.slot_at(slot).item;
                if (slot_item == lcu::items::kNoItemId) {
                    continue;
                }
                if (std::find(craft_grid.begin(), craft_grid.end(), slot_item) == craft_grid.end()) {
                    craft_grid.push_back(slot_item);
                }
            }
            const lcu::items::ItemStack* result =
                recipe_registry.find_match(craft_grid, static_cast<lcu::u32>(craft_grid.size()), 1);
            if (result != nullptr) {
                // Grid contents == the matched recipe's ingredient
                // multiset exactly (matches_shapeless requires an exact
                // multiset match) - since craft_grid holds exactly 1 of
                // each distinct type by construction, consuming 1 of
                // each entry consumes exactly what the recipe required,
                // no more.
                for (lcu::items::ItemId ingredient : craft_grid) {
                    player_inventory.remove_item(ingredient, 1);
                }
                player_inventory.add_item(item_registry, *result);
                LCU_LOG_INFO("Crafted {} {} (inventory: {})", result->count,
                             item_registry.definition_of(result->item).namespaced_id,
                             player_inventory.count_item(result->item));
            } else {
                LCU_LOG_INFO("No recipe matches your held items");
            }
        }

        if (interact_pressed && hit && networked) {
            // Server-authoritative: send the request and wait for the
            // broadcast BlockChange to actually mutate this client's
            // World (see the BlockChange case below) - this client never
            // mutates its own World speculatively for a block edit the
            // way it does for movement (see DECISIONS.md "block edits
            // are not client-predicted").
            LCU_LOG_INFO("Requesting break at world ({}, {}, {})", hit->world.x, hit->world.y, hit->world.z);
            server_connection.send(lcu::network::Channel::ReliableOrdered,
                                    protocol::encode_block_action({protocol::BlockActionType::Break, hit->world.x,
                                                                    hit->world.y, hit->world.z, 0}));
            // Item pickup is client-authoritative and optimistic - it
            // happens here, at request time, not in the BlockChange
            // handler (which runs for every connected client on every
            // edit, including other players' edits, with no way to
            // tell "was this my own break"). The server independently
            // tracks the same three items (Phase 19) and reconciles
            // this optimistic guess via InventoryUpdate once its own
            // outcome is known - see DECISIONS.md.
            grant_item_for_broken_block(hit->block);
        } else if (interact_pressed && hit) {
            LCU_LOG_INFO("Breaking block at world ({}, {}, {})", hit->world.x, hit->world.y, hit->world.z);
            const auto split = lcu::voxel::world_to_chunk_and_local(hit->world, lcu::voxel::Chunk::kEdgeLength);
            if (lcu::voxel::Chunk* target = world.chunk_at_mutable(split.chunk)) {
                const lcu::voxel::BlockId old_id = target->block_at(split.local.x, split.local.y, split.local.z);
                target->set_block(split.local.x, split.local.y, split.local.z, lcu::voxel::kAirBlockId);
                update_lighting_for_edit(split.chunk, split.local, old_id, lcu::voxel::kAirBlockId);
                remesh_and_upload(split.chunk);
                for (const lcu::voxel::ChunkCoord& neighbor : neighbors_sharing_boundary(split.chunk, split.local)) {
                    remesh_and_upload(neighbor);
                }
#if defined(LCU_ENABLE_SCRIPTING)
                mod_event_bus.emit_block_broken(hit->world.x, hit->world.y, hit->world.z, old_id);
#endif
                {
                    const lcu::math::Vec3 block_center{static_cast<lcu::f32>(hit->world.x) + 0.5f,
                                                         static_cast<lcu::f32>(hit->world.y) + 0.5f,
                                                         static_cast<lcu::f32>(hit->world.z) + 0.5f};
                    const lcu::audio::StereoGain pan =
                        lcu::audio::compute_stereo_pan(camera.position, camera.right(), block_center);
                    const lcu::f32 attenuation =
                        lcu::audio::distance_attenuation(lcu::math::length(block_center - camera.position), 16.0f);
                    audio_engine.play(break_sound, {pan.left * attenuation, pan.right * attenuation});
                }
                // The broken block hands the player its item - block-break's
                // first real item consumer (see DECISIONS.md), a direct
                // 1:1 block->item mapping (stone/grass/dirt as of Phase
                // 17), not a loot-table system.
                grant_item_for_broken_block(hit->block);
            } else {
                LCU_LOG_DEBUG("Break target's chunk isn't loaded, ignoring");
            }
        }

        const PlaceableItem& selected_placeable = placeable_items[selected_placeable_index];
        if (place_pressed && hit && player_inventory.remove_item(selected_placeable.item_id, 1) == 1) {
            const lcu::voxel::BlockWorldCoord place_pos{
                hit->world.x + static_cast<lcu::i64>(hit->normal.x),
                hit->world.y + static_cast<lcu::i64>(hit->normal.y),
                hit->world.z + static_cast<lcu::i64>(hit->normal.z),
            };
            if (networked) {
                // See the interact_pressed/BlockActionType::Break branch
                // above - same server-authoritative pattern. The item is
                // still consumed client-side immediately (no server-side
                // inventory exists yet - see DECISIONS.md), so a request
                // the server ends up rejecting (e.g. the target stopped
                // being air by the time it's processed) currently isn't
                // refunded; a real inventory-sync/rejection channel is a
                // separate, larger feature.
                LCU_LOG_INFO("Requesting place {} at world ({}, {}, {}) (inventory: {})", selected_placeable.name,
                             place_pos.x, place_pos.y, place_pos.z,
                             player_inventory.count_item(selected_placeable.item_id));
                server_connection.send(lcu::network::Channel::ReliableOrdered,
                                        protocol::encode_block_action({protocol::BlockActionType::Place, place_pos.x,
                                                                        place_pos.y, place_pos.z,
                                                                        selected_placeable.block_id}));
            } else {
                LCU_LOG_INFO("Placing {} at world ({}, {}, {}) (inventory: {})", selected_placeable.name,
                             place_pos.x, place_pos.y, place_pos.z,
                             player_inventory.count_item(selected_placeable.item_id));
                const auto split = lcu::voxel::world_to_chunk_and_local(place_pos, lcu::voxel::Chunk::kEdgeLength);
                if (lcu::voxel::Chunk* target = world.chunk_at_mutable(split.chunk)) {
                    const lcu::voxel::BlockId old_id = target->block_at(split.local.x, split.local.y, split.local.z);
                    target->set_block(split.local.x, split.local.y, split.local.z, selected_placeable.block_id);
                    update_lighting_for_edit(split.chunk, split.local, old_id, selected_placeable.block_id);
                    remesh_and_upload(split.chunk);
                    for (const lcu::voxel::ChunkCoord& neighbor :
                         neighbors_sharing_boundary(split.chunk, split.local)) {
                        remesh_and_upload(neighbor);
                    }
                    {
                        const lcu::math::Vec3 block_center{static_cast<lcu::f32>(place_pos.x) + 0.5f,
                                                             static_cast<lcu::f32>(place_pos.y) + 0.5f,
                                                             static_cast<lcu::f32>(place_pos.z) + 0.5f};
                        const lcu::audio::StereoGain pan =
                            lcu::audio::compute_stereo_pan(camera.position, camera.right(), block_center);
                        const lcu::f32 attenuation =
                            lcu::audio::distance_attenuation(lcu::math::length(block_center - camera.position), 16.0f);
                        audio_engine.play(place_sound, {pan.left * attenuation, pan.right * attenuation});
                    }
                } else {
                    LCU_LOG_DEBUG("Place target's chunk isn't loaded, refunding the item");
                    player_inventory.add_item(item_registry, {selected_placeable.item_id, 1});
                }
            }
        }

        previous_input = input;

        if (networked) {
            server_connection.update(delta_seconds);
            for (auto& packet : server_connection.take_outgoing_packets()) {
                network_socket.send_to(server_address, packet);
            }
        }

#if defined(LCU_ENABLE_BGFX)
        renderer.begin_frame(0x87ceebff);
        const lcu::math::Mat4 view = camera.view_matrix();
        const lcu::f32 aspect =
            static_cast<lcu::f32>(renderer_desc.width) / static_cast<lcu::f32>(renderer_desc.height);
        const lcu::math::Mat4 proj = lcu::math::Mat4::perspective(1.0f, aspect, 0.1f, 500.0f);
        constexpr lcu::i32 kEdge = static_cast<lcu::i32>(lcu::voxel::Chunk::kEdgeLength);
        for (const auto& [coord, gpu_mesh] : gpu_meshes) {
            const lcu::math::Mat4 model = lcu::math::Mat4::translation({static_cast<lcu::f32>(coord.x * kEdge),
                                                                         static_cast<lcu::f32>(coord.y * kEdge),
                                                                         static_cast<lcu::f32>(coord.z * kEdge)});
            renderer.submit_chunk_mesh(gpu_mesh, chunk_program, model, view, proj);
        }
        lcu::ui::draw_debug_overlay(renderer, renderer_desc.width, renderer_desc.height, last_known_fps);
        renderer.end_frame();
#endif

        if (const auto report = frame_stats.update(delta_seconds)) {
            LCU_LOG_INFO("fps={:.1f} frame_ms={:.2f} total_frames={}", report->fps, report->avg_frame_ms,
                         report->frame_count);
#if defined(LCU_ENABLE_BGFX)
            last_known_fps = report->fps;
#endif
        }

        ++frame;
        if (max_frames && frame >= *max_frames) {
            LCU_LOG_INFO("LCU_MAX_FRAMES reached ({} frames), exiting", frame);
            break;
        }
    }

#if defined(LCU_ENABLE_BGFX)
    if (bgfx::isValid(chunk_program)) {
        bgfx::destroy(chunk_program);
    }
    for (auto& [coord, gpu_mesh] : gpu_meshes) {
        lcu::rendering::destroy_gpu_chunk_mesh(gpu_mesh);
    }
#endif

    LCU_LOG_INFO("Day/night: time_of_day={:.3f} sky_light_scale={:.3f}", day_night_cycle.time_of_day(),
                 day_night_cycle.sky_light_scale());
    if (networked) {
        LCU_LOG_INFO("Player position (server-reconciled): ({:.2f}, {:.2f}, {:.2f})", player.aabb.min.x,
                     player.aabb.min.y, player.aabb.min.z);
        for (const auto& [entity_index, interpolator] : remote_entity_interpolators) {
            const lcu::math::Vec3 pos = interpolator.interpolated_position(network_clock);
            LCU_LOG_INFO("Remote AI entity (index={}) interpolated position: ({:.2f}, {:.2f}, {:.2f})", entity_index,
                         pos.x, pos.y, pos.z);
        }
    } else {
        for (const lcu::ecs::EntityId& entity :
             entity_registry.pool_for<game::components::AIWander>().dense_entities()) {
            const lcu::math::Vec3 pos = entity_registry.get_component<game::components::Position>(entity)->value;
            LCU_LOG_INFO("AI entity (index={}) at ({:.2f}, {:.2f}, {:.2f})", entity.index, pos.x, pos.y, pos.z);
        }
    }

    LCU_LOG_INFO("LiveCraftUltimate client shutting down after {} frames", frame);
    return 0;
}
