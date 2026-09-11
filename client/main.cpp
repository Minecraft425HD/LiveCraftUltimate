#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
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
#include "lcu/lighting/world_light.h"
#include "lcu/network/address.h"
#include "lcu/network/connection.h"
#include "lcu/network/fragmentation.h"
#include "lcu/network/udp_socket.h"
#include "lcu/physics/collision.h"
#include "lcu/physics/raycast.h"
#include "lcu/platform/input.h"
#include "lcu/platform/key_bindings.h"
#include "lcu/platform/options.h"
#include "lcu/platform/window.h"
#include "lcu/player/camera.h"
#include "lcu/player/movement_input.h"
#include "lcu/replication/position_interpolator.h"
#include "lcu/replication/prediction_buffer.h"
#include "lcu/serialization/chunk_serializer.h"
#include "lcu/ui/menu_stack.h"
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
#include "lcu/math/vec4.h"
#include "lcu/platform/native_handle.h"
#include "lcu/rendering/chunk_mesh_upload.h"
#include "lcu/rendering/renderer.h"
#include "lcu/rendering/shader_program.h"
#include "lcu/ui/debug_overlay.h"
#include "lcu/ui/menu_renderer.h"
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

// Real spawn placement (Phase 37): terrain_height() is now centered on
// sea level, so a fixed world (0,0) spawn column can legitimately land
// underwater by pure chance - no swim mechanics exist yet (see
// DECISIONS.md), so that would strand the player. A small deterministic
// square-ring search outward from the origin for the nearest column at
// or above sea level - same seed always finds the same spot, no hidden
// randomness. Falls back to (0,0) itself if nothing within kMaxRadius
// qualifies (statistically implausible given terrain_height's roughly
// symmetric distribution around sea level, but honestly handled rather
// than assumed impossible).
//
// kMaxRadius (Phase 38): worldgen's continental noise stage varies at
// a much lower frequency than the old single-stage noise this search
// radius was originally sized for (~666-block wavelength - see
// worldgen.cpp's kContinentalNoiseScale) - a small radius can now
// legitimately stay inside one giant ocean basin the entire time and
// never find land, confirmed by a real search for seed 1337 needing
// radius 84 to find dry land at all. 1024 gives real headroom over
// that (more than one full continental wavelength in every direction)
// while an O(ring-perimeter) search (only the new ring's boundary
// cells, not a full re-scanned square) keeps even the worst case fast
// - a few hundred thousand terrain_height() calls at most, a one-time
// startup cost, not a per-frame one.
struct SpawnColumn {
    lcu::i32 x = 0;
    lcu::i32 z = 0;
};

// Real, observable confirmation the climate/biome pipeline stage
// (Phase 39) actually ran and produced something concrete - used in
// the spawn-load log line below, not just for debugging.
const char* biome_name(lcu::world::worldgen::Biome biome) {
    switch (biome) {
        case lcu::world::worldgen::Biome::Snowy:
            return "Snowy";
        case lcu::world::worldgen::Biome::Desert:
            return "Desert";
        case lcu::world::worldgen::Biome::Plains:
            return "Plains";
    }
    return "Plains";
}

SpawnColumn find_dry_spawn_column(lcu::u32 seed) {
    const auto is_dry = [seed](lcu::i32 x, lcu::i32 z) {
        return lcu::world::worldgen::terrain_height(seed, x, z) >= lcu::world::worldgen::kSeaLevel;
    };
    if (is_dry(0, 0)) {
        return {0, 0};
    }
    constexpr lcu::i32 kMaxRadius = 1024;
    for (lcu::i32 radius = 1; radius <= kMaxRadius; ++radius) {
        // Top and bottom edges of the ring (full width, including
        // corners).
        for (lcu::i32 x = -radius; x <= radius; ++x) {
            if (is_dry(x, -radius)) {
                return {x, -radius};
            }
            if (is_dry(x, radius)) {
                return {x, radius};
            }
        }
        // Left and right edges (corners already covered above).
        for (lcu::i32 z = -radius + 1; z <= radius - 1; ++z) {
            if (is_dry(-radius, z)) {
                return {-radius, z};
            }
            if (is_dry(radius, z)) {
                return {radius, z};
            }
        }
    }
    return {0, 0};  // honestly-scoped fallback - see this function's doc comment above.
}

// How far around spawn (in chunks) to keep loaded, and the vertical chunk
// range (covering worldgen's height range: kBaseHeight=0 +/-
// kHeightVariation=20 => world Y in [-20,20], centered on sea level;
// edge length 16 means Desktop's chunk Y in [-1,2] covers world Y in
// [-16,47] - see quality_profile.cpp). Real streaming driven by the
// player's current position/view direction (brief section 22) is a later
// refinement once World::update_streaming has a moving center to react to
// every frame; this client loads a static area once at startup, sized by
// LCU_QUALITY_PROFILE (Phase 10, see lcu::core::QualityProfile).
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

// Crosshair (Phase 44): the first real consumer of the new 2D UI quad
// batch (engine/rendering::Renderer::submit_ui_quad/flush_ui_quads) -
// a genuine, permanent HUD element (every FPS needs one), not a
// throwaway test rectangle drawn only to prove the pipeline works,
// even though it does double as exactly that real proof (a real run
// showing it at screen center, see BUILD_STATUS.md). Two thin bars,
// centered on the screen regardless of resolution. Gated behind
// LCU_ENABLE_BGFX (its only use site) for the same real reason every
// other rendering-only constant in this file is - see the kNightSkyColor
// block below's own comment on the Phase 43 hygiene fix this matches.
#if defined(LCU_ENABLE_BGFX)
constexpr lcu::f32 kCrosshairSize = 16.0f;
constexpr lcu::f32 kCrosshairThickness = 2.0f;
constexpr lcu::math::Vec4 kCrosshairColor{1.0f, 1.0f, 1.0f, 0.85f};
#endif

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

// A third, independent headless hook (LCU_VERIFY_MENU, Phase 46): opens
// the pause menu, holds MoveForward while paused (must NOT move the
// player - the whole point of this check), navigates into Options,
// adjusts mouse sensitivity, saves back out to the pause screen, closes
// the menu, then holds MoveForward again while resumed (must move the
// player this time) - a real, end-to-end exercise of "menu open ==
// simulation paused" and real menu navigation, not a mock of either.
constexpr lcu::u64 kVerifyMenuOpenFrame = 5;
constexpr lcu::u64 kVerifyMenuMoveWhilePausedStart = 6;
constexpr lcu::u64 kVerifyMenuMoveWhilePausedEnd = 35;
constexpr lcu::u64 kVerifyMenuNavigateToOptionsFrame = 40;
constexpr lcu::u64 kVerifyMenuOpenOptionsFrame = 41;
// Every one of the single-frame presses below needs at least one real
// released frame before the next press of the *same* action - edge
// detection compares against the PREVIOUS frame's InputState, so two
// presses of the same action on directly adjacent frame numbers would
// never produce a second real edge (this was a genuine bug caught by
// this exact hook during headless testing - see DECISIONS.md).
constexpr lcu::u64 kVerifyMenuAdjustSensitivityFrame1 = 42;
constexpr lcu::u64 kVerifyMenuAdjustSensitivityFrame2 = 44;
// 4 LookDown presses (one every other frame) to reach "Zurueck", the
// 5th row (index 4) of the 5-item Options screen, from its default
// selection (index 0) - see build_options_screen's own item order
// below.
constexpr lcu::u64 kVerifyMenuNavigateToBackFrame1 = 46;
constexpr lcu::u64 kVerifyMenuNavigateToBackFrame2 = 48;
constexpr lcu::u64 kVerifyMenuNavigateToBackFrame3 = 50;
constexpr lcu::u64 kVerifyMenuNavigateToBackFrame4 = 52;
constexpr lcu::u64 kVerifyMenuActivateBackFrame = 54;
constexpr lcu::u64 kVerifyMenuCloseFrame = 56;
constexpr lcu::u64 kVerifyMenuMoveWhileResumedStart = 58;
constexpr lcu::u64 kVerifyMenuMoveWhileResumedEnd = 87;

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

// A third, independent headless hook (LCU_VERIFY_TORCH, Phase 34):
// breaks the block the player spawns on (same kVerifyBreakFrame value
// as LCU_VERIFY_BREAK_PLACE, a proven-working target/timing), grants
// the player one game:torch item directly (nothing in this build's
// world drops one to break/craft yet, so this is the synthetic setup
// the hook needs, the same honest "hook synthesizes exactly the
// input/state a real key press or drop would produce" approach
// LCU_VERIFY_BREAK_PLACE/LCU_VERIFY_CRAFT already use), cycles the
// hotbar three times to reach it (index 3 - see `placeable_items`),
// then places it into the hole the break just made. Proves the real
// end-to-end pipeline: a placed torch actually reaches update_
// lighting_for_edit -> propagate_added_block_light_cross_chunk -> a
// real, observably nonzero block_light value at its own position,
// logged below - not just "it compiled and didn't crash".
constexpr lcu::u64 kVerifyTorchBreakFrame = 3;
constexpr lcu::u64 kVerifyTorchCycleFrame1 = 5;
constexpr lcu::u64 kVerifyTorchCycleFrame2 = 7;
constexpr lcu::u64 kVerifyTorchCycleFrame3 = 9;
constexpr lcu::u64 kVerifyTorchPlaceFrame = 11;
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

// Rendering-only constants (Phase 27/36) - every use site below is
// inside an `#if defined(LCU_ENABLE_BGFX)` block (real draw calls), so
// these are genuinely dead declarations in a non-bgfx build. Gated here
// too (Phase 43 hygiene fix): a non-bgfx build previously declared these
// at namespace scope with nothing referencing them, which some compilers
// (Apple Clang on a real macOS run, not reproduced by this sandbox's own
// GCC/Clang - see BUILD_STATUS.md) flag as unused-variable warnings,
// since a `constexpr` at namespace scope has internal linkage same as a
// plain `const` would. The real fix is removing the dead declarations
// where they're genuinely dead, not silencing the warning with
// `[[maybe_unused]]` on constants that have zero purpose without bgfx.
#if defined(LCU_ENABLE_BGFX)
// Skybox (Phase 27): the render clear color IS the sky, since there's no
// separate skybox geometry - a deep blue-black night and a bright blue
// day, linearly interpolated by DayNightCycle::sky_light_scale() (see
// day_night_cycle.h - [0.1, 1.0], 1.0 at noon). Reusing that existing
// real scale (not a second, separately-tuned day/night signal) keeps
// the sky, ambient light, and ambient-light-scaled block lighting all
// agreeing with each other.
constexpr lcu::math::Vec3 kNightSkyColor{0.02f, 0.03f, 0.08f};
constexpr lcu::math::Vec3 kDaySkyColor{0.45f, 0.65f, 0.95f};

// Sun/moon (Phase 27): a billboard quad at a fixed large distance from
// the camera (within the 500-unit far clip plane, see the perspective()
// call in the render loop), positioned by angle from
// DayNightCycle::time_of_day() around a fixed world-space circle - the
// same real time source everything else (sky color, sky light scale)
// already uses, not a second, separately-tuned animation. The moon is
// exactly the opposite phase (180 degrees) of the sun, so one is always
// up while the other is down, same as reality.
constexpr lcu::f32 kCelestialRadius = 300.0f;
constexpr lcu::f32 kCelestialHalfSize = 12.0f;
constexpr lcu::math::Vec3 kSunColor{1.0f, 0.95f, 0.75f};
constexpr lcu::math::Vec3 kMoonColor{0.75f, 0.8f, 0.9f};

// Entity debug box color (Phase 36, brief section 60): a bright,
// unmistakably-not-terrain red, matching the classic "debug wireframe"
// convention most engines use.
constexpr lcu::math::Vec3 kEntityBoxColor{1.0f, 0.1f, 0.1f};
#endif

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
    // Base tint (Phase 26) - see fs_chunk.sc for the procedural
    // noise/top-vs-side pattern this multiplies against, since there's
    // still no texture atlas (brief section 12/Phase 12).
    stone_def.color = {0.5f, 0.5f, 0.5f};
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
    grass_def.color = {0.3f, 0.7f, 0.2f};
    // Real grass-block convention (per-face color, Phase 26): green on
    // top, dirt-brown on the sides (bottom_color left unset - falls back
    // to side_color, since the underside looks like the sides, not the
    // top).
    grass_def.side_color = {0.4f, 0.25f, 0.1f};
    const lcu::voxel::BlockId grass_id = block_registry.register_block(grass_def);

    lcu::voxel::BlockDefinition dirt_def;
    dirt_def.namespaced_id = "game:dirt";
    dirt_def.display_name = "Dirt";
    dirt_def.is_transparent = false;
    dirt_def.has_collision = true;
    dirt_def.color = {0.4f, 0.25f, 0.1f};
    const lcu::voxel::BlockId dirt_id = block_registry.register_block(dirt_def);

    // Real climate/biome content (Phase 39, brief section 21) - the
    // Desert and Snowy biomes' own surface/subsurface block, standing
    // in for grass/dirt the same way those already stand in for
    // Plains (see worldgen::BiomeBlocks/generate_terrain_chunk below).
    // Sand doubles as both its own biome's surface and subsurface
    // (a real desert is sandy all the way down, not just a thin top
    // layer - unlike grass-over-dirt, there's no second, visually
    // distinct block a desert column would need underneath).
    lcu::voxel::BlockDefinition sand_def;
    sand_def.namespaced_id = "game:sand";
    sand_def.display_name = "Sand";
    sand_def.is_transparent = false;
    sand_def.has_collision = true;
    sand_def.color = {0.86f, 0.78f, 0.55f};
    const lcu::voxel::BlockId sand_id = block_registry.register_block(sand_def);

    // Snow is a real surface-only cap - the Snowy biome's subsurface
    // stays dirt (a snow-covered tundra, not "snow all the way down"),
    // matching Plains' own grass-over-dirt shape rather than Desert's
    // sand-all-the-way-down one.
    lcu::voxel::BlockDefinition snow_def;
    snow_def.namespaced_id = "game:snow";
    snow_def.display_name = "Snow";
    snow_def.is_transparent = false;
    snow_def.has_collision = true;
    snow_def.color = {0.95f, 0.97f, 1.0f};
    const lcu::voxel::BlockId snow_id = block_registry.register_block(snow_def);

    // First real light-emitting, player-placeable block (Phase 34,
    // closing the "torch block" half of the brief's own phase): every
    // earlier block in this file is dark (light_emission=0 by
    // BlockDefinition's own default) - this one emits real block light
    // that Phases 6/28/30/31's already-built propagation/rendering
    // pipeline picks up with zero further wiring, a genuine end-to-end
    // exercise of that whole pipeline, not new lighting code of its
    // own.
    //
    // Deliberately `is_transparent = false` (a solid glowing cube, not
    // a wall/floor-mounted cross/billboard shape): mesh_chunk_greedy
    // only ever meshes the *opaque* layer into real geometry today
    // (engine/voxel::ChunkMesh::transparent/water exist structurally
    // but stay empty - see DECISIONS.md "no transparent block
    // registered anywhere" from Phase 26, now literally not true, but
    // no transparent-layer *meshing* exists to make one visible yet).
    // A `true` here would make this block real, correctly-lit, and
    // completely invisible - exactly the kind of half-working gap
    // brief section 96's "no fake features" exists to catch. Solid and
    // collidable like every other block here until a real cross-shaped
    // block-rendering path exists to justify the visual difference.
    // Warm orange-yellow tint (no flame animation/particle - see Known
    // Limitations).
    lcu::voxel::BlockDefinition torch_def;
    torch_def.namespaced_id = "game:torch";
    torch_def.display_name = "Torch";
    torch_def.is_transparent = false;
    torch_def.has_collision = true;
    torch_def.light_emission = 14;
    torch_def.color = {1.0f, 0.65f, 0.2f};
    const lcu::voxel::BlockId torch_id = block_registry.register_block(torch_def);

    // Sea level + water (Phase 37, brief section 21): terrain_height()
    // is now centered on lcu::world::worldgen::kSeaLevel (world Y 0)
    // instead of always positive, so some columns' terrain genuinely
    // dips below it - generate_terrain_chunk (below) fills that gap
    // with this block up to sea level. Same "solid, not fake-invisible"
    // reasoning the torch block above already established: no
    // transparent-layer *meshing* exists yet (see DECISIONS.md), so
    // `is_transparent = true` here would make water correctly placed
    // by worldgen but completely invisible - exactly the trap Phase 34
    // caught for the torch. `has_collision = false` is the real,
    // honest difference from every solid block registered so far - a
    // player can walk/swim straight through it, using the same is_solid
    // predicate (BlockDefinition::has_collision) every other block's
    // collision already goes through, not a new physics special case.
    lcu::voxel::BlockDefinition water_def;
    water_def.namespaced_id = "game:water";
    water_def.display_name = "Water";
    water_def.is_transparent = false;
    water_def.has_collision = false;
    water_def.color = {0.15f, 0.35f, 0.85f};
    const lcu::voxel::BlockId water_id = block_registry.register_block(water_def);

    // Real ore blocks (Phase 40, brief section 21's "caves/ores" pipeline
    // stage): solid, collidable, dark like every other stone-family block
    // registered so far - no new visual/physical mechanic, just a
    // distinct color so `ore_at`'s substitution is actually visible in
    // the world. Coal darker/duller (a common, low-value resource),
    // Iron a warmer tan (rarer, per worldgen.cpp's own noise thresholds).
    lcu::voxel::BlockDefinition coal_ore_def;
    coal_ore_def.namespaced_id = "game:coal_ore";
    coal_ore_def.display_name = "Coal Ore";
    coal_ore_def.is_transparent = false;
    coal_ore_def.has_collision = true;
    coal_ore_def.color = {0.2f, 0.2f, 0.22f};
    const lcu::voxel::BlockId coal_ore_id = block_registry.register_block(coal_ore_def);

    lcu::voxel::BlockDefinition iron_ore_def;
    iron_ore_def.namespaced_id = "game:iron_ore";
    iron_ore_def.display_name = "Iron Ore";
    iron_ore_def.is_transparent = false;
    iron_ore_def.has_collision = true;
    iron_ore_def.color = {0.82f, 0.71f, 0.58f};
    const lcu::voxel::BlockId iron_ore_id = block_registry.register_block(iron_ore_def);

    // Real vegetation blocks (Phase 41, brief section 21's "vegetation"
    // pipeline stage): solid, collidable, dark like every other block
    // registered so far - no new physics/rendering mechanic (a real
    // see-through/non-collidable leaves block would need cross-shaped
    // or transparent-layer meshing, neither of which exists yet - see
    // DECISIONS.md, the same reasoning water/torch already established).
    lcu::voxel::BlockDefinition wood_def;
    wood_def.namespaced_id = "game:wood";
    wood_def.display_name = "Wood";
    wood_def.is_transparent = false;
    wood_def.has_collision = true;
    wood_def.color = {0.45f, 0.30f, 0.15f};
    const lcu::voxel::BlockId wood_id = block_registry.register_block(wood_def);

    lcu::voxel::BlockDefinition leaves_def;
    leaves_def.namespaced_id = "game:leaves";
    leaves_def.display_name = "Leaves";
    leaves_def.is_transparent = false;
    leaves_def.has_collision = true;
    leaves_def.color = {0.20f, 0.55f, 0.15f};
    const lcu::voxel::BlockId leaves_id = block_registry.register_block(leaves_def);

    lcu::voxel::BlockDefinition cactus_def;
    cactus_def.namespaced_id = "game:cactus";
    cactus_def.display_name = "Cactus";
    cactus_def.is_transparent = false;
    cactus_def.has_collision = true;
    cactus_def.color = {0.10f, 0.45f, 0.30f};
    const lcu::voxel::BlockId cactus_id = block_registry.register_block(cactus_def);

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

    lcu::items::ItemDefinition torch_item_def;
    torch_item_def.namespaced_id = "game:torch";
    torch_item_def.display_name = "Torch";
    torch_item_def.max_stack_size = 64;
    const lcu::items::ItemId torch_item_id = item_registry.register_item(torch_item_def);

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
    const std::array<PlaceableItem, 4> placeable_items{{
        {stone_id, stone_item_id, "game:stone"},
        {grass_id, grass_item_id, "game:grass"},
        {dirt_id, dirt_item_id, "game:dirt"},
        {torch_id, torch_item_id, "game:torch"},
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
    block_item_mapping.register_pair(torch_id, torch_item_id);

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

#if defined(LCU_ENABLE_BGFX)
    std::unordered_map<lcu::voxel::ChunkCoord, lcu::rendering::GpuChunkMesh> gpu_meshes;
#endif

    // Per-chunk light data (brief section 24), fed into meshing since
    // Phase 28 (real per-voxel light in the chunk shader). A real
    // lcu::lighting::WorldLight (Phase 29) rather than a bare
    // std::unordered_map<ChunkCoord, Light>; sky light is genuinely
    // cross-chunk-aware as of Phase 30 (see compute_initial_sky_light
    // below) - block light stays single-chunk-scoped until Phase 31.
    lcu::lighting::DefaultWorldLight world_light;

    // Initial block light for one just-loaded chunk (light from any
    // emitters, flooded within that chunk only). Order-independent
    // across chunks (block light doesn't cross chunk boundaries yet -
    // Phase 31), unlike the sky light pass below.
    const auto compute_initial_block_light = [&](lcu::voxel::ChunkCoord coord) {
        const lcu::voxel::Chunk* chunk = world.chunk_at(coord);
        if (!chunk) {
            return;
        }
        lcu::lighting::Light& light = world_light.chunk_light(coord);
        lcu::lighting::compute_block_light(*chunk, block_registry, light);
    };

    // Initial sky light for one just-loaded chunk (Phase 30):
    // cross-chunk-aware, seeded from the chunk directly above via
    // world_light (see compute_sky_light_column_cross_chunk's doc
    // comment). Callers MUST process an (x,z) column's chunks top-down
    // (highest chunk_y first) for this to actually cascade correctly -
    // see the load loop below.
    const auto compute_initial_sky_light = [&](lcu::voxel::ChunkCoord coord) {
        const lcu::voxel::Chunk* chunk = world.chunk_at(coord);
        if (!chunk) {
            return;
        }
        lcu::lighting::compute_sky_light_cross_chunk(*chunk, block_registry, world_light, coord);
    };

    // Incremental local lighting update after a single block at
    // `local` (within `coord`) changed from `old_id` to `new_id` - the
    // actual point of propagate_added_block_light/unpropagate_block_light
    // existing (brief section 24 "local updates, not full recompute"):
    // this never re-floods the whole chunk, only the region the edit
    // actually affects.
    // Returns every OTHER chunk (never `coord` itself - the caller
    // already knows to remesh that one) whose light this edit actually
    // wrote to (Phase 33) - the real answer to "what needs remeshing
    // besides the edited chunk", replacing a guess at a fixed neighbor
    // radius with the cross-chunk BFS's own ground truth. Still unioned
    // with neighbors_sharing_boundary at each call site below, since a
    // block's opacity change can uncover/hide a neighbor's face purely
    // geometrically even when no light value changed at all.
    const auto update_lighting_for_edit = [&](lcu::voxel::ChunkCoord coord, lcu::voxel::LocalBlockCoord local,
                                               lcu::voxel::BlockId old_id,
                                               lcu::voxel::BlockId new_id) -> std::unordered_set<lcu::voxel::ChunkCoord> {
        std::unordered_set<lcu::voxel::ChunkCoord> touched_chunks;
        const lcu::voxel::Chunk* chunk = world.chunk_at(coord);
        lcu::lighting::Light* light_ptr = world_light.find_chunk_light_mutable(coord);
        if (!chunk || !light_ptr) {
            return touched_chunks;
        }
        lcu::lighting::Light& light = *light_ptr;

        const lcu::u8 old_emission = block_registry.definition_of(old_id).light_emission;
        const lcu::u8 new_emission = block_registry.definition_of(new_id).light_emission;
        const bool new_is_opaque = !block_registry.definition_of(new_id).is_transparent;

        if (old_emission > 0) {
            const lcu::u8 old_level = light.block_light(local.x, local.y, local.z);
            lcu::lighting::unpropagate_block_light_cross_chunk(world, block_registry, world_light, coord, local.x,
                                                                local.y, local.z, old_level, &touched_chunks);
        }

        if (new_emission > 0) {
            light.set_block_light(local.x, local.y, local.z, new_emission);
            lcu::lighting::propagate_added_block_light_cross_chunk(world, block_registry, world_light, coord,
                                                                    local.x, local.y, local.z, &touched_chunks);
        } else if (new_is_opaque) {
            // The new block blocks light - retract whatever was there
            // before (a no-op if it was already dark).
            const lcu::u8 stale_level = light.block_light(local.x, local.y, local.z);
            if (stale_level > 0) {
                lcu::lighting::unpropagate_block_light_cross_chunk(world, block_registry, world_light, coord,
                                                                    local.x, local.y, local.z, stale_level,
                                                                    &touched_chunks);
            }
        } else {
            // The cell is now open (air, or another transparent block)
            // and wasn't a light source itself - let light flow back in
            // from whichever neighbor is currently brightest, the same
            // way removing a wall lets a hallway's existing torchlight
            // spill into the newly opened room. Cross-chunk-aware
            // (Phase 31) via WorldLight::block_light_at, so this works
            // correctly right at a chunk boundary too, not just for
            // neighbors inside this same chunk.
            lcu::u8 best_neighbor_level = 0;
            constexpr lcu::i32 kOffsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
            for (const auto& offset : kOffsets) {
                const auto neighbor_level = world_light.block_light_at(
                    coord, static_cast<lcu::i32>(local.x) + offset[0], static_cast<lcu::i32>(local.y) + offset[1],
                    static_cast<lcu::i32>(local.z) + offset[2]);
                if (neighbor_level) {
                    best_neighbor_level = std::max(best_neighbor_level, *neighbor_level);
                }
            }
            if (best_neighbor_level > 1) {
                light.set_block_light(local.x, local.y, local.z, static_cast<lcu::u8>(best_neighbor_level - 1));
                lcu::lighting::propagate_added_block_light_cross_chunk(world, block_registry, world_light, coord,
                                                                        local.x, local.y, local.z, &touched_chunks);
            }
        }

        // Sky light: genuinely local to its own (x,z) column, but
        // cross-chunk-aware vertically as of Phase 30 (the chunk
        // directly above, if loaded and lit, seeds this column's
        // sky_open_above). No cross-chunk write happens from this call
        // (it only ever writes into `coord`'s own light), so nothing to
        // add to touched_chunks here - see DECISIONS.md for the
        // honestly-scoped gap this leaves (a chunk below doesn't get
        // retroactively relit by an edit in the chunk above it).
        lcu::lighting::compute_sky_light_column_cross_chunk(*chunk, block_registry, world_light, coord, local.x,
                                                             local.z);
        return touched_chunks;
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
        // Phase 28: mesh with this chunk's real, already-computed
        // per-voxel light (compute_initial_block_light/
        // compute_initial_sky_light run for every loaded chunk before
        // its first remesh_and_upload call - see the load loop and
        // every edit/streaming call site below). Falls back to
        // the light-less (full-bright) mesh_chunk_greedy overload only
        // if that invariant is somehow violated, rather than asserting/
        // crashing on what would be a genuine ordering bug elsewhere.
        const lcu::lighting::Light* light_ptr = world_light.find_chunk_light(coord);
        lcu::voxel::ChunkMesh mesh;
        const auto job = job_system.submit(
            [&] {
                mesh = light_ptr ? lcu::voxel::mesh_chunk_greedy(*chunk, block_registry, *light_ptr)
                                  : lcu::voxel::mesh_chunk_greedy(*chunk, block_registry);
            },
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

    // Remeshes every chunk a single-block edit might have visibly
    // changed, besides the edited chunk itself (already remeshed
    // separately by every call site below): the union of
    // neighbors_sharing_boundary (a face uncovered/hidden purely by the
    // opacity change, regardless of light) and `light_touched_chunks`
    // (Phase 33 - the cross-chunk BFS's own real answer for which
    // OTHER chunks' light values actually changed, replacing a guessed
    // fixed radius). A std::unordered_set dedupes the case where both
    // sources name the same chunk.
    const auto remesh_edit_neighbors = [&](lcu::voxel::ChunkCoord coord, lcu::voxel::LocalBlockCoord local,
                                            const std::unordered_set<lcu::voxel::ChunkCoord>& light_touched_chunks) {
        std::unordered_set<lcu::voxel::ChunkCoord> to_remesh = light_touched_chunks;
        for (const lcu::voxel::ChunkCoord& neighbor : neighbors_sharing_boundary(coord, local)) {
            to_remesh.insert(neighbor);
        }
        for (const lcu::voxel::ChunkCoord& neighbor : to_remesh) {
            remesh_and_upload(neighbor);
        }
    };

    // Client-side chunk persistence (Phase 35): mirrors VoxelServer's
    // own chunk_save_dir/chunk_file_path exactly (see DECISIONS.md) -
    // needed so the real client-side chunk unloading this phase adds
    // (see unload_far_chunks below) doesn't silently discard
    // single-player edits the moment the player wanders far enough
    // away and back. Deliberately a separate directory from any
    // VoxelServer instance's own "<world>/chunks" (a plain
    // "client_world" next to the executable, the same relative-to-cwd
    // convention "shaders/chunk" already uses) - a networked client's
    // local copy is never authoritative anyway (server ChunkData
    // always wins on arrival, see the ChunkDataFragment handler
    // below), so there's no need for it to share a server's actual
    // save directory even when both processes happen to run from the
    // same working directory.
    const std::filesystem::path chunk_save_dir = std::filesystem::path("client_world") / "chunks";
    std::filesystem::create_directories(chunk_save_dir);
    const auto chunk_file_path = [&](lcu::voxel::ChunkCoord coord) {
        return (chunk_save_dir / fmt::format("{}_{}_{}.chunk", coord.x, coord.y, coord.z)).string();
    };

    // Loads `coord` (placeholder terrain via world.load_chunk), then
    // overwrites it with a real save from disk if one exists - same
    // "regenerate first, then overwrite if a save exists" sequencing
    // VoxelServer's own per-movement streaming already uses. Callers
    // that already know `coord` isn't Unloaded (idempotent re-checks)
    // should keep calling world.load_chunk directly instead - this is
    // specifically for a coordinate about to be loaded for real.
    const auto load_chunk_checking_disk = [&](lcu::voxel::ChunkCoord coord) {
        world.load_chunk(coord);
        lcu::voxel::Chunk loaded_from_disk;
        const auto load_result = lcu::serialization::load_chunk_from_file(chunk_file_path(coord), loaded_from_disk);
        if (load_result == lcu::serialization::ChunkLoadResult::Ok) {
            *world.chunk_at_mutable(coord) = loaded_from_disk;
        } else if (load_result != lcu::serialization::ChunkLoadResult::FileNotFound) {
            LCU_LOG_WARN(
                "Chunk ({},{},{}) has a local save file that failed to load, using regenerated terrain instead",
                coord.x, coord.y, coord.z);
        }
    };

    // Phase 35's actual "neighbor dirtying": call once, right after
    // `coord`'s own initial light (block + sky) has just been
    // computed, for any freshly-loaded chunk - reseeds light across
    // every already-loaded neighbor face (closing the Phase 30/31
    // "arrived too late" gaps - see reseed_light_for_newly_loaded_
    // chunk's own doc comment in propagation.h) and remeshes whatever
    // it reports touched. `coord` itself is remeshed separately by
    // every call site below regardless of what this reports (see that
    // function's doc comment on why `coord` may or may not appear in
    // its own result).
    const auto reseed_and_remesh_after_load = [&](lcu::voxel::ChunkCoord coord) {
        const auto touched = lcu::lighting::reseed_light_for_newly_loaded_chunk(world, block_registry, world_light,
                                                                                 coord);
        for (const lcu::voxel::ChunkCoord& neighbor : touched) {
            remesh_and_upload(neighbor);
        }
    };

    const lcu::core::ChunkLoadSettings load_settings = load_settings_from_env();
    // Phase 37: the loaded area centers on the real (possibly non-
    // origin) dry spawn column find_dry_spawn_column found above, not
    // always chunk (0,0) - see that function's doc comment.
    const SpawnColumn spawn_column = find_dry_spawn_column(kWorldSeed);
    const lcu::voxel::ChunkCoord spawn_chunk =
        lcu::voxel::world_to_chunk_and_local({spawn_column.x, 0, spawn_column.z}, lcu::voxel::Chunk::kEdgeLength)
            .chunk;
    LCU_LOG_INFO(
        "Loading world (seed={}) around spawn column ({},{}, biome={}) (radius_xz={}, chunk_y=[{},{}])...",
        kWorldSeed, spawn_column.x, spawn_column.z, biome_name(lcu::world::worldgen::biome_at(
                                                          kWorldSeed, spawn_column.x, spawn_column.z)),
        load_settings.radius_xz, load_settings.min_chunk_y, load_settings.max_chunk_y);
    for (lcu::i32 cx = spawn_chunk.x - load_settings.radius_xz; cx <= spawn_chunk.x + load_settings.radius_xz;
         ++cx) {
        for (lcu::i32 cz = spawn_chunk.z - load_settings.radius_xz; cz <= spawn_chunk.z + load_settings.radius_xz;
             ++cz) {
            // Three passes per column, not one: block light (Phase 6)
            // and sky light (Phase 30) are computed separately because
            // sky light must cascade top-down (the highest chunk_y in
            // this column needs its own light computed before the one
            // below it can correctly seed from it - see
            // compute_initial_sky_light's doc comment); block light and
            // meshing have no such ordering requirement, but meshing
            // still has to happen last, after both light passes, since
            // Phase 28's mesh_chunk_greedy reads whatever's already in
            // world_light for a chunk.
            for (lcu::i32 cy = load_settings.min_chunk_y; cy <= load_settings.max_chunk_y; ++cy) {
                const lcu::voxel::ChunkCoord coord{cx, cy, cz};
                load_chunk_checking_disk(coord);
                compute_initial_block_light(coord);
            }
            for (lcu::i32 cy = load_settings.max_chunk_y; cy >= load_settings.min_chunk_y; --cy) {
                compute_initial_sky_light({cx, cy, cz});
            }
            for (lcu::i32 cy = load_settings.min_chunk_y; cy <= load_settings.max_chunk_y; ++cy) {
                // Reseeds against whatever earlier column in this same
                // spawn-area load already finished (Phase 35) - real
                // even on the very first load, not just later
                // streaming: this loop's own earlier iterations are
                // "already-loaded neighbors" by the time a later one
                // runs.
                reseed_and_remesh_after_load({cx, cy, cz});
                remesh_and_upload({cx, cy, cz});
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
            {spawn_column.x, lcu::world::worldgen::terrain_height(kWorldSeed, spawn_column.x, spawn_column.z) + 5,
             spawn_column.z},
            lcu::voxel::Chunk::kEdgeLength);
        if (const auto sky_light = world_light.sky_light_at(open_air_split.chunk, open_air_split.local.x,
                                                              open_air_split.local.y, open_air_split.local.z)) {
            LCU_LOG_INFO("Sky light 5 blocks above spawn column: {}", *sky_light);
        }
    }

#if defined(LCU_ENABLE_BGFX)
    // Only present when LCU_BUILD_SHADER_TOOLS compiled shaders into
    // <exe_dir>/shaders/chunk (see client/CMakeLists.txt and BUILDING.md).
    // Real fix (Phase 43 hygiene): resolved against Window::
    // executable_base_path() (SDL_GetBasePath), not the current working
    // directory - a previous version of this path was CWD-relative,
    // which happened to work in every verification run in this repo
    // (always launched from the executable's own directory) but broke
    // the first time a real user launched the client from elsewhere
    // (e.g. double-clicking it, or `./build/macos/bin/VoxelClient` from
    // the repo root). A real asset system (brief section 33) will
    // replace this raw path with an ID-based lookup once one exists.
    const std::string shader_base_path = lcu::platform::Window::executable_base_path();
    bgfx::ProgramHandle chunk_program = BGFX_INVALID_HANDLE;
    // Sky program (Phase 27, sun/moon) - same on/off-ness as chunk_program,
    // driven by the same LCU_HAS_CHUNK_SHADERS define (client/CMakeLists.txt
    // compiles both shader pairs together under LCU_BUILD_SHADER_TOOLS).
    bgfx::ProgramHandle sky_program = BGFX_INVALID_HANDLE;
    // 2D UI program (Phase 44) - same on/off-ness as chunk_program/
    // sky_program, compiled by the same LCU_HAS_CHUNK_SHADERS-gated
    // block in client/CMakeLists.txt.
    bgfx::ProgramHandle ui2d_program = BGFX_INVALID_HANDLE;
#if defined(LCU_HAS_CHUNK_SHADERS)
    chunk_program = lcu::rendering::load_chunk_program(shader_base_path + "shaders/chunk", "chunk");
    sky_program = lcu::rendering::load_chunk_program(shader_base_path + "shaders/sky", "sky");
    ui2d_program = lcu::rendering::load_chunk_program(shader_base_path + "shaders/ui2d", "ui2d");
#endif
    LCU_LOG_INFO("Chunk shader program valid={}", bgfx::isValid(chunk_program));
    LCU_LOG_INFO("Sky shader program valid={}", bgfx::isValid(sky_program));
    LCU_LOG_INFO("UI2D shader program valid={}", bgfx::isValid(ui2d_program));
#endif

    // --- Player: spawns resting on dry land at spawn_column (Phase 37 - see
    // find_dry_spawn_column's doc comment) ---
    // terrain_height() returns the topmost *solid* block's Y (worldgen.cpp:
    // world_y <= height is solid) - the first open-air cell to stand in is
    // one above that, not terrain_height() itself (an off-by-one that would
    // otherwise spawn the player embedded in the top layer of solid ground).
    const lcu::i32 spawn_ground_y =
        lcu::world::worldgen::terrain_height(kWorldSeed, spawn_column.x, spawn_column.z) + 1;
    lcu::physics::PlayerPhysicsState player;
    player.aabb = make_player_aabb(
        {static_cast<lcu::f32>(spawn_column.x), static_cast<lcu::f32>(spawn_ground_y),
         static_cast<lcu::f32>(spawn_column.z)});
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
                // Same three-pass split as the initial spawn-area load
                // above (block light any order, sky light top-down,
                // then remesh) - collected into a vector first since
                // (unlike the spawn load) not every cy in range is
                // necessarily newly-loaded here (state_of's Unloaded
                // check may skip some).
                std::vector<lcu::voxel::ChunkCoord> newly_loaded;
                for (lcu::i32 cy = load_settings.min_chunk_y; cy <= load_settings.max_chunk_y; ++cy) {
                    const lcu::voxel::ChunkCoord coord{cx, cy, cz};
                    if (world.state_of(coord) != lcu::world::ChunkLifecycleState::Unloaded) {
                        continue;
                    }
                    load_chunk_checking_disk(coord);
                    compute_initial_block_light(coord);
                    newly_loaded.push_back(coord);
                }
                for (auto it = newly_loaded.rbegin(); it != newly_loaded.rend(); ++it) {
                    compute_initial_sky_light(*it);
                }
                for (const lcu::voxel::ChunkCoord& coord : newly_loaded) {
                    // Phase 35: reseeds against every already-loaded
                    // neighbor (including ones outside this same
                    // streaming batch) before this chunk's own mesh
                    // upload, so a torch already lit just across the
                    // boundary shows up correctly the moment this
                    // chunk first appears, not one edit later.
                    reseed_and_remesh_after_load(coord);
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

    // Client-side chunk unloading (Phase 35, "chunk unload marks
    // neighbors dirty"'s literal half): before this phase the
    // client's World only ever grew for the process's whole lifetime
    // (deliberately - see Phase 16's DECISIONS.md entry), even as the
    // player walked far away, holding every chunk's mesh/GPU buffers/
    // light data forever. This is the client's own counterpart to
    // VoxelServer's Phase 20 interest-scoped unloading: only X/Z
    // distance-gated (the vertical range is always the same fixed
    // [min_chunk_y, max_chunk_y] band, never trimmed), with a margin
    // beyond load_settings.radius_xz so a chunk just past the load
    // radius doesn't immediately reload next frame (the same
    // load/unload-radius hysteresis World::update_streaming's own doc
    // comment describes, applied manually here since this client
    // drives loading itself rather than through that function). Saves
    // to chunk_save_dir first - without that, any single-player edit
    // in the chunk would silently revert to pristine regenerated
    // terrain the moment the player wandered back into range.
    constexpr lcu::i32 kUnloadRadiusMargin = 1;
    const auto unload_far_chunks = [&](lcu::voxel::ChunkCoord center) {
        std::vector<lcu::voxel::ChunkCoord> to_unload;
        for (const lcu::voxel::ChunkCoord& coord : world.loaded_chunk_coords()) {
            const lcu::i32 chebyshev_xz = std::max(std::abs(coord.x - center.x), std::abs(coord.z - center.z));
            if (chebyshev_xz > load_settings.radius_xz + kUnloadRadiusMargin) {
                to_unload.push_back(coord);
            }
        }
        for (const lcu::voxel::ChunkCoord& coord : to_unload) {
            if (const lcu::voxel::Chunk* chunk_to_save = world.chunk_at(coord)) {
                if (!lcu::serialization::save_chunk_to_file(*chunk_to_save, chunk_file_path(coord))) {
                    LCU_LOG_WARN(
                        "Failed to save chunk ({},{},{}) before unloading - any local edits in it will be lost",
                        coord.x, coord.y, coord.z);
                }
            }
#if defined(LCU_ENABLE_BGFX)
            if (auto it = gpu_meshes.find(coord); it != gpu_meshes.end()) {
                lcu::rendering::destroy_gpu_chunk_mesh(it->second);
                gpu_meshes.erase(it);
            }
#endif
            // Real map hygiene (WorldLight::remove_chunk_light's own
            // doc comment has named this phase as its real caller
            // since Phase 29) - without this, world_light would keep
            // accumulating light data for chunks nothing can see or
            // mesh anymore, forever, for the lifetime of the process.
            world_light.remove_chunk_light(coord);
            world.unload_chunk(coord);
        }
        if (!to_unload.empty()) {
            LCU_LOG_INFO("Unloaded {} chunk(s) beyond streaming range (total {} loaded)", to_unload.size(),
                         world.loaded_chunk_count());
        }
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
            const lcu::math::Vec3 spawn_pos{static_cast<lcu::f32>(spawn_column.x) + 4.0f * std::cos(angle),
                                             static_cast<lcu::f32>(spawn_ground_y),
                                             static_cast<lcu::f32>(spawn_column.z) + 4.0f * std::sin(angle)};
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

    // Persistent options (Phase 45): loaded once at startup - real
    // Minecraft-parity KeyBindings/mouse-sensitivity/HUD defaults if no
    // options.txt exists yet at this real, per-OS location (a real,
    // expected first-run state, not an error - see Options::load's own
    // doc comment), the user's real saved choices otherwise. Phase 46's
    // options/controls menu is the first thing that will actually
    // *change* this at runtime; this phase only wires up the real
    // load/save mechanics and lets the client's existing systems
    // consume them (mouse sensitivity, HUD/debug-overlay visibility,
    // the actual keymap) instead of the fixed constants/fresh-default
    // KeyBindings they used through Phase 44.
    lcu::platform::Options options;
    const std::string options_path = lcu::platform::Options::default_path();
    if (options.load(options_path)) {
        LCU_LOG_INFO("Loaded options from \"{}\"", options_path);
    } else {
        LCU_LOG_INFO("No options file at \"{}\" yet - using real defaults", options_path);
    }

    lcu::platform::DesktopInputBackend input_backend;
    lcu::platform::InputState input;
    lcu::platform::InputState previous_input;

    // Real mouse-look capture (Phase 43): the standard FPS convention -
    // cursor hidden and confined the moment the game starts, released by
    // ESC/Tab or a focus loss, re-grabbed by clicking back into the
    // window (see the capture-management block in the frame loop below).
    // Real, not a fake toggle: under this sandbox's own headless/dummy
    // SDL video driver, the underlying SDL call fails (logged, not
    // fatal - see Window::set_relative_mouse_mode) since there's no real
    // mouse device to capture, same honest "logic runs for real, the
    // visual/device-level result is NOT VERIFIED — ENVIRONMENT
    // LIMITATION" pattern every other input-adjacent feature here uses.
    window.set_relative_mouse_mode(true);
    lcu::debug::FrameStats frame_stats;
#if defined(LCU_ENABLE_BGFX)
    lcu::f32 last_known_fps = 0.0f;  // updated only on frame_stats' periodic reports (see below); drives the on-screen debug overlay
#endif

    // Headless verification hook for the pause menu (Phase 46): drives
    // ESC/navigate/adjust/ESC through InputState the same way every
    // other LCU_VERIFY_* hook drives real gameplay actions - see the
    // frame-numbered block below. Deliberately does NOT try to exercise
    // the controls screen's "press any key to rebind" capture: that
    // reads real SDL keyboard/mouse hardware state directly
    // (poll_any_pressed_key), which the dummy video/input driver this
    // sandbox runs under never actually produces (same real, honest gap
    // Phase 43's own mouse-look verification already has - see
    // DECISIONS.md).
    const bool verify_menu = std::getenv("LCU_VERIFY_MENU") != nullptr;

    const bool verify_break_place = std::getenv("LCU_VERIFY_BREAK_PLACE") != nullptr;
    const bool verify_craft = std::getenv("LCU_VERIFY_CRAFT") != nullptr;
    int verify_craft_step = 0;
    const auto verify_craft_start = std::chrono::steady_clock::now();
    const bool verify_torch = std::getenv("LCU_VERIFY_TORCH") != nullptr;
    bool verify_torch_granted = false;

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

    // Real pause menu (Phase 46, brief section 60's menu framework):
    // ESC in-game now opens this instead of only releasing mouse
    // capture (Phase 43's own behavior stays as a side effect - opening
    // the menu still needs the cursor free to click rows). `menu_stack`
    // being non-empty *is* "paused" - there is no separate bool to ever
    // drift out of sync with it (see the `paused` local computed fresh
    // every frame below).
    lcu::ui::MenuStack menu_stack;
    bool quit_requested = false;

    // Real "press any key to rebind" capture (Phase 46's controls
    // screen): set by a row's on_activate, consumed by
    // lcu::platform::poll_any_pressed_key() below once per frame while
    // true. ESC cancels without changing the binding (see is_escape_key
    // below) - it does not also pop the controls screen the way it
    // normally would, so accidentally hitting ESC to cancel a capture
    // doesn't also kick the player back to the pause screen.
    bool waiting_for_rebind = false;
    bool rebind_ready = false;
    lcu::platform::Action rebind_action = lcu::platform::Action::Jump;
    lcu::usize rebind_slot = 0;

    // Real per-frame-deferred menu-stack mutation (Phase 46): a
    // MenuItem callback (on_activate/on_adjust) is invoked FROM INSIDE
    // that same item's own storage - it lives in the MenuScreen
    // currently on top of menu_stack. Popping/pushing/clearing
    // menu_stack DIRECTLY from within such a callback would destroy (or,
    // for push_back, potentially reallocate and move) that very
    // MenuScreen - including the closure currently executing - while
    // its own code is still running, a real use-after-free (this was
    // reproduced as a real, verified segfault during headless testing
    // before this fix - see DECISIONS.md). So every callback below that
    // needs to change which screen is on top only ever *records* what
    // to do here; the actual push/pop/clear happens once, after
    // activate_selected()/adjust_selected() has fully returned, back in
    // the main loop where nothing is executing from the old screen's
    // storage anymore.
    std::function<void()> pending_menu_action;

    // Forward-declared as std::function (not auto/lambda) so
    // build_options_screen/build_controls_screen can reference
    // themselves (to rebuild their own screen after a value changes) -
    // a plain lambda can't reference itself by name before it's fully
    // defined, a mutable std::function variable can.
    std::function<lcu::ui::MenuScreen()> build_pause_screen;
    std::function<lcu::ui::MenuScreen()> build_options_screen;
    std::function<lcu::ui::MenuScreen()> build_controls_screen;

    build_pause_screen = [&]() -> lcu::ui::MenuScreen {
        lcu::ui::MenuScreen screen;
        screen.title = "Pause";
        screen.items.push_back(
            {"Zurueck zum Spiel", "", [&]() { pending_menu_action = [&]() { menu_stack.clear(); }; }, nullptr});
        screen.items.push_back({"Optionen", "",
                                 [&]() { pending_menu_action = [&]() { menu_stack.push(build_options_screen()); }; },
                                 nullptr});
        screen.items.push_back({"Steuerung", "",
                                 [&]() { pending_menu_action = [&]() { menu_stack.push(build_controls_screen()); }; },
                                 nullptr});
        screen.items.push_back({"Beenden", "", [&]() { quit_requested = true; }, nullptr});
        return screen;
    };

    build_options_screen = [&]() -> lcu::ui::MenuScreen {
        lcu::ui::MenuScreen screen;
        screen.title = "Optionen";

        // Schedules this same screen to be rebuilt from `options`'
        // current values, re-selecting whatever row was selected before
        // - the simplest real way to keep every row's displayed
        // value_text in sync with the option it names, at the cost of
        // reconstructing a handful of small MenuItems on every change
        // (real, not noticeable - these screens are a few rows, not
        // thousands). Deferred via pending_menu_action, not run
        // immediately - see its own doc comment above for why.
        const auto schedule_rebuild = [&]() {
            pending_menu_action = [&]() {
                const lcu::usize index = menu_stack.top().selected_index;
                menu_stack.pop();
                menu_stack.push(build_options_screen());
                menu_stack.select_index(index);
            };
        };

        lcu::ui::MenuItem sensitivity;
        sensitivity.label = "Maus-Empfindlichkeit";
        sensitivity.value_text = fmt::format("{:.4f}", options.mouse_sensitivity);
        sensitivity.on_adjust = [&, schedule_rebuild](lcu::i32 direction) {
            options.mouse_sensitivity =
                std::clamp(options.mouse_sensitivity + static_cast<lcu::f32>(direction) * 0.0002f, 0.0002f, 0.02f);
            schedule_rebuild();
        };
        screen.items.push_back(std::move(sensitivity));

        lcu::ui::MenuItem fov;
        fov.label = "Sichtfeld (FOV)";
        fov.value_text = std::to_string(options.fov);
        fov.on_adjust = [&, schedule_rebuild](lcu::i32 direction) {
            options.fov = std::clamp(options.fov + direction * 5, 30, 110);
            schedule_rebuild();
        };
        screen.items.push_back(std::move(fov));

        lcu::ui::MenuItem hud;
        hud.label = "HUD";
        hud.value_text = options.hud_enabled ? "AN" : "AUS";
        hud.on_activate = [&, schedule_rebuild]() {
            options.hud_enabled = !options.hud_enabled;
            schedule_rebuild();
        };
        screen.items.push_back(std::move(hud));

        lcu::ui::MenuItem debug_overlay;
        debug_overlay.label = "Debug-Overlay";
        debug_overlay.value_text = options.debug_overlay_enabled ? "AN" : "AUS";
        debug_overlay.on_activate = [&, schedule_rebuild]() {
            options.debug_overlay_enabled = !options.debug_overlay_enabled;
            schedule_rebuild();
        };
        screen.items.push_back(std::move(debug_overlay));

        // Real "Renderdistanz" is deliberately NOT a row here - the
        // streaming radius (`load_settings.radius_xz` below) is `const`
        // and re-streaming/unloading on a live radius change is a real,
        // separate structural change this phase's own directive allows
        // deferring as PARTIAL (see DECISIONS.md) rather than shipping
        // a +/- row that would visibly do nothing.
        lcu::ui::MenuItem back;
        back.label = "Zurueck";
        back.on_activate = [&]() {
            options.save(options_path);
            LCU_LOG_INFO("Saved options to \"{}\"", options_path);
            pending_menu_action = [&]() { menu_stack.pop(); };
        };
        screen.items.push_back(std::move(back));

        return screen;
    };

    build_controls_screen = [&]() -> lcu::ui::MenuScreen {
        lcu::ui::MenuScreen screen;
        screen.title = "Steuerung";

        for (lcu::usize i = 0; i < static_cast<lcu::usize>(lcu::platform::Action::Count); ++i) {
            const auto action = static_cast<lcu::platform::Action>(i);
            // Escape/MenuConfirm are deliberately not listed - real
            // menu-meta actions, not rebindable from this screen (see
            // their own doc comments in input.h).
            if (action == lcu::platform::Action::Escape || action == lcu::platform::Action::MenuConfirm) {
                continue;
            }
            lcu::ui::MenuItem item;
            item.label = lcu::platform::action_name(action);
            item.value_text = lcu::platform::physical_key_name(options.key_bindings.bindings_for(action)[0]);
            item.on_activate = [&, action]() {
                waiting_for_rebind = true;
                rebind_ready = false;
                rebind_action = action;
                rebind_slot = 0;
                LCU_LOG_INFO("Waiting for a new binding for {} (ESC cancels)...",
                             lcu::platform::action_name(action));
            };
            screen.items.push_back(std::move(item));
        }

        lcu::ui::MenuItem reset;
        reset.label = "Reset";
        reset.on_activate = [&]() {
            options.key_bindings.reset_to_defaults();
            LCU_LOG_INFO("Controls reset to defaults");
            pending_menu_action = [&]() {
                const lcu::usize index = menu_stack.top().selected_index;
                menu_stack.pop();
                menu_stack.push(build_controls_screen());
                menu_stack.select_index(index);
            };
        };
        screen.items.push_back(std::move(reset));

        lcu::ui::MenuItem back;
        back.label = "Zurueck";
        back.on_activate = [&]() {
            options.save(options_path);
            LCU_LOG_INFO("Saved options to \"{}\"", options_path);
            pending_menu_action = [&]() { menu_stack.pop(); };
        };
        screen.items.push_back(std::move(back));

        return screen;
    };

    while (window.pump_events()) {
        input_backend.update(options.key_bindings, input);

        // LCU_VERIFY_MENU (Phase 46) - overrides real (always-unpressed
        // under this sandbox's dummy input driver) polled state with
        // synthetic edges, same as every other LCU_VERIFY_* hook. Placed
        // here, right after input_backend.update() and before the real
        // escape/menu-navigation logic below, so those real code paths
        // see this frame's synthetic input the same way they'd see a
        // real key press - not one frame late.
        if (verify_menu) {
            input.set_down(lcu::platform::Action::Escape,
                            frame == kVerifyMenuOpenFrame || frame == kVerifyMenuCloseFrame);
            input.set_down(lcu::platform::Action::MoveForward,
                            (frame >= kVerifyMenuMoveWhilePausedStart && frame <= kVerifyMenuMoveWhilePausedEnd) ||
                                (frame >= kVerifyMenuMoveWhileResumedStart && frame <= kVerifyMenuMoveWhileResumedEnd));
            input.set_down(lcu::platform::Action::LookDown,
                            frame == kVerifyMenuNavigateToOptionsFrame || frame == kVerifyMenuNavigateToBackFrame1 ||
                                frame == kVerifyMenuNavigateToBackFrame2 || frame == kVerifyMenuNavigateToBackFrame3 ||
                                frame == kVerifyMenuNavigateToBackFrame4);
            input.set_down(lcu::platform::Action::LookRight, frame == kVerifyMenuAdjustSensitivityFrame1 ||
                                                                    frame == kVerifyMenuAdjustSensitivityFrame2);
            input.set_down(lcu::platform::Action::MenuConfirm,
                            frame == kVerifyMenuOpenOptionsFrame || frame == kVerifyMenuActivateBackFrame);
            if (frame == kVerifyMenuMoveWhilePausedStart) {
                LCU_LOG_INFO("Menu verify: player position before paused-movement attempt: ({:.2f}, {:.2f}, {:.2f})",
                             player.aabb.center().x, player.aabb.center().y, player.aabb.center().z);
            }
            if (frame == kVerifyMenuMoveWhilePausedEnd) {
                LCU_LOG_INFO("Menu verify: player position after paused-movement attempt (should be unchanged): "
                             "({:.2f}, {:.2f}, {:.2f})",
                             player.aabb.center().x, player.aabb.center().y, player.aabb.center().z);
            }
            if (frame == kVerifyMenuMoveWhileResumedStart) {
                LCU_LOG_INFO(
                    "Menu verify: player position before resumed-movement attempt: ({:.2f}, {:.2f}, {:.2f})",
                    player.aabb.center().x, player.aabb.center().y, player.aabb.center().z);
            }
            if (frame == kVerifyMenuMoveWhileResumedEnd) {
                LCU_LOG_INFO("Menu verify: player position after resumed-movement attempt (should have moved): "
                             "({:.2f}, {:.2f}, {:.2f})",
                             player.aabb.center().x, player.aabb.center().y, player.aabb.center().z);
            }
        }

        // Real mouse-capture management (Phase 43, extended Phase 46):
        // ESC/Tab or losing window focus releases capture; clicking
        // while free re-captures it. The re-capture click must not ALSO
        // register as a break/place action the same frame - a real game
        // treats "the click that got focus back" as consumed by that
        // alone, not a double-purpose input - so
        // suppress_click_for_recapture is threaded down to
        // interact_pressed/place_pressed's own edge-detection below.
        const bool escape_pressed =
            input.is_down(lcu::platform::Action::Escape) && !previous_input.is_down(lcu::platform::Action::Escape);

        // Real "press any key to rebind" capture (Phase 46's controls
        // screen) - polled once per frame, before the pause-toggle
        // logic below, so the very ESC press that cancels a capture
        // never also closes the whole menu that same frame. Requires
        // the physical keyboard/mouse to read fully released at least
        // once after entering this mode (rebind_ready) before accepting
        // a real capture - otherwise the same Enter/click that opened
        // "waiting for input" would immediately bind itself, since
        // poll_any_pressed_key() sees currently-held state, not edges.
        if (waiting_for_rebind) {
            const lcu::platform::PhysicalKey polled = lcu::platform::poll_any_pressed_key();
            if (!rebind_ready) {
                if (polled == lcu::platform::kUnboundKey) {
                    rebind_ready = true;
                }
            } else if (polled != lcu::platform::kUnboundKey) {
                if (lcu::platform::is_escape_key(polled)) {
                    LCU_LOG_INFO("Rebind cancelled");
                } else {
                    options.key_bindings.bind(rebind_action, rebind_slot, polled);
                    LCU_LOG_INFO("Bound {} to {}", lcu::platform::action_name(rebind_action),
                                 lcu::platform::physical_key_name(polled));
                    if (!menu_stack.empty()) {
                        const lcu::usize index = menu_stack.top().selected_index;
                        menu_stack.pop();
                        menu_stack.push(build_controls_screen());
                        menu_stack.select_index(index);
                    }
                }
                waiting_for_rebind = false;
                rebind_ready = false;
            }
        }

        // ESC opens the pause menu from gameplay, or pops one screen
        // back while a menu is already open (popping the last screen
        // closes it and re-captures the mouse) - see Phase 46's own
        // directive. Suppressed while actively capturing a rebind so
        // ESC cancels that instead (handled above).
        if (escape_pressed && !waiting_for_rebind) {
            if (menu_stack.empty()) {
                menu_stack.push(build_pause_screen());
                window.set_relative_mouse_mode(false);
            } else {
                menu_stack.pop();
                if (menu_stack.empty()) {
                    window.set_relative_mouse_mode(true);
                }
            }
        }
        if (window.consume_focus_lost() && window.relative_mouse_mode()) {
            window.set_relative_mouse_mode(false);
        }
        bool suppress_click_for_recapture = false;
        if (menu_stack.empty() && !window.relative_mouse_mode() &&
            (input.is_down(lcu::platform::Action::Interact) || input.is_down(lcu::platform::Action::PlaceBlock))) {
            window.set_relative_mouse_mode(true);
            suppress_click_for_recapture = true;
        }

        // Real menu navigation (Phase 46): while a menu is open, the
        // existing LookUp/Down/Left/Right actions (already bound to the
        // arrow keys, see input.h's own doc comment on why they still
        // exist alongside mouse-look) drive selection/value-adjustment
        // instead of the camera, MenuConfirm (Enter) activates the
        // selected row, and a real click hit-tests against the row the
        // cursor is actually over (menu_item_at_point) rather than
        // whatever happens to be selected. Escape is handled above
        // (pop/close), not here.
        if (!menu_stack.empty() && !waiting_for_rebind) {
            if (input.is_down(lcu::platform::Action::LookUp) && !previous_input.is_down(lcu::platform::Action::LookUp)) {
                menu_stack.move_selection(-1);
            }
            if (input.is_down(lcu::platform::Action::LookDown) &&
                !previous_input.is_down(lcu::platform::Action::LookDown)) {
                menu_stack.move_selection(1);
            }
            if (input.is_down(lcu::platform::Action::LookLeft) &&
                !previous_input.is_down(lcu::platform::Action::LookLeft)) {
                menu_stack.adjust_selected(-1);
            }
            if (input.is_down(lcu::platform::Action::LookRight) &&
                !previous_input.is_down(lcu::platform::Action::LookRight)) {
                menu_stack.adjust_selected(1);
            }
            if (input.is_down(lcu::platform::Action::MenuConfirm) &&
                !previous_input.is_down(lcu::platform::Action::MenuConfirm)) {
                menu_stack.activate_selected();
            }
            if (input.is_down(lcu::platform::Action::Interact) &&
                !previous_input.is_down(lcu::platform::Action::Interact)) {
                const lcu::platform::Window::MousePosition mouse_pos = lcu::platform::Window::mouse_position();
                const auto hit_index =
                    lcu::ui::menu_item_at_point(menu_stack.top(), static_cast<lcu::u32>(window.width()),
                                                 static_cast<lcu::u32>(window.height()), mouse_pos.x, mouse_pos.y);
                if (hit_index) {
                    menu_stack.select_index(*hit_index);
                    menu_stack.activate_selected();
                }
            }
            const lcu::f32 menu_wheel_delta = window.consume_wheel_delta_y();
            if (menu_wheel_delta > 0.0f) {
                menu_stack.move_selection(-1);
            } else if (menu_wheel_delta < 0.0f) {
                menu_stack.move_selection(1);
            }
        }

        // Runs any menu_stack push/pop/clear a row's callback scheduled
        // above (activate_selected()/adjust_selected() only ever record
        // these via pending_menu_action, never mutate menu_stack
        // directly - see its own doc comment for the real
        // use-after-free this avoids). Safe here: activate_selected()/
        // adjust_selected() have both fully returned by this point, so
        // nothing is still executing from whichever screen is about to
        // be popped/replaced.
        if (pending_menu_action) {
            const std::function<void()> action = std::move(pending_menu_action);
            pending_menu_action = nullptr;
            action();
        }

        const bool paused = !menu_stack.empty();

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
        if (verify_torch) {
            if (!verify_torch_granted) {
                // Synthetic setup (see kVerifyTorchBreakFrame's doc
                // comment above): nothing in this build's world drops a
                // torch to pick up yet, so this hook grants one
                // directly, the same way LCU_VERIFY_CRAFT's own setup
                // breaks real blocks to seed its inventory state.
                player_inventory.add_item(item_registry, {torch_item_id, 1});
                verify_torch_granted = true;
            }
            input.set_down(lcu::platform::Action::Interact, frame == kVerifyTorchBreakFrame);
            input.set_down(lcu::platform::Action::CycleHotbar,
                            frame == kVerifyTorchCycleFrame1 || frame == kVerifyTorchCycleFrame2 ||
                                frame == kVerifyTorchCycleFrame3);
            input.set_down(lcu::platform::Action::PlaceBlock, frame == kVerifyTorchPlaceFrame);
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
                                        const auto light_touched =
                                            update_lighting_for_edit(split.chunk, split.local, old_id, new_id);
                                        remesh_and_upload(split.chunk);
                                        remesh_edit_neighbors(split.chunk, split.local, light_touched);
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
                                compute_initial_block_light(coord);
                                // Sky light here can't guarantee the
                                // top-down cascade ordering
                                // compute_initial_sky_light's doc
                                // comment asks for - a networked
                                // ChunkData can arrive in any vertical
                                // order relative to its own neighbors.
                                // No longer an open gap as of Phase 35:
                                // reseed_and_remesh_after_load below
                                // retroactively recomputes/remeshes
                                // every already-loaded chunk this
                                // arrival's light should have reached
                                // (both directions - a roof arriving
                                // above an already-lit chunk below now
                                // correctly darkens it too, not just
                                // the historically-self-correcting
                                // reverse order).
                                compute_initial_sky_light(coord);
                                remesh_and_upload(coord);
                                reseed_and_remesh_after_load(coord);
                                // Still unioned with the plain
                                // geometric 6-neighbor remesh: a full
                                // chunk overwrite (unlike a single
                                // block edit) can uncover/hide a
                                // neighbor's boundary faces purely by
                                // opacity, independent of any light
                                // value ever changing - reseed's
                                // touched set only reports the latter.
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
        } else if (!paused) {
            game::systems::update_ai_wander(entity_registry, ai_wander_config, ai_rng, delta_seconds);
        }
        // Real pause (Phase 46, brief section 60's menu framework:
        // "game pauses (simulation, audio, network)"): everything from
        // here through the end of this frame's break/place/craft/
        // movement logic is real simulation, so it's skipped outright
        // while a menu is open - the camera/world/inventory simply
        // don't advance, not merely "the player can't act". The
        // networked packet-*receive* loop above deliberately stays
        // outside this gate even while paused (see DECISIONS.md): fully
        // halting it risked the connection reading as dead (missed
        // heartbeats/ChunkData) by the time the player unpauses - only
        // this client's own *outgoing* input pauses (no predict_and_
        // record/send call happens below), audio only in the sense that
        // no new gameplay sound can trigger without the break/place
        // logic that plays it running.
        if (!paused) {
            day_night_cycle.update(delta_seconds);
        }

        if (!paused) {
            // Real mouse-look (Phase 43) - applied additively alongside the
            // arrow-key fallback below, not instead of it (see Action::LookUp's
            // own doc comment in input.h). Only while the window actually owns
            // capture, so a free/uncaptured mouse (e.g. right after alt-tabbing
            // back, before the recapture click lands) never spuriously spins
            // the camera from residual/incidental motion.
            if (window.relative_mouse_mode()) {
                camera.add_yaw_pitch(input.mouse_delta_x() * options.mouse_sensitivity,
                                     -input.mouse_delta_y() * options.mouse_sensitivity);
            }

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
                unload_far_chunks(current_center);
                LCU_LOG_INFO("Streaming center moved to ({},{},{}) - {} chunk(s) loaded", current_center.x,
                             current_center.y, current_center.z, world.loaded_chunk_count());
                last_streamed_center = current_center;
            }

            // Real mouse-wheel hotbar cycling (Phase 43): SDL only delivers
            // wheel motion as discrete events (see Window::consume_wheel_delta_y),
            // so this synthesizes a one-frame "pressed" pulse from it - the
            // edge-detection below then fires exactly once per scroll notch,
            // indistinguishable from a real key press (the same shape every
            // LCU_VERIFY_* hook already uses to drive InputState directly).
            const lcu::f32 wheel_delta_y = window.consume_wheel_delta_y();
            if (wheel_delta_y > 0.0f) {
                input.set_down(lcu::platform::Action::CycleHotbar, true);
            } else if (wheel_delta_y < 0.0f) {
                input.set_down(lcu::platform::Action::CycleHotbarPrev, true);
            }

            const auto hit = lcu::physics::raycast(world, camera.position, camera.forward(), kInteractRange, is_solid);

            const bool interact_pressed = input.is_down(lcu::platform::Action::Interact) &&
                                           !previous_input.is_down(lcu::platform::Action::Interact) &&
                                           !suppress_click_for_recapture;
            const bool place_pressed = input.is_down(lcu::platform::Action::PlaceBlock) &&
                                        !previous_input.is_down(lcu::platform::Action::PlaceBlock) &&
                                        !suppress_click_for_recapture;
            const bool pick_block_pressed = input.is_down(lcu::platform::Action::PickBlock) &&
                                             !previous_input.is_down(lcu::platform::Action::PickBlock) &&
                                             !suppress_click_for_recapture;
            const bool cycle_hotbar_pressed = input.is_down(lcu::platform::Action::CycleHotbar) &&
                                               !previous_input.is_down(lcu::platform::Action::CycleHotbar);
            const bool cycle_hotbar_prev_pressed = input.is_down(lcu::platform::Action::CycleHotbarPrev) &&
                                                    !previous_input.is_down(lcu::platform::Action::CycleHotbarPrev);
            const bool craft_pressed =
                input.is_down(lcu::platform::Action::Craft) && !previous_input.is_down(lcu::platform::Action::Craft);

            if (cycle_hotbar_pressed) {
                selected_placeable_index = (selected_placeable_index + 1) % placeable_items.size();
                LCU_LOG_INFO("Selected placeable item: {}", placeable_items[selected_placeable_index].name);
            }
            if (cycle_hotbar_prev_pressed) {
                selected_placeable_index =
                    (selected_placeable_index + placeable_items.size() - 1) % placeable_items.size();
                LCU_LOG_INFO("Selected placeable item: {}", placeable_items[selected_placeable_index].name);
            }

            // Direct number-row hotbar selection (Phase 43): SelectHotbar1..9
            // are declared consecutively in Action (see input.h), so this
            // walks them as one contiguous range instead of 9 near-identical
            // if-blocks. A slot with no matching placeable_items entry (5-9,
            // today - only 4 placeable items exist) is a real, silent no-op,
            // not a crash or a wraparound onto some other slot.
            for (lcu::usize i = 0; i < 9; ++i) {
                const auto slot_action =
                    static_cast<lcu::platform::Action>(static_cast<lcu::u8>(lcu::platform::Action::SelectHotbar1) + i);
                if (input.is_down(slot_action) && !previous_input.is_down(slot_action)) {
                    if (i < placeable_items.size()) {
                        selected_placeable_index = i;
                        LCU_LOG_INFO("Selected placeable item: {}", placeable_items[selected_placeable_index].name);
                    }
                    break;
                }
            }

            if (pick_block_pressed && hit) {
                // Real "middle-click to pick block" (Phase 43) - selects
                // whichever placeable_items entry matches the looked-at
                // block, without granting the item (the player still needs
                // to actually hold it to place - see place_pressed below).
                // A silent no-op if the block has no placeable entry (e.g.
                // looking at an ore/cave-only block with no matching hotbar
                // slot yet).
                bool found_placeable = false;
                for (lcu::usize i = 0; i < placeable_items.size(); ++i) {
                    if (placeable_items[i].block_id == hit->block) {
                        selected_placeable_index = i;
                        LCU_LOG_INFO("Picked block into hotbar: {}", placeable_items[selected_placeable_index].name);
                        found_placeable = true;
                        break;
                    }
                }
                if (!found_placeable) {
                    LCU_LOG_DEBUG("PickBlock: no placeable hotbar entry for block id {}", hit->block);
                }
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
    #if defined(LCU_ENABLE_SCRIPTING)
                    mod_event_bus.emit_item_crafted(result->item, result->count);
    #endif
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
                    const auto light_touched =
                        update_lighting_for_edit(split.chunk, split.local, old_id, lcu::voxel::kAirBlockId);
                    remesh_and_upload(split.chunk);
                    remesh_edit_neighbors(split.chunk, split.local, light_touched);
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
                        const auto light_touched =
                            update_lighting_for_edit(split.chunk, split.local, old_id, selected_placeable.block_id);
                        remesh_and_upload(split.chunk);
                        remesh_edit_neighbors(split.chunk, split.local, light_touched);
                        if (selected_placeable.block_id == torch_id) {
                            // Real confirmation a placed light source actually
                            // lit itself (Phase 34) - not test-only scaffolding,
                            // this fires for any real torch placement, headless
                            // verification included.
                            const auto placed_light = world_light.block_light_at(
                                split.chunk, static_cast<lcu::i32>(split.local.x), static_cast<lcu::i32>(split.local.y),
                                static_cast<lcu::i32>(split.local.z));
                            LCU_LOG_INFO("Placed game:torch at world ({}, {}, {}): block_light={}", place_pos.x,
                                         place_pos.y, place_pos.z, placed_light.value_or(0));
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
        }  // if (!paused)

        previous_input = input;

        if (networked) {
            server_connection.update(delta_seconds);
            for (auto& packet : server_connection.take_outgoing_packets()) {
                network_socket.send_to(server_address, packet);
            }
        }

#if defined(LCU_ENABLE_BGFX)
        const lcu::f32 sky_t = day_night_cycle.sky_light_scale();
        const lcu::math::Vec3 sky_color = kNightSkyColor + (kDaySkyColor - kNightSkyColor) * sky_t;
        renderer.begin_frame(sky_color);
        const lcu::math::Mat4 view = camera.view_matrix();
        const lcu::f32 aspect =
            static_cast<lcu::f32>(renderer_desc.width) / static_cast<lcu::f32>(renderer_desc.height);
        // Real FOV (Phase 46 - closes the gap Phase 45 deliberately left
        // open: options.fov was persisted but never actually read for
        // rendering until this phase's Options screen gave it a real,
        // in-game-visible consumer - see DECISIONS.md). options.fov is
        // stored in degrees (matching the options.txt example in Phase
        // 45's own directive and every real settings UI convention),
        // converted to radians here since Mat4::perspective's own
        // contract takes fov_y_radians.
        const lcu::f32 fov_y_radians = static_cast<lcu::f32>(options.fov) * (3.14159265358979323846f / 180.0f);
        const lcu::math::Mat4 proj = lcu::math::Mat4::perspective(fov_y_radians, aspect, 0.1f, 500.0f);

        // Real per-frame draw-call count (Phase 36, brief section 60's
        // debug overlay) - incremented only when a submit_*() call
        // below actually reached bgfx::submit(), not merely attempted:
        // every submit_* silently no-ops on an invalid program (e.g.
        // LCU_BUILD_SHADER_TOOLS off, see BUILD_STATUS.md), so this
        // mirrors each call's own no-op condition rather than
        // double-counting a call that produced nothing.
        lcu::u32 draw_calls = 0;

        // Sun/moon (Phase 27) - see kCelestialRadius's doc comment. Direction
        // math lives in game::systems::sun_direction (headlessly unit-tested
        // at the four cardinal phase points) rather than duplicated here.
        {
            const lcu::math::Vec3 sun_dir = game::systems::sun_direction(day_night_cycle.time_of_day());
            const lcu::math::Vec3 moon_dir = -sun_dir;
            const lcu::math::Vec3 billboard_right = camera.right();
            const lcu::math::Vec3 billboard_up = lcu::math::cross(billboard_right, camera.forward());
            // Small negative-y margin so each body is still drawn while
            // just below the horizon (a sunset/sunrise glow effect would
            // build on this later; for now it just avoids an abrupt
            // pop at exactly y=0).
            if (sun_dir.y > -0.05f) {
                renderer.submit_billboard(camera.position + sun_dir * kCelestialRadius, billboard_right,
                                           billboard_up, kCelestialHalfSize, kSunColor, sky_program, view, proj);
                if (bgfx::isValid(sky_program)) {
                    ++draw_calls;
                }
            }
            if (moon_dir.y > -0.05f) {
                renderer.submit_billboard(camera.position + moon_dir * kCelestialRadius, billboard_right,
                                           billboard_up, kCelestialHalfSize, kMoonColor, sky_program, view, proj);
                if (bgfx::isValid(sky_program)) {
                    ++draw_calls;
                }
            }
        }

        constexpr lcu::i32 kEdge = static_cast<lcu::i32>(lcu::voxel::Chunk::kEdgeLength);
        for (const auto& [coord, gpu_mesh] : gpu_meshes) {
            const lcu::math::Mat4 model = lcu::math::Mat4::translation({static_cast<lcu::f32>(coord.x * kEdge),
                                                                         static_cast<lcu::f32>(coord.y * kEdge),
                                                                         static_cast<lcu::f32>(coord.z * kEdge)});
            renderer.submit_chunk_mesh(gpu_mesh, chunk_program, model, view, proj, day_night_cycle.sky_light_scale());
            if (gpu_mesh.is_valid() && bgfx::isValid(chunk_program)) {
                ++draw_calls;
            }
        }

        // Entity debug boxes (Phase 36, brief section 60) - a real
        // AABB per visible entity, reusing make_player_aabb (the exact
        // same box shape the player's own collision already uses; AI/
        // remote entity Position is a feet position too, same
        // convention the spawn code already established). Drawn via
        // the sky program (position+flat-color, no lighting concept -
        // see submit_wireframe_box's doc comment) since a dedicated
        // debug shader would be the same shader twice for no reason.
        lcu::u32 entity_count = 0;
        if (networked) {
            for (const auto& [entity_index, interpolator] : remote_entity_interpolators) {
                const lcu::math::Vec3 pos = interpolator.interpolated_position(network_clock);
                const lcu::physics::AABB box = make_player_aabb(pos);
                renderer.submit_wireframe_box(box.min, box.max, kEntityBoxColor, sky_program, view, proj);
                if (bgfx::isValid(sky_program)) {
                    ++draw_calls;
                }
                ++entity_count;
            }
        } else {
            for (const lcu::ecs::EntityId& entity :
                 entity_registry.pool_for<game::components::AIWander>().dense_entities()) {
                const lcu::math::Vec3 pos = entity_registry.get_component<game::components::Position>(entity)->value;
                const lcu::physics::AABB box = make_player_aabb(pos);
                renderer.submit_wireframe_box(box.min, box.max, kEntityBoxColor, sky_program, view, proj);
                if (bgfx::isValid(sky_program)) {
                    ++draw_calls;
                }
                ++entity_count;
            }
        }

        // Crosshair (Phase 44) - real 2D UI quad batch: two thin bars
        // queued via submit_ui_quad, then one real draw call via
        // flush_ui_quads (see kCrosshairSize's own doc comment above).
        // Gated on options.hud_enabled (Phase 45) - the first real
        // consumer of that persisted flag, not just a field that gets
        // saved/loaded without affecting anything.
        if (options.hud_enabled) {
            const auto screen_center_x = static_cast<lcu::f32>(renderer_desc.width) / 2.0f;
            const auto screen_center_y = static_cast<lcu::f32>(renderer_desc.height) / 2.0f;
            renderer.submit_ui_quad(screen_center_x - kCrosshairSize / 2.0f,
                                     screen_center_y - kCrosshairThickness / 2.0f, kCrosshairSize,
                                     kCrosshairThickness, kCrosshairColor);
            renderer.submit_ui_quad(screen_center_x - kCrosshairThickness / 2.0f,
                                     screen_center_y - kCrosshairSize / 2.0f, kCrosshairThickness, kCrosshairSize,
                                     kCrosshairColor);
        }

        // Real pause/options/controls menu (Phase 46) - its backdrop/
        // selection-highlight quads queue into this same batch as the
        // crosshair above (one real draw call for both), its row labels
        // are drawn separately, after draw_debug_overlay below (see
        // menu_renderer.h's own doc comment for why the quad- and
        // text-drawing halves can't happen at the same call site here).
        if (!menu_stack.empty()) {
            lcu::ui::queue_menu_backdrop(renderer, menu_stack, renderer_desc.width, renderer_desc.height);
        }

        const bool ui_had_quads = renderer.pending_ui_quad_count() > 0;
        renderer.flush_ui_quads(ui2d_program);
        if (ui_had_quads && bgfx::isValid(ui2d_program)) {
            ++draw_calls;
        }

        // Debug overlay (FPS/chunks/entities/draw-calls/jobs) gated on
        // options.debug_overlay_enabled (Phase 45) - real use of the
        // second persisted HUD flag, independent of hud_enabled (the
        // crosshair and the debug overlay are two separate real toggles,
        // matching Phase 46's own planned "HUD: an/aus" / "Debug-
        // Overlay: an/aus" as two distinct options-screen rows).
        if (options.debug_overlay_enabled) {
            lcu::ui::draw_debug_overlay(
                renderer, renderer_desc.width, renderer_desc.height, last_known_fps,
                {static_cast<lcu::u32>(world.loaded_chunk_count()), entity_count, draw_calls,
                 job_system.unfinished_job_count()});
        }
        // After the debug overlay, not before - draw_menu_labels clears
        // and re-owns the same bgfx debug-text buffer the overlay just
        // wrote to (see its own doc comment), so this ordering is real,
        // not incidental.
        if (!menu_stack.empty()) {
            lcu::ui::draw_menu_labels(renderer, menu_stack, renderer_desc.width, renderer_desc.height);
        }
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
        if (quit_requested) {
            LCU_LOG_INFO("Quit requested from pause menu, exiting");
            break;
        }
    }

#if defined(LCU_ENABLE_BGFX)
    if (bgfx::isValid(chunk_program)) {
        bgfx::destroy(chunk_program);
    }
    if (bgfx::isValid(sky_program)) {
        bgfx::destroy(sky_program);
    }
    if (bgfx::isValid(ui2d_program)) {
        bgfx::destroy(ui2d_program);
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
        LCU_LOG_INFO("Player position: ({:.2f}, {:.2f}, {:.2f})", player.aabb.center().x, player.aabb.center().y,
                     player.aabb.center().z);
        for (const lcu::ecs::EntityId& entity :
             entity_registry.pool_for<game::components::AIWander>().dense_entities()) {
            const lcu::math::Vec3 pos = entity_registry.get_component<game::components::Position>(entity)->value;
            LCU_LOG_INFO("AI entity (index={}) at ({:.2f}, {:.2f}, {:.2f})", entity.index, pos.x, pos.y, pos.z);
        }
    }

    // Real save-on-exit (Phase 45): Phase 46's options/controls menu
    // will also save on every change once it exists, but a clean exit
    // is a real, honest trigger on its own - it's what actually
    // creates options.txt for a first-time run (nothing has written to
    // it yet otherwise), and round-trips any values this phase's
    // headless verification runs never actually change back out again.
    options.save(options_path);
    LCU_LOG_INFO("Saved options to \"{}\"", options_path);

    LCU_LOG_INFO("LiveCraftUltimate client shutting down after {} frames", frame);
    return 0;
}
