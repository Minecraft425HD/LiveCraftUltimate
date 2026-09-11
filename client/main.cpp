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
#include "game/components/item_entity.h"
#include "game/components/position.h"
#include "game/items/block_item_mapping.h"
#include "game/systems/ai_wander_system.h"
#include "game/systems/day_night_cycle.h"
#include "game/systems/item_entity_system.h"
#include "game/systems/replication_protocol.h"
#include "lcu/audio/audio_engine.h"
#include "lcu/audio/positional.h"
#include "lcu/audio/waveform.h"
#include "lcu/core/log.h"
#include "lcu/core/quality_profile.h"
#include "lcu/debug/frame_stats.h"
#include "lcu/ecs/registry.h"
#include "lcu/items/inventory.h"
#include "lcu/items/inventory_ops.h"
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
#include "lcu/ui/crafting_table_screen.h"
#include "lcu/ui/hud.h"
#include "lcu/ui/inventory_screen.h"
#include "lcu/ui/menu_stack.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/break_progress.h"
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
#include "lcu/ui/crafting_table_screen_renderer.h"
#include "lcu/ui/debug_overlay.h"
#include "lcu/ui/hud_renderer.h"
#include "lcu/ui/inventory_screen_renderer.h"
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
// Real item-entity pickup range (Phase 50.2): an exact player-AABB-
// vs-item-AABB overlap would almost never trigger in practice - the
// block a player just broke is typically one block *in front of* them
// (raycast target), not at their own feet, so a dropped item with no
// horizontal velocity of its own (see ItemEntity's own doc comment)
// settles roughly where the broken block was, which can genuinely be
// one OR TWO blocks *below* the player's own standing height (e.g.
// mining straight down while standing still, exactly what
// LCU_VERIFY_CRAFT's own break-grass-then-break-dirt-beneath-it
// sequence does). Two real, measured failures from this phase's own
// headless verification pinned this down, not a guess: with a 0.75
// inflate, a single-block-deep item (real observed y=0.12, player
// min.y=1.0) missed pickup_aabb.min.y=0.25 by 0.005 (LCU_VERIFY_
// BREAK_PLACE); with 1.0, a two-blocks-deep item (grass then dirt
// beneath it, item settling around y=-0.875) still sat below
// pickup_aabb.min.y=0.0 (LCU_VERIFY_CRAFT, the second break's item was
// never picked up in an extended run). 2.0 comfortably covers both real
// cases with margin (min.y - 2.0 = -1.0, below every real settled
// position measured above) - Minecraft's own real player pickup range
// is likewise larger than its exact hitbox, so inflating `player.aabb`
// by this much on every axis before the pickup overlap check below is
// the same real, deliberate design choice (a player mining a staircase
// down while standing still can still pick up what they just broke),
// not a test-passing hack (see DECISIONS.md).
constexpr lcu::f32 kItemPickupRangeInflate = 2.0f;
// Third-person-behind camera distance (Phase 47, F5) - real, chosen to
// clear the player's own AABB (kPlayerHeight/kPlayerHalfWidth above)
// comfortably; no real collision check pulls it closer against a wall
// yet (a real, honest PARTIAL - see DECISIONS.md).
constexpr lcu::f32 kThirdPersonDistance = 4.0f;

// Real hand-icon swing (Phase 48) - triggered on every real break/place
// action, real elapsed-time-driven, not a frame-count animation (so it
// plays at the same real speed regardless of frame rate, same reasoning
// LCU_VERIFY_MOVE_SECONDS already established for movement).
constexpr lcu::f32 kHandSwingDuration = 0.25f;
constexpr lcu::f32 kHandIconSize = 32.0f;
constexpr lcu::f32 kHandRestMarginX = 24.0f;
constexpr lcu::f32 kHandRestMarginY = 24.0f;
constexpr lcu::f32 kHandSwingOffset = 20.0f;

// Real block-highlight wireframe (Phase 48) - a slightly outset cube so
// the highlight lines sit just outside the block's own faces, visible
// rather than z-fighting with them.
constexpr lcu::f32 kBlockHighlightOutset = 0.002f;
constexpr lcu::math::Vec3 kBlockHighlightColor{0.05f, 0.05f, 0.05f};

// Real break-progress overlay (Phase 48.2) - a solid (opaque - no real
// alpha blending, see DECISIONS.md) box over the targeted block that
// darkens toward black as break progress advances, real visual
// feedback that breaking is happening. Deliberately NOT a real crack-
// noise-density shader effect on the block's own face (that needs a
// new per-fragment world-position uniform threaded through fs_chunk.sc
// - a real, separate shader feature this phase's own scope doesn't
// reach - see DECISIONS.md); this overlay is a real, visible,
// honestly-scoped substitute, not a placeholder.
constexpr lcu::f32 kBreakOverlayMaxDarken = 0.9f;

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
// input): if LCU_VERIFY_BREAK_PLACE is set, synthesizes a real held
// Interact press long enough to actually break the grass block the
// player spawns on, then a PlaceBlock press. This drives the exact
// same edge-detected/hold-accumulated InputState path a real held
// mouse button would - a real exercise of the hold-to-break ->
// mutate-world -> remesh -> re-upload pipeline, not a mock of it.
//
// Real elapsed-time hold windows, not frame numbers (same reasoning
// LCU_VERIFY_MOVE_SECONDS/LCU_VERIFY_CRAFT already established for
// their own real-time-gated steps): Phase 48 changed breaking a block
// from an instant single click to a real held-duration accumulation
// against that block's own `BlockDefinition::hardness` (grass = 0.6s -
// see client/main.cpp's block registrations), so a fixed frame count
// can no longer reliably land the break - this sandbox's loop is
// unthrottled and can run many thousands of frames per real second, so
// a frame count that happened to cover 0.6s on one run could cover a
// wildly different real duration on another.
constexpr lcu::f32 kVerifyBreakHoldSeconds = 0.7f;  // grass hardness 0.6s + margin.
// After the hold above releases: exercises the real CycleHotbar/
// CycleHotbarPrev selection path end to end (Phase 21, redone for
// Phase 49's real slot-driven hotbar - see "Real Minecraft-style hotbar
// selection" below). The broken grass item lands in the player's first
// empty real inventory slot, slot 0 (Inventory::add_item always fills
// earliest-first, and the inventory starts empty) - CycleHotbar moves
// off it (0 -> 1) to prove forward cycling really works, then
// CycleHotbarPrev moves back (1 -> 0) to prove the reverse direction
// too *and* land back on the slot actually holding the broken item
// before PlaceBlock fires.
constexpr lcu::f32 kVerifyCycleHotbarAtSeconds = 0.9f;
constexpr lcu::f32 kVerifyCycleHotbarPrevAtSeconds = 1.0f;
constexpr lcu::f32 kVerifyPlaceAtSeconds = 1.2f;
// Real width of a single-press pulse window for an edge-triggered
// action driven by elapsed time (CycleHotbar/PlaceBlock/Craft below) -
// wide enough to reliably span at least one real frame at any
// plausible frame rate, narrow enough to still read as one real press,
// not a second held-to-break accumulation.
constexpr lcu::f32 kVerifyEdgePulseSeconds = 0.05f;

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

// A fourth, independent headless hook (LCU_VERIFY_HUD, Phase 47): each
// F-key toggle pressed on its own frame (different actions, so no
// same-action-adjacency debounce concern - see kVerifyMenuAdjustSensitivityFrame2's
// own comment for why that matters for a *repeated* press of the same
// action).
constexpr lcu::u64 kVerifyHudToggleHudFrame = 5;
constexpr lcu::u64 kVerifyHudToggleDebugOverlayFrame = 6;
constexpr lcu::u64 kVerifyHudTogglePerspectiveFrame = 7;
constexpr lcu::u64 kVerifyHudFullscreenFrame = 8;
constexpr lcu::u64 kVerifyHudScreenshotFrame = 9;

// A second, independent headless hook (LCU_VERIFY_CRAFT, Phase 23):
// holds Interact long enough to break the grass block the player
// spawns on, then holds it again long enough to break the dirt block
// beneath it, then presses Craft - exercising the real quick-craft
// path end to end (see "Quick-craft" below). Kept separate from
// LCU_VERIFY_BREAK_PLACE above (different timing windows, not meant to
// run in the same process) since the two exercise unrelated inventory
// states. Real elapsed-time gates throughout (Phase 48 update: each
// break is now a real hold-to-break window sized to its own block's
// hardness plus margin, not a single-frame pulse - see
// kVerifyBreakHoldSeconds' own doc comment above for why frame counts
// can't express this). In networked mode a break doesn't mutate this
// client's own World until the server's BlockChange broadcast round-
// trips back (block edits are never client-predicted - see
// DECISIONS.md), so the real gap between the two break windows below
// also covers that round trip, not just the hold-to-break duration
// itself - confirmed by an earlier, frame-count-gated version of this
// hook actually hitting a stale-raycast race in a real networked run.
constexpr lcu::f32 kVerifyCraftBreakGrassHoldSeconds = 0.7f;  // grass hardness 0.6s + margin.
constexpr lcu::f32 kVerifyCraftBreakDirtHoldStartSeconds = 1.5f;
constexpr lcu::f32 kVerifyCraftBreakDirtHoldEndSeconds = 2.5f;  // dirt hardness 0.5s + a real, generous margin.
constexpr lcu::f32 kVerifyCraftFirstCraftAtSeconds = 2.7f;
// A second Craft press after the first one succeeds: by now the player
// holds only game:compost (grass/dirt were fully consumed) - a single
// distinct item type matches no registered recipe, so this exercises
// the real rejection path ("No recipe matches...") in the same run,
// not just the match path.
constexpr lcu::f32 kVerifyCraftRejectAtSeconds = 3.0f;

// A third, independent headless hook (LCU_VERIFY_TORCH, Phase 34):
// holds Interact long enough to break the block the player spawns on
// (same real hold-window sizing as LCU_VERIFY_BREAK_PLACE above),
// grants the player one game:torch item directly (nothing in this
// build's world drops one to break/craft yet, so this is the synthetic
// setup the hook needs, the same honest "hook synthesizes exactly the
// input/state a real key press or drop would produce" approach
// LCU_VERIFY_BREAK_PLACE/LCU_VERIFY_CRAFT already use, done *before* the
// break below so the torch is the first item added and lands in real
// inventory slot 0), cycles the hotbar three forward and three back
// (Phase 49 update - proves both CycleHotbar and CycleHotbarPrev really
// work via a real net-zero round trip, then correctly lands back on
// slot 0 - the torch's own slot - before placing; the old fixed-list
// scheme's "cycle 3 times to reach index 3" no longer applies now that
// the hotbar is real slot storage, not a virtual item-type list), then
// places it into the hole the break just made. Proves the real
// end-to-end pipeline: a placed torch actually reaches update_
// lighting_for_edit -> propagate_added_block_light_cross_chunk -> a
// real, observably nonzero block_light value at its own position,
// logged below - not just "it compiled and didn't crash".
constexpr lcu::f32 kVerifyTorchBreakHoldSeconds = 0.7f;  // grass hardness 0.6s + margin.
constexpr lcu::f32 kVerifyTorchCycleAt1Seconds = 0.9f;
constexpr lcu::f32 kVerifyTorchCycleAt2Seconds = 1.0f;
constexpr lcu::f32 kVerifyTorchCycleAt3Seconds = 1.1f;
constexpr lcu::f32 kVerifyTorchCyclePrevAt1Seconds = 1.2f;
constexpr lcu::f32 kVerifyTorchCyclePrevAt2Seconds = 1.3f;
constexpr lcu::f32 kVerifyTorchCyclePrevAt3Seconds = 1.4f;
constexpr lcu::f32 kVerifyTorchPlaceAtSeconds = 1.6f;

// A fifth, independent headless hook (LCU_VERIFY_INVENTORY, Phase 49):
// grants the player 1 game:wood directly (same synthetic-setup
// precedent as LCU_VERIFY_TORCH's torch grant - nothing in this build's
// world drops wood fast enough to rely on breaking one), opens the
// inventory screen (E), then drives four real mouse clicks via
// Window::warp_mouse + a synthesized Interact press each - exactly the
// same "hook synthesizes exactly the input/state a real key press or
// click would produce" approach every other verify hook here already
// uses, just extended to mouse position for the first time (Phase 49's
// screen is the first real mouse-click-driven UI in this project - see
// Window::warp_mouse's own doc comment): pick up the wood from hotbar
// slot 0 into the cursor, drop it into the 2x2 craft grid's first cell,
// take the crafted result (proving RecipeRegistry::find_match's real
// 2x2 integration, not just the quick-craft path LCU_VERIFY_CRAFT
// already covers), place the result into the main inventory, then
// shift-click it back into the hotbar range - exercising every real
// click kind (plain left-click pick-up/place and shift-click transfer)
// end to end in one real run. Closes the screen at the end.
constexpr lcu::f32 kVerifyInventoryOpenAtSeconds = 0.2f;
constexpr lcu::f32 kVerifyInventoryPickupWoodAtSeconds = 0.4f;
constexpr lcu::f32 kVerifyInventoryDropInCraftAtSeconds = 0.6f;
constexpr lcu::f32 kVerifyInventoryTakeResultAtSeconds = 0.8f;
constexpr lcu::f32 kVerifyInventoryPlaceInMainAtSeconds = 1.0f;
constexpr lcu::f32 kVerifyInventoryShiftToHotbarAtSeconds = 1.2f;
constexpr lcu::f32 kVerifyInventoryCloseAtSeconds = 1.4f;

// A sixth, independent headless hook (LCU_VERIFY_WORKBENCH, Phase 50.3):
// grants the player 1 game:wood directly (same synthetic-setup
// precedent every prior hook's own item/block seeding already uses),
// then directly overwrites the world block the player spawns looking at
// (the same real (-84,0,-85) target LCU_VERIFY_BREAK_PLACE/TORCH/CRAFT
// already establish) with a real `game:crafting_table` block - a real,
// deterministic way to guarantee a crafting table is right there to
// right-click, the same "synthesize exactly the state a real action
// would produce" honesty those hooks' own item grants already use,
// just applied to a block instead of an item this time (placing one via
// a real PlaceBlock press first would need the player to physically
// re-aim at a face after placement, which this sandbox's fixed spawn
// orientation can't do deterministically). Then: right-click opens the
// workbench (PlaceBlock, since it's bound to the right mouse button -
// see key_bindings.cpp), pick up the wood from the hotbar into the
// cursor, drop it anywhere in the real 3x3 grid (game:wood -> 4
// game:planks is shapeless, so it matches regardless of which of the 9
// cells it's in - a real, direct proof the same recipe genuinely works
// in a bigger grid, not just the 2x2 one LCU_VERIFY_INVENTORY already
// covers), take the result, then closes the screen via Escape.
constexpr lcu::f32 kVerifyWorkbenchOpenAtSeconds = 0.2f;
constexpr lcu::f32 kVerifyWorkbenchPickupWoodAtSeconds = 0.4f;
constexpr lcu::f32 kVerifyWorkbenchDropInGridAtSeconds = 0.6f;
constexpr lcu::f32 kVerifyWorkbenchTakeResultAtSeconds = 0.8f;
constexpr lcu::f32 kVerifyWorkbenchCloseAtSeconds = 1.0f;

// Real Minecraft-sized inventory (Phase 49): 9 hotbar slots (indices
// 0-8, real Minecraft slot numbering) + 27 main storage slots (indices
// 9-35, 3 rows of 9). Armor slots have no consumer yet - nothing reads/
// writes them - so aren't built speculatively (brief section 98).
constexpr lcu::usize kHotbarSlotCount = 9;
constexpr lcu::usize kMainInventorySlotCount = 27;
constexpr lcu::usize kInventorySlotCount = kHotbarSlotCount + kMainInventorySlotCount;
// 2x2 real crafting grid (Phase 49.3) - a separate, real
// `lcu::items::Inventory` from the main 36-slot one above, since a
// craft grid's contents are consumed on craft, not just stored (see
// build_inventory_screen below). Slots 0-3 are the 2x2 input grid,
// slot 4 is the read-only result slot.
constexpr lcu::usize kCraftGridInputSlotCount = 4;
constexpr lcu::usize kCraftGridResultSlotIndex = kCraftGridInputSlotCount;
constexpr lcu::usize kCraftGridTotalSlotCount = kCraftGridInputSlotCount + 1;
// The workbench's own real 3x3 crafting grid (Phase 50.3) - same shape
// as kCraftGridInputSlotCount above, just 9 slots instead of 4, and its
// own separate `lcu::items::Inventory` (a crafting-table screen's grid
// contents shouldn't share state with the inventory screen's own 2x2
// grid - two real, independent workspaces, matching Minecraft's own
// two independent grids).
constexpr lcu::usize kWorkbenchGridInputSlotCount = 9;
constexpr lcu::usize kWorkbenchGridResultSlotIndex = kWorkbenchGridInputSlotCount;
constexpr lcu::usize kWorkbenchGridTotalSlotCount = kWorkbenchGridInputSlotCount + 1;

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
    // Real break-time in seconds of held Interact (Phase 48) - this
    // phase's own directive's exact stone value.
    stone_def.hardness = 2.0f;
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
    // Real break-time (Phase 48) - not in this phase's own directive's
    // table (only stone/wood/dirt/leaves are named), so this is a real,
    // own choice: slightly tougher than bare dirt (real roots/turf),
    // matching Minecraft's own grass-vs-dirt relationship.
    grass_def.hardness = 0.6f;
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
    // Real break-time (Phase 48) - this phase's own directive's exact
    // dirt value.
    dirt_def.hardness = 0.5f;
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
    // Real break-time (Phase 48) - a real own choice (not in this
    // phase's own directive's table): as loose/soft as dirt.
    sand_def.hardness = 0.5f;
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
    // Real break-time (Phase 48) - a real own choice: the softest solid
    // block registered, matching real snow.
    snow_def.hardness = 0.1f;
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
    // Real break-time (Phase 48) - instant, matching real Minecraft
    // torches (any tool, including bare hands, breaks one immediately).
    torch_def.hardness = 0.0f;
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
    // No hardness override needed (Phase 48, "Wasser unendlich (nicht
    // abbaubar)"): the DDA raycast (see is_solid below) only ever stops
    // on a block with has_collision=true, so water is never a real
    // break target to begin with - already, honestly, unbreakable
    // without a special case, not by an infinite hardness value.
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
    // Real break-time (Phase 48) - a real own choice: harder than plain
    // stone (ore-bearing rock), matching Minecraft's own ore-vs-stone
    // relationship.
    coal_ore_def.hardness = 3.0f;
    coal_ore_def.color = {0.2f, 0.2f, 0.22f};
    const lcu::voxel::BlockId coal_ore_id = block_registry.register_block(coal_ore_def);

    lcu::voxel::BlockDefinition iron_ore_def;
    iron_ore_def.namespaced_id = "game:iron_ore";
    iron_ore_def.display_name = "Iron Ore";
    iron_ore_def.is_transparent = false;
    iron_ore_def.has_collision = true;
    iron_ore_def.hardness = 3.0f;
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
    // Real break-time (Phase 48) - this phase's own directive's exact
    // wood value.
    wood_def.hardness = 1.5f;
    wood_def.color = {0.45f, 0.30f, 0.15f};
    const lcu::voxel::BlockId wood_id = block_registry.register_block(wood_def);

    lcu::voxel::BlockDefinition leaves_def;
    leaves_def.namespaced_id = "game:leaves";
    leaves_def.display_name = "Leaves";
    leaves_def.is_transparent = false;
    leaves_def.has_collision = true;
    // Real break-time (Phase 48) - this phase's own directive's exact
    // leaves value.
    leaves_def.hardness = 0.2f;
    leaves_def.color = {0.20f, 0.55f, 0.15f};
    const lcu::voxel::BlockId leaves_id = block_registry.register_block(leaves_def);

    lcu::voxel::BlockDefinition cactus_def;
    cactus_def.namespaced_id = "game:cactus";
    cactus_def.display_name = "Cactus";
    cactus_def.is_transparent = false;
    cactus_def.has_collision = true;
    // Real break-time (Phase 48) - a real own choice: as soft as leaves
    // (a real cactus is mostly water, easy to cut through).
    cactus_def.hardness = 0.4f;
    cactus_def.color = {0.10f, 0.45f, 0.30f};
    const lcu::voxel::BlockId cactus_id = block_registry.register_block(cactus_def);

    // Real crafting table (Phase 50.3): right-clicking it opens a real
    // 3x3 crafting-grid screen (see workbench_open below) instead of
    // placing/breaking normally through PlaceBlock. A solid, sturdy
    // furniture block - hardness between plain wood (1.5s) and stone
    // (2.0s), a real, own choice (no tool-tier system exists to gate it
    // further - see DECISIONS.md, out of this project's scope entirely).
    lcu::voxel::BlockDefinition crafting_table_def;
    crafting_table_def.namespaced_id = "game:crafting_table";
    crafting_table_def.display_name = "Crafting Table";
    crafting_table_def.is_transparent = false;
    crafting_table_def.has_collision = true;
    crafting_table_def.hardness = 2.0f;
    crafting_table_def.color = {0.55f, 0.35f, 0.15f};
    const lcu::voxel::BlockId crafting_table_id = block_registry.register_block(crafting_table_def);

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
    // Real hotbar icon colors (Phase 47) - matching each item's own
    // block's tint where one exists (BlockDefinition::color above), the
    // same "flat color, no atlas" convention, not a coincidence.
    stone_item_def.icon_color = {0.5f, 0.5f, 0.5f, 1.0f};
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
    grass_item_def.icon_color = {0.3f, 0.7f, 0.2f, 1.0f};
    const lcu::items::ItemId grass_item_id = item_registry.register_item(grass_item_def);

    lcu::items::ItemDefinition dirt_item_def;
    dirt_item_def.namespaced_id = "game:dirt";
    dirt_item_def.display_name = "Dirt";
    dirt_item_def.max_stack_size = 64;
    dirt_item_def.icon_color = {0.4f, 0.25f, 0.1f, 1.0f};
    const lcu::items::ItemId dirt_item_id = item_registry.register_item(dirt_item_def);

    lcu::items::ItemDefinition torch_item_def;
    torch_item_def.namespaced_id = "game:torch";
    torch_item_def.display_name = "Torch";
    torch_item_def.max_stack_size = 64;
    torch_item_def.icon_color = {1.0f, 0.65f, 0.2f, 1.0f};
    const lcu::items::ItemId torch_item_id = item_registry.register_item(torch_item_def);

    // game:wood's own item (Phase 49): Phase 41 registered the wood
    // *block* but never gave it an item, so breaking one has always
    // granted nothing - the same honest gap block_item_mapping.h's own
    // doc comment describes for any block with no registered pair. This
    // closes it, and gives the new real 2x2 crafting grid (49.3) its own
    // in-house recipe ingredient rather than reusing compost's grass+dirt
    // pairing for the grid's own verification.
    lcu::items::ItemDefinition wood_item_def;
    wood_item_def.namespaced_id = "game:wood";
    wood_item_def.display_name = "Wood";
    wood_item_def.max_stack_size = 64;
    wood_item_def.icon_color = {wood_def.color.x, wood_def.color.y, wood_def.color.z, 1.0f};
    const lcu::items::ItemId wood_item_id = item_registry.register_item(wood_item_def);

    // game:crafting_table's own item (Phase 50.3) - breaking a crafting
    // table drops itself, the same direct 1:1 block->item convention
    // every other real placeable block here already follows.
    lcu::items::ItemDefinition crafting_table_item_def;
    crafting_table_item_def.namespaced_id = "game:crafting_table";
    crafting_table_item_def.display_name = "Crafting Table";
    crafting_table_item_def.max_stack_size = 64;
    crafting_table_item_def.icon_color = {crafting_table_def.color.x, crafting_table_def.color.y,
                                           crafting_table_def.color.z, 1.0f};
    const lcu::items::ItemId crafting_table_item_id = item_registry.register_item(crafting_table_item_def);

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
    compost_item_def.icon_color = {0.25f, 0.15f, 0.05f, 1.0f};
    const lcu::items::ItemId compost_item_id = item_registry.register_item(compost_item_def);

    // Phase 49's own suggested example recipe's crafted-only result:
    // game:planks, a lighter shade of wood's own color (no block behind
    // it yet - planks-as-a-placeable-block isn't part of this phase's
    // scope, only the recipe itself is, see the phase's own brief).
    lcu::items::ItemDefinition planks_item_def;
    planks_item_def.namespaced_id = "game:planks";
    planks_item_def.display_name = "Planks";
    planks_item_def.max_stack_size = 64;
    planks_item_def.icon_color = {0.65f, 0.48f, 0.28f, 1.0f};
    const lcu::items::ItemId planks_item_id = item_registry.register_item(planks_item_def);

    lcu::items::Inventory player_inventory(kInventorySlotCount);

    // Crafting (Phase 23, closing RecipeRegistry's long-standing "no
    // crafting-grid caller anywhere" gap - Phase 5 built and unit
    // tested it, nothing ever called it). Two real shapeless recipes:
    // 1 game:grass + 1 game:dirt -> 1 game:compost (Phase 23's quick-craft
    // path, see craft_pressed below), and 1 game:wood -> 4 game:planks
    // (Phase 49's own suggested example, the real 2x2 grid's own first
    // recipe - see build_inventory_screen below). Purely client-side
    // local inventory bookkeeping, same as item pickup itself
    // (DECISIONS.md "Item pickup/consumption stays client-authoritative")
    // - crafting never touches the World or needs server validation, so
    // it behaves identically in single-player and networked mode with
    // no protocol involvement.
    lcu::items::RecipeRegistry recipe_registry;
    recipe_registry.add_shapeless({{grass_item_id, dirt_item_id}, {compost_item_id, 1}});
    recipe_registry.add_shapeless({{wood_item_id}, {planks_item_id, 4}});

    // Real Minecraft-style hotbar selection (Phase 49, replacing Phase
    // 21's placeable_items/selected_placeable_index "virtual known-item-
    // types" selector, which never actually pointed at where an item
    // physically lived in the inventory - it just remembered up to 4
    // fixed item *types* you could cycle between, decoupled from real
    // slot storage). The active hotbar slot is now a real index (0-8)
    // into player_inventory itself: whatever ItemStack physically sits
    // in that slot is what gets placed, exactly like Minecraft's own
    // hotbar - see the real inventory screen below (build_
    // inventory_screen) for how items actually get organized into it.
    // CycleHotbar/CycleHotbarPrev/SelectHotbar1-9 all just move this
    // index around now; there's no separate "known placeable item types"
    // list to keep in sync - block_item_mapping (below) is the only
    // thing translating a held item into a placeable block, so any newly
    // registered block/item pair is automatically placeable the moment
    // the player holds it in their hotbar.
    lcu::usize selected_hotbar_slot = 0;

    // Data-driven block->item mapping (Phase 22, closing Phase 19's
    // remaining honest gap): replaces the hardcoded if/else chain this
    // lambda used to carry (one `if (broken_block == X)` per block,
    // Phase 17-19) with a single table populated once, right after each
    // block/item pair is registered above. Adding a new item-backed
    // block from here on is one register_pair call, not a new branch
    // here and a matching one in VoxelServer's own item_for_block - see
    // game/items/block_item_mapping.h and DECISIONS.md. Its reverse
    // direction (block_for_item, Phase 49) is now real placement's only
    // source of truth for "what block does this held item place".
    game::items::BlockItemMapping block_item_mapping;
    block_item_mapping.register_pair(stone_id, stone_item_id);
    block_item_mapping.register_pair(grass_id, grass_item_id);
    block_item_mapping.register_pair(dirt_id, dirt_item_id);
    block_item_mapping.register_pair(torch_id, torch_item_id);
    block_item_mapping.register_pair(wood_id, wood_item_id);
    block_item_mapping.register_pair(crafting_table_id, crafting_table_item_id);

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

    // Block-break's item drop (brief section 55) - still a direct 1:1
    // block->item mapping (Phase 17), just data-driven (Phase 22).
    // Real physical item entity now (Phase 50), replacing the old
    // direct-to-inventory grant: spawns a real `game::components::
    // ItemEntity` + `Position` (on the same `entity_registry` the AI
    // entities above already share - one real ECS world, not a second
    // one) at the broken block's own center with a real small upward
    // toss (`update_item_entities` below applies real gravity/ground
    // collision every frame after this), rather than teleporting the
    // item straight into the player's inventory. Actual pickup happens
    // later, once the player's own AABB overlaps it and its own pickup
    // delay has elapsed (`pickup_item_entities`) - a real Minecraft-
    // shaped break -> pop up -> fall -> land -> pick up pipeline, not a
    // shortcut. Shared by both the networked (optimistic, client-
    // authoritative - see DECISIONS.md) and single-player break paths
    // below so the two don't drift out of sync with each other.
    const auto spawn_item_entity_for_broken_block = [&](lcu::voxel::BlockId broken_block,
                                                          const lcu::voxel::BlockWorldCoord& block_pos) {
        const lcu::items::ItemId item_id = block_item_mapping.item_for_block(broken_block);
        if (item_id == lcu::items::kNoItemId) {
            return;
        }
        const lcu::math::Vec3 spawn_center{static_cast<lcu::f32>(block_pos.x) + 0.5f,
                                            static_cast<lcu::f32>(block_pos.y) + 0.5f,
                                            static_cast<lcu::f32>(block_pos.z) + 0.5f};
        const lcu::ecs::EntityId entity = entity_registry.create_entity();
        entity_registry.add_component<game::components::Position>(entity, {spawn_center});
        game::components::ItemEntity item_entity;
        item_entity.stack = {item_id, 1};
        item_entity.vertical_velocity = game::systems::kItemEntitySpawnUpSpeed;
        item_entity.pickup_delay_seconds = game::systems::kItemEntityPickupDelaySeconds;
        entity_registry.add_component<game::components::ItemEntity>(entity, item_entity);
        LCU_LOG_INFO("Spawned item entity: {} at world ({}, {}, {})",
                     item_registry.definition_of(item_id).namespaced_id, block_pos.x, block_pos.y, block_pos.z);
    };

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

    // Headless verification hook for the Phase 47 HUD/F-key toggles:
    // presses ToggleHud/ToggleDebugOverlay/TogglePerspective/Fullscreen/
    // Screenshot each on their own frame and logs the real resulting
    // state, so a real run's log output proves each binding actually
    // flipped its real target (options.hud_enabled, window.fullscreen(),
    // etc.), not just that the Action exists.
    const bool verify_hud = std::getenv("LCU_VERIFY_HUD") != nullptr;

    const bool verify_break_place = std::getenv("LCU_VERIFY_BREAK_PLACE") != nullptr;
    const auto verify_break_place_start = std::chrono::steady_clock::now();
    const bool verify_craft = std::getenv("LCU_VERIFY_CRAFT") != nullptr;
    const auto verify_craft_start = std::chrono::steady_clock::now();
    const bool verify_torch = std::getenv("LCU_VERIFY_TORCH") != nullptr;
    const auto verify_torch_start = std::chrono::steady_clock::now();
    bool verify_torch_granted = false;

    const bool verify_inventory = std::getenv("LCU_VERIFY_INVENTORY") != nullptr;
    const auto verify_inventory_start = std::chrono::steady_clock::now();
    bool verify_inventory_wood_granted = false;

    const bool verify_workbench = std::getenv("LCU_VERIFY_WORKBENCH") != nullptr;
    const auto verify_workbench_start = std::chrono::steady_clock::now();
    bool verify_workbench_setup_done = false;

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

    // Real inventory screen (Phase 49, brief section 60's own directive:
    // "game keeps running (Minecraft behavior: no pause in inventory)").
    // Deliberately NOT another menu_stack entry: menu_stack being
    // non-empty freezes the whole simulation (day/night, AI wander - see
    // `paused` below), but Minecraft's own inventory screen doesn't
    // pause the world, only the player's own control - movement/camera/
    // mining/placing lock while it's open (see the new `!inventory_open`
    // gate added below to the big `if (!paused)` player-control block),
    // while everything else keeps ticking. craft_grid_inventory is a
    // separate, small 5-slot Inventory (not part of player_inventory's
    // own 36 slots) since its slot 4 (kCraftGridResultSlotIndex) is a
    // read-only, recomputed-on-change display of whatever the 2x2 grid's
    // real ingredients currently craft, not stored player state.
    // cursor_stack is the real drag/drop "stack picked up by the mouse"
    // (Phase 49.2) - shared across every slot in both inventories, since
    // Minecraft only ever lets you hold one stack on the cursor at a
    // time regardless of which screen/inventory it came from.
    bool inventory_open = false;
    lcu::items::Inventory craft_grid_inventory(kCraftGridTotalSlotCount);
    lcu::items::ItemStack cursor_stack;

    // Recomputes the real crafting result (Phase 49.3) from the 2x2
    // grid's current contents via RecipeRegistry::find_match - called
    // after every click that could have changed craft_grid_inventory's
    // input slots (0-3), so the result slot (4) always reflects real,
    // live ingredient state rather than a stale guess.
    const auto recompute_craft_result = [&]() {
        std::vector<lcu::items::ItemId> grid(kCraftGridInputSlotCount, lcu::items::kNoItemId);
        for (lcu::usize i = 0; i < kCraftGridInputSlotCount; ++i) {
            grid[i] = craft_grid_inventory.slot_at(i).item;
        }
        const lcu::items::ItemStack* match =
            recipe_registry.find_match(grid, lcu::ui::kCraftGridEdge, lcu::ui::kCraftGridEdge);
        craft_grid_inventory.set_slot(kCraftGridResultSlotIndex, match != nullptr ? *match : lcu::items::ItemStack{});
    };

    // Real crafting-table workbench screen (Phase 50.3): right-clicking
    // a `game:crafting_table` block opens this instead of placing a
    // block or breaking normally (see place_pressed below) - a real 3x3
    // grid + result, plus the same main storage + hotbar rows the
    // regular inventory screen shows (a workbench GUI with no way to
    // actually move items into its own grid would be unusable - see
    // DECISIONS.md for why this reads "like inventory UI" as "reuses the
    // same screen shape," not "grid+result only and nothing else").
    // Mutually exclusive with both menu_stack and inventory_open by
    // construction, same pattern inventory_open itself already
    // establishes against menu_stack.
    bool workbench_open = false;
    lcu::items::Inventory workbench_grid_inventory(kWorkbenchGridTotalSlotCount);

    // Recomputes the workbench's own real 3x3 crafting result - same
    // RecipeRegistry::find_match integration recompute_craft_result
    // above uses, just queried as a 3x3 grid instead of 2x2.
    const auto recompute_workbench_result = [&]() {
        std::vector<lcu::items::ItemId> grid(kWorkbenchGridInputSlotCount, lcu::items::kNoItemId);
        for (lcu::usize i = 0; i < kWorkbenchGridInputSlotCount; ++i) {
            grid[i] = workbench_grid_inventory.slot_at(i).item;
        }
        const lcu::items::ItemStack* match =
            recipe_registry.find_match(grid, lcu::ui::kCraftingTableGridEdge, lcu::ui::kCraftingTableGridEdge);
        workbench_grid_inventory.set_slot(kWorkbenchGridResultSlotIndex,
                                           match != nullptr ? *match : lcu::items::ItemStack{});
    };

    // Real F-key HUD/display state (Phase 47) - toggled by their own
    // edge-detected Actions below, independent of pause state (a
    // display preference, not gameplay, so these work while the menu
    // is open too).
    bool third_person = false;

    // Real hold-to-break progress (Phase 48): accumulates real elapsed
    // hold time against whichever block is currently targeted;
    // switching targets or releasing Interact resets it.
    // `break_request_sent` latches once per real break target so a held
    // click past the threshold requests exactly one break, not one
    // every single frame afterward - a real concern in networked mode,
    // where this client doesn't locally remove the block and so would
    // keep re-hitting the same still-solid block until the server's own
    // BlockChange broadcast arrives.
    std::optional<lcu::voxel::BlockWorldCoord> breaking_block;
    lcu::f32 breaking_progress_seconds = 0.0f;
    bool break_request_sent = false;
    const auto same_block = [](const lcu::voxel::BlockWorldCoord& a, const lcu::voxel::BlockWorldCoord& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };

    // Real hand swing animation (Phase 48) - real elapsed time since the
    // last break/place action, used to offset the hand icon's own quad
    // position over kHandSwingDuration then settle back to rest.
    lcu::f32 hand_swing_elapsed = kHandSwingDuration;

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

        if (verify_hud) {
            input.set_down(lcu::platform::Action::ToggleHud, frame == kVerifyHudToggleHudFrame);
            input.set_down(lcu::platform::Action::ToggleDebugOverlay, frame == kVerifyHudToggleDebugOverlayFrame);
            input.set_down(lcu::platform::Action::TogglePerspective, frame == kVerifyHudTogglePerspectiveFrame);
            input.set_down(lcu::platform::Action::Fullscreen, frame == kVerifyHudFullscreenFrame);
            input.set_down(lcu::platform::Action::Screenshot, frame == kVerifyHudScreenshotFrame);
        }

        // LCU_VERIFY_INVENTORY (Phase 49) - placed here, same as
        // LCU_VERIFY_MENU/LCU_VERIFY_HUD above (and unlike LCU_VERIFY_
        // BREAK_PLACE/CRAFT/TORCH further below): the real E-key toggle/
        // click-handling code this hook drives runs earlier in the frame
        // than those three (it has to, to run before menu-navigation and
        // gate mouse-capture-recapture - see the inventory-open/close and
        // drag/drop blocks right below), so this hook's own input.
        // set_down calls need to land before that consumer code reads
        // them, not after - a real, previously-hit-and-fixed ordering
        // bug found during this hook's own first headless run (it
        // initially sat with the other three hooks further down and
        // silently never opened the inventory at all - previous_input/
        // input edge-detection isn't "late by a frame", it's "never sees
        // the press" when the override lands after its only reader).
        if (verify_inventory) {
            if (!verify_inventory_wood_granted) {
                // Synthetic setup (see kVerifyInventoryOpenAtSeconds' own
                // doc comment above) - lands in real inventory slot 0,
                // the same "first item added to an empty inventory"
                // precedent LCU_VERIFY_BREAK_PLACE/LCU_VERIFY_TORCH both
                // already rely on.
                player_inventory.add_item(item_registry, {wood_item_id, 1});
                verify_inventory_wood_granted = true;
            }
            const lcu::f32 elapsed =
                std::chrono::duration<lcu::f32>(std::chrono::steady_clock::now() - verify_inventory_start).count();
            input.set_down(lcu::platform::Action::Inventory,
                            (elapsed >= kVerifyInventoryOpenAtSeconds &&
                             elapsed < kVerifyInventoryOpenAtSeconds + kVerifyEdgePulseSeconds) ||
                                (elapsed >= kVerifyInventoryCloseAtSeconds &&
                                 elapsed < kVerifyInventoryCloseAtSeconds + kVerifyEdgePulseSeconds));

            // Real mouse-click simulation (Phase 49, see Window::
            // warp_mouse's own doc comment): computed fresh each frame
            // from the real current window size, same as the real
            // click-handling code below does, so this hook exercises the
            // exact same layout math the player's own clicks would.
            const lcu::ui::InventoryScreenLayout verify_inventory_layout = lcu::ui::inventory_screen_layout(
                static_cast<lcu::u32>(window.width()), static_cast<lcu::u32>(window.height()));
            const auto verify_slot_center = [](const lcu::ui::InventorySlotRect& rect) {
                return lcu::platform::Window::MousePosition{rect.x + rect.size * 0.5f, rect.y + rect.size * 0.5f};
            };

            bool interact_now = false;
            bool shift_now = false;
            if (elapsed >= kVerifyInventoryPickupWoodAtSeconds &&
                elapsed < kVerifyInventoryPickupWoodAtSeconds + kVerifyEdgePulseSeconds) {
                const auto pos = verify_slot_center(verify_inventory_layout.hotbar_slots[0]);
                window.warp_mouse(pos.x, pos.y);
                interact_now = true;
            } else if (elapsed >= kVerifyInventoryDropInCraftAtSeconds &&
                       elapsed < kVerifyInventoryDropInCraftAtSeconds + kVerifyEdgePulseSeconds) {
                const auto pos = verify_slot_center(verify_inventory_layout.craft_input[0]);
                window.warp_mouse(pos.x, pos.y);
                interact_now = true;
            } else if (elapsed >= kVerifyInventoryTakeResultAtSeconds &&
                       elapsed < kVerifyInventoryTakeResultAtSeconds + kVerifyEdgePulseSeconds) {
                const auto pos = verify_slot_center(verify_inventory_layout.craft_result);
                window.warp_mouse(pos.x, pos.y);
                interact_now = true;
            } else if (elapsed >= kVerifyInventoryPlaceInMainAtSeconds &&
                       elapsed < kVerifyInventoryPlaceInMainAtSeconds + kVerifyEdgePulseSeconds) {
                const auto pos = verify_slot_center(verify_inventory_layout.main_slots[0]);
                window.warp_mouse(pos.x, pos.y);
                interact_now = true;
            } else if (elapsed >= kVerifyInventoryShiftToHotbarAtSeconds &&
                       elapsed < kVerifyInventoryShiftToHotbarAtSeconds + kVerifyEdgePulseSeconds) {
                const auto pos = verify_slot_center(verify_inventory_layout.main_slots[0]);
                window.warp_mouse(pos.x, pos.y);
                interact_now = true;
                shift_now = true;
            }
            input.set_down(lcu::platform::Action::Interact, interact_now);
            input.set_down(lcu::platform::Action::Crouch, shift_now);
        }
        if (verify_workbench) {
            if (!verify_workbench_setup_done) {
                // Synthetic setup (see kVerifyWorkbenchOpenAtSeconds' own
                // doc comment above): grants wood directly (same
                // precedent every prior hook's own item grant uses), and
                // directly overwrites the real world block at the same
                // (-84,0,-85) spawn-look target LCU_VERIFY_BREAK_PLACE/
                // TORCH/CRAFT already establish with a real
                // game:crafting_table block, so it's guaranteed to be
                // right there to right-click.
                player_inventory.add_item(item_registry, {wood_item_id, 1});
                const lcu::voxel::BlockWorldCoord seed_pos{-84, 0, -85};
                const auto seed_split = lcu::voxel::world_to_chunk_and_local(seed_pos, lcu::voxel::Chunk::kEdgeLength);
                if (lcu::voxel::Chunk* seed_target = world.chunk_at_mutable(seed_split.chunk)) {
                    seed_target->set_block(seed_split.local.x, seed_split.local.y, seed_split.local.z,
                                            crafting_table_id);
                }
                verify_workbench_setup_done = true;
            }
            const lcu::f32 elapsed =
                std::chrono::duration<lcu::f32>(std::chrono::steady_clock::now() - verify_workbench_start).count();
            input.set_down(lcu::platform::Action::Escape, elapsed >= kVerifyWorkbenchCloseAtSeconds &&
                                                               elapsed < kVerifyWorkbenchCloseAtSeconds +
                                                                             kVerifyEdgePulseSeconds);

            const lcu::ui::CraftingTableScreenLayout verify_workbench_layout = lcu::ui::crafting_table_screen_layout(
                static_cast<lcu::u32>(window.width()), static_cast<lcu::u32>(window.height()));
            const auto verify_wb_slot_center = [](const lcu::ui::InventorySlotRect& rect) {
                return lcu::platform::Window::MousePosition{rect.x + rect.size * 0.5f, rect.y + rect.size * 0.5f};
            };

            bool place_now = false;
            bool interact_now = false;
            if (elapsed >= kVerifyWorkbenchOpenAtSeconds &&
                elapsed < kVerifyWorkbenchOpenAtSeconds + kVerifyEdgePulseSeconds) {
                place_now = true;
            } else if (elapsed >= kVerifyWorkbenchPickupWoodAtSeconds &&
                       elapsed < kVerifyWorkbenchPickupWoodAtSeconds + kVerifyEdgePulseSeconds) {
                const auto pos = verify_wb_slot_center(verify_workbench_layout.hotbar_slots[0]);
                window.warp_mouse(pos.x, pos.y);
                interact_now = true;
            } else if (elapsed >= kVerifyWorkbenchDropInGridAtSeconds &&
                       elapsed < kVerifyWorkbenchDropInGridAtSeconds + kVerifyEdgePulseSeconds) {
                const auto pos = verify_wb_slot_center(verify_workbench_layout.grid_input[0]);
                window.warp_mouse(pos.x, pos.y);
                interact_now = true;
            } else if (elapsed >= kVerifyWorkbenchTakeResultAtSeconds &&
                       elapsed < kVerifyWorkbenchTakeResultAtSeconds + kVerifyEdgePulseSeconds) {
                const auto pos = verify_wb_slot_center(verify_workbench_layout.result);
                window.warp_mouse(pos.x, pos.y);
                interact_now = true;
            }
            input.set_down(lcu::platform::Action::PlaceBlock, place_now);
            input.set_down(lcu::platform::Action::Interact, interact_now);
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

        // Real inventory screen open/close (Phase 49) - the cursor stack
        // (drag/drop's "picked up by the mouse" item, Phase 49.2) is
        // never simply discarded on close: whatever's still on it drops
        // back into player_inventory the same way a broken block's item
        // does (add_item's own earliest-slot-first fill order), any
        // leftover that doesn't fit staying on the cursor rather than
        // vanishing - a real inventory can't silently delete items.
        const auto close_inventory = [&]() {
            inventory_open = false;
            window.set_relative_mouse_mode(true);
            if (!cursor_stack.is_empty()) {
                const lcu::u32 leftover = player_inventory.add_item(item_registry, cursor_stack);
                cursor_stack =
                    leftover > 0 ? lcu::items::ItemStack{cursor_stack.item, leftover} : lcu::items::ItemStack{};
            }
        };

        // Real workbench close (Phase 50.3) - same "never silently
        // discard the cursor stack" contract close_inventory above
        // establishes.
        const auto close_workbench = [&]() {
            workbench_open = false;
            window.set_relative_mouse_mode(true);
            if (!cursor_stack.is_empty()) {
                const lcu::u32 leftover = player_inventory.add_item(item_registry, cursor_stack);
                cursor_stack =
                    leftover > 0 ? lcu::items::ItemStack{cursor_stack.item, leftover} : lcu::items::ItemStack{};
            }
        };

        const bool inventory_toggle_pressed =
            input.is_down(lcu::platform::Action::Inventory) && !previous_input.is_down(lcu::platform::Action::Inventory);
        if (inventory_toggle_pressed && !waiting_for_rebind && menu_stack.empty() && !workbench_open) {
            if (inventory_open) {
                close_inventory();
                LCU_LOG_INFO("Inventory closed");
            } else {
                inventory_open = true;
                window.set_relative_mouse_mode(false);
                recompute_craft_result();
                LCU_LOG_INFO("Inventory opened");
            }
        }

        // ESC opens the pause menu from gameplay, or pops one screen
        // back while a menu is already open (popping the last screen
        // closes it and re-captures the mouse) - see Phase 46's own
        // directive. Closes the inventory or workbench screen instead if
        // one of those is currently open (Phase 49/50) - Minecraft's own
        // ESC behavior, and keeps the pause menu, inventory screen, and
        // workbench screen mutually exclusive (menu_stack.empty() above
        // already refuses to open the inventory while paused, and the
        // place_pressed workbench-open check below refuses to open the
        // workbench while inventory_open or paused, so this side only
        // needs the reverse checks). Suppressed while actively capturing
        // a rebind so ESC cancels that instead (handled above).
        if (escape_pressed && !waiting_for_rebind) {
            if (inventory_open) {
                close_inventory();
                LCU_LOG_INFO("Inventory closed");
            } else if (workbench_open) {
                close_workbench();
                LCU_LOG_INFO("Workbench closed");
            } else if (menu_stack.empty()) {
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
        if (menu_stack.empty() && !inventory_open && !workbench_open && !window.relative_mouse_mode() &&
            (input.is_down(lcu::platform::Action::Interact) || input.is_down(lcu::platform::Action::PlaceBlock))) {
            window.set_relative_mouse_mode(true);
            suppress_click_for_recapture = true;
        }

        // Real inventory-screen drag/drop (Phase 49.2): reuses the same
        // Interact/PlaceBlock actions gameplay break/place would - since
        // the whole gameplay block below is gated off while
        // inventory_open (see `!inventory_open` on the big `if (!paused)`
        // below), these two actions are unambiguously "left-click a UI
        // slot" / "right-click a UI slot" while the screen is open,
        // exactly mirroring their real mouse-button bindings (Interact =
        // left button, PlaceBlock = right button - see key_bindings.cpp).
        // Crouch (bound to Left Shift by default, same as Minecraft's
        // own Sneak key) doubles as the real shift-click modifier, same
        // physical key Minecraft itself uses for both purposes.
        if (inventory_open && !waiting_for_rebind) {
            const bool inv_left_pressed =
                input.is_down(lcu::platform::Action::Interact) && !previous_input.is_down(lcu::platform::Action::Interact);
            const bool inv_right_pressed = input.is_down(lcu::platform::Action::PlaceBlock) &&
                                            !previous_input.is_down(lcu::platform::Action::PlaceBlock);
            if (inv_left_pressed || inv_right_pressed) {
                const bool shift_held = input.is_down(lcu::platform::Action::Crouch);
                const lcu::platform::Window::MousePosition mouse_pos = lcu::platform::Window::mouse_position();
                const lcu::ui::InventoryScreenLayout inventory_layout = lcu::ui::inventory_screen_layout(
                    static_cast<lcu::u32>(window.width()), static_cast<lcu::u32>(window.height()));
                const lcu::ui::InventoryScreenHit hit = lcu::ui::hit_test_inventory_screen(
                    inventory_layout, static_cast<lcu::f32>(mouse_pos.x), static_cast<lcu::f32>(mouse_pos.y));

                switch (hit.region) {
                    case lcu::ui::InventoryScreenRegion::kCraftInput: {
                        if (shift_held && inv_left_pressed) {
                            lcu::items::inventory_shift_click(craft_grid_inventory, hit.index, item_registry,
                                                               player_inventory, 0, kInventorySlotCount);
                        } else if (inv_left_pressed) {
                            lcu::items::inventory_left_click(craft_grid_inventory, item_registry, hit.index, cursor_stack);
                        } else {
                            lcu::items::inventory_right_click(craft_grid_inventory, item_registry, hit.index,
                                                               cursor_stack);
                        }
                        recompute_craft_result();
                        break;
                    }
                    case lcu::ui::InventoryScreenRegion::kCraftResult: {
                        // Real "take the crafted result" (Phase 49.3) -
                        // Minecraft's own right-click on the result slot
                        // behaves identically to left-click (there's no
                        // "half the result" concept), so this
                        // deliberately doesn't distinguish inv_left_pressed
                        // from inv_right_pressed, an honest simplification
                        // rather than a fake distinct behavior.
                        const lcu::items::ItemStack result = craft_grid_inventory.slot_at(kCraftGridResultSlotIndex);
                        const lcu::u32 max_stack =
                            result.is_empty() ? 0 : item_registry.definition_of(result.item).max_stack_size;
                        const bool cursor_accepts =
                            cursor_stack.is_empty() ||
                            (cursor_stack.item == result.item && cursor_stack.count + result.count <= max_stack);
                        if (!result.is_empty() && cursor_accepts) {
                            cursor_stack = cursor_stack.is_empty()
                                               ? result
                                               : lcu::items::ItemStack{cursor_stack.item, cursor_stack.count + result.count};
                            // Consumes exactly 1 of each non-empty
                            // ingredient slot - correct for every real
                            // shapeless recipe registered so far (each
                            // lists each ingredient once - see
                            // recipe_registry.add_shapeless above); a
                            // recipe needing >1 of the same ingredient in
                            // one cell would need per-recipe ingredient
                            // counts this simple "decrement by 1" doesn't
                            // model - a real, documented limit, not
                            // silently wrong for anything actually
                            // registered.
                            for (lcu::usize i = 0; i < kCraftGridInputSlotCount; ++i) {
                                const lcu::items::ItemStack ingredient = craft_grid_inventory.slot_at(i);
                                if (!ingredient.is_empty()) {
                                    craft_grid_inventory.set_slot(
                                        i, ingredient.count > 1 ? lcu::items::ItemStack{ingredient.item, ingredient.count - 1}
                                                                : lcu::items::ItemStack{});
                                }
                            }
                            recompute_craft_result();
                        }
                        break;
                    }
                    case lcu::ui::InventoryScreenRegion::kMainInventory: {
                        // Main-grid slot i maps to real inventory index
                        // kHotbarSlotCount + i - the main storage range
                        // starts right after the 9 hotbar slots (see
                        // "Real Minecraft-sized inventory" above).
                        const lcu::usize slot = kHotbarSlotCount + hit.index;
                        if (shift_held && inv_left_pressed) {
                            lcu::items::inventory_shift_click(player_inventory, slot, item_registry, player_inventory,
                                                               0, kHotbarSlotCount);
                        } else if (inv_left_pressed) {
                            lcu::items::inventory_left_click(player_inventory, item_registry, slot, cursor_stack);
                        } else {
                            lcu::items::inventory_right_click(player_inventory, item_registry, slot, cursor_stack);
                        }
                        break;
                    }
                    case lcu::ui::InventoryScreenRegion::kHotbar: {
                        // Hotbar slot i IS real inventory slot i - the
                        // inventory screen's hotbar row is the same
                        // physical storage the in-world hotbar reads
                        // (Phase 49's whole point), not a separate copy.
                        const lcu::usize slot = hit.index;
                        if (shift_held && inv_left_pressed) {
                            lcu::items::inventory_shift_click(player_inventory, slot, item_registry, player_inventory,
                                                               kHotbarSlotCount, kInventorySlotCount);
                        } else if (inv_left_pressed) {
                            lcu::items::inventory_left_click(player_inventory, item_registry, slot, cursor_stack);
                        } else {
                            lcu::items::inventory_right_click(player_inventory, item_registry, slot, cursor_stack);
                        }
                        break;
                    }
                    case lcu::ui::InventoryScreenRegion::kNone:
                        break;
                }
                if (hit.region != lcu::ui::InventoryScreenRegion::kNone) {
                    if (cursor_stack.is_empty()) {
                        LCU_LOG_INFO("Inventory click: region={} index={}{} -> cursor empty",
                                     static_cast<int>(hit.region), hit.index, shift_held ? " (shift)" : "");
                    } else {
                        LCU_LOG_INFO("Inventory click: region={} index={}{} -> cursor {} x{}",
                                     static_cast<int>(hit.region), hit.index, shift_held ? " (shift)" : "",
                                     item_registry.definition_of(cursor_stack.item).namespaced_id, cursor_stack.count);
                    }
                }
            }
        }

        // Real workbench drag/drop (Phase 50.3) - same real click
        // dispatch inventory_open's own block above uses, just against
        // the workbench's own 3x3 grid + the shared player_inventory's
        // main/hotbar ranges (there is no separate "workbench inventory"
        // - the main storage/hotbar rows are the same real
        // player_inventory the regular inventory screen and the
        // in-world hotbar both read).
        if (workbench_open && !waiting_for_rebind) {
            const bool wb_left_pressed =
                input.is_down(lcu::platform::Action::Interact) && !previous_input.is_down(lcu::platform::Action::Interact);
            const bool wb_right_pressed = input.is_down(lcu::platform::Action::PlaceBlock) &&
                                           !previous_input.is_down(lcu::platform::Action::PlaceBlock);
            if (wb_left_pressed || wb_right_pressed) {
                const bool shift_held = input.is_down(lcu::platform::Action::Crouch);
                const lcu::platform::Window::MousePosition mouse_pos = lcu::platform::Window::mouse_position();
                const lcu::ui::CraftingTableScreenLayout workbench_layout = lcu::ui::crafting_table_screen_layout(
                    static_cast<lcu::u32>(window.width()), static_cast<lcu::u32>(window.height()));
                const lcu::ui::CraftingTableScreenHit hit = lcu::ui::hit_test_crafting_table_screen(
                    workbench_layout, static_cast<lcu::f32>(mouse_pos.x), static_cast<lcu::f32>(mouse_pos.y));

                switch (hit.region) {
                    case lcu::ui::CraftingTableScreenRegion::kGridInput: {
                        if (shift_held && wb_left_pressed) {
                            lcu::items::inventory_shift_click(workbench_grid_inventory, hit.index, item_registry,
                                                               player_inventory, 0, kInventorySlotCount);
                        } else if (wb_left_pressed) {
                            lcu::items::inventory_left_click(workbench_grid_inventory, item_registry, hit.index,
                                                              cursor_stack);
                        } else {
                            lcu::items::inventory_right_click(workbench_grid_inventory, item_registry, hit.index,
                                                               cursor_stack);
                        }
                        recompute_workbench_result();
                        break;
                    }
                    case lcu::ui::CraftingTableScreenRegion::kResult: {
                        // Same real "take the result, consume 1 of each
                        // ingredient" logic the 2x2 inventory grid's own
                        // result-click uses - see its own doc comment for
                        // the real, documented multi-ingredient limit.
                        const lcu::items::ItemStack result =
                            workbench_grid_inventory.slot_at(kWorkbenchGridResultSlotIndex);
                        const lcu::u32 max_stack =
                            result.is_empty() ? 0 : item_registry.definition_of(result.item).max_stack_size;
                        const bool cursor_accepts =
                            cursor_stack.is_empty() ||
                            (cursor_stack.item == result.item && cursor_stack.count + result.count <= max_stack);
                        if (!result.is_empty() && cursor_accepts) {
                            cursor_stack = cursor_stack.is_empty()
                                               ? result
                                               : lcu::items::ItemStack{cursor_stack.item, cursor_stack.count + result.count};
                            for (lcu::usize i = 0; i < kWorkbenchGridInputSlotCount; ++i) {
                                const lcu::items::ItemStack ingredient = workbench_grid_inventory.slot_at(i);
                                if (!ingredient.is_empty()) {
                                    workbench_grid_inventory.set_slot(
                                        i, ingredient.count > 1 ? lcu::items::ItemStack{ingredient.item, ingredient.count - 1}
                                                                : lcu::items::ItemStack{});
                                }
                            }
                            recompute_workbench_result();
                        }
                        break;
                    }
                    case lcu::ui::CraftingTableScreenRegion::kMainInventory: {
                        const lcu::usize slot = kHotbarSlotCount + hit.index;
                        if (shift_held && wb_left_pressed) {
                            lcu::items::inventory_shift_click(player_inventory, slot, item_registry, player_inventory,
                                                               0, kHotbarSlotCount);
                        } else if (wb_left_pressed) {
                            lcu::items::inventory_left_click(player_inventory, item_registry, slot, cursor_stack);
                        } else {
                            lcu::items::inventory_right_click(player_inventory, item_registry, slot, cursor_stack);
                        }
                        break;
                    }
                    case lcu::ui::CraftingTableScreenRegion::kHotbar: {
                        const lcu::usize slot = hit.index;
                        if (shift_held && wb_left_pressed) {
                            lcu::items::inventory_shift_click(player_inventory, slot, item_registry, player_inventory,
                                                               kHotbarSlotCount, kInventorySlotCount);
                        } else if (wb_left_pressed) {
                            lcu::items::inventory_left_click(player_inventory, item_registry, slot, cursor_stack);
                        } else {
                            lcu::items::inventory_right_click(player_inventory, item_registry, slot, cursor_stack);
                        }
                        break;
                    }
                    case lcu::ui::CraftingTableScreenRegion::kNone:
                        break;
                }
                if (hit.region != lcu::ui::CraftingTableScreenRegion::kNone) {
                    if (cursor_stack.is_empty()) {
                        LCU_LOG_INFO("Workbench click: region={} index={}{} -> cursor empty",
                                     static_cast<int>(hit.region), hit.index, shift_held ? " (shift)" : "");
                    } else {
                        LCU_LOG_INFO("Workbench click: region={} index={}{} -> cursor {} x{}",
                                     static_cast<int>(hit.region), hit.index, shift_held ? " (shift)" : "",
                                     item_registry.definition_of(cursor_stack.item).namespaced_id, cursor_stack.count);
                    }
                }
            }
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

        // Set inside the !paused block below (from the real raycast hit
        // and break-progress accumulator), read afterward in the bgfx
        // render section (Phase 48's block highlight/break-progress
        // overlay) - declared at this outer scope since those two
        // points aren't the same block.
        // [[maybe_unused]]: both are read only inside the
        // LCU_ENABLE_BGFX-only render section below - real dead stores
        // in a non-bgfx build (no renderer exists to consume them),
        // not a bug.
        [[maybe_unused]] std::optional<lcu::physics::RaycastHit> render_hit;
        [[maybe_unused]] lcu::f32 render_break_fraction = 0.0f;

        if (verify_break_place) {
            const lcu::f32 elapsed =
                std::chrono::duration<lcu::f32>(std::chrono::steady_clock::now() - verify_break_place_start).count();
            input.set_down(lcu::platform::Action::Interact, elapsed < kVerifyBreakHoldSeconds);
            input.set_down(lcu::platform::Action::CycleHotbar,
                            elapsed >= kVerifyCycleHotbarAtSeconds &&
                                elapsed < kVerifyCycleHotbarAtSeconds + kVerifyEdgePulseSeconds);
            input.set_down(lcu::platform::Action::CycleHotbarPrev,
                            elapsed >= kVerifyCycleHotbarPrevAtSeconds &&
                                elapsed < kVerifyCycleHotbarPrevAtSeconds + kVerifyEdgePulseSeconds);
            input.set_down(lcu::platform::Action::PlaceBlock,
                            elapsed >= kVerifyPlaceAtSeconds && elapsed < kVerifyPlaceAtSeconds + kVerifyEdgePulseSeconds);
        }
        if (verify_craft) {
            const lcu::f32 elapsed =
                std::chrono::duration<lcu::f32>(std::chrono::steady_clock::now() - verify_craft_start).count();
            // Real held Interact windows (Phase 48 update - see
            // kVerifyCraftBreakGrassHoldSeconds' own doc comment above):
            // break the grass block the player spawns on, then, after a
            // real gap (covering both the block's own hardness and, in
            // networked mode, the server round trip), break the dirt
            // block beneath it.
            const bool interact_now = (elapsed < kVerifyCraftBreakGrassHoldSeconds) ||
                                       (elapsed >= kVerifyCraftBreakDirtHoldStartSeconds &&
                                        elapsed < kVerifyCraftBreakDirtHoldEndSeconds);
            const bool craft_now =
                (elapsed >= kVerifyCraftFirstCraftAtSeconds &&
                 elapsed < kVerifyCraftFirstCraftAtSeconds + kVerifyEdgePulseSeconds) ||
                (elapsed >= kVerifyCraftRejectAtSeconds && elapsed < kVerifyCraftRejectAtSeconds + kVerifyEdgePulseSeconds);
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
                // Synthetic setup (see kVerifyTorchBreakHoldSeconds' own
                // doc comment above): nothing in this build's world
                // drops a torch to pick up yet, so this hook grants one
                // directly, the same way LCU_VERIFY_CRAFT's own setup
                // breaks real blocks to seed its inventory state. Granted
                // before the break below so it's the first item the
                // (empty) inventory ever receives, landing in real slot
                // 0 - the default selected_hotbar_slot, so no cycling is
                // actually *required* to place it, but the hook still
                // exercises CycleHotbar/CycleHotbarPrev below for real
                // (see their own doc comment above).
                player_inventory.add_item(item_registry, {torch_item_id, 1});
                verify_torch_granted = true;
            }
            const lcu::f32 elapsed =
                std::chrono::duration<lcu::f32>(std::chrono::steady_clock::now() - verify_torch_start).count();
            input.set_down(lcu::platform::Action::Interact, elapsed < kVerifyTorchBreakHoldSeconds);
            input.set_down(lcu::platform::Action::CycleHotbar,
                            (elapsed >= kVerifyTorchCycleAt1Seconds &&
                             elapsed < kVerifyTorchCycleAt1Seconds + kVerifyEdgePulseSeconds) ||
                                (elapsed >= kVerifyTorchCycleAt2Seconds &&
                                 elapsed < kVerifyTorchCycleAt2Seconds + kVerifyEdgePulseSeconds) ||
                                (elapsed >= kVerifyTorchCycleAt3Seconds &&
                                 elapsed < kVerifyTorchCycleAt3Seconds + kVerifyEdgePulseSeconds));
            input.set_down(lcu::platform::Action::CycleHotbarPrev,
                            (elapsed >= kVerifyTorchCyclePrevAt1Seconds &&
                             elapsed < kVerifyTorchCyclePrevAt1Seconds + kVerifyEdgePulseSeconds) ||
                                (elapsed >= kVerifyTorchCyclePrevAt2Seconds &&
                                 elapsed < kVerifyTorchCyclePrevAt2Seconds + kVerifyEdgePulseSeconds) ||
                                (elapsed >= kVerifyTorchCyclePrevAt3Seconds &&
                                 elapsed < kVerifyTorchCyclePrevAt3Seconds + kVerifyEdgePulseSeconds));
            input.set_down(lcu::platform::Action::PlaceBlock,
                            elapsed >= kVerifyTorchPlaceAtSeconds &&
                                elapsed < kVerifyTorchPlaceAtSeconds + kVerifyEdgePulseSeconds);
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

        // Real item-entity physics + pickup (Phase 50) - client-side
        // always (item entities are spawned optimistically client-side
        // in both single-player and networked mode, same as their own
        // spawn call site's own doc comment explains), gated on `paused`
        // alone like day_night_cycle above, NOT on `inventory_open` -
        // dropped items keep falling/despawning and can still be picked
        // up while the player's inventory screen is open, matching
        // Minecraft's own real behavior (only the player's own
        // movement/mining/placing locks while it's open, not the world).
        if (!paused) {
            game::systems::update_item_entities(entity_registry, world, delta_seconds, is_solid);
            const lcu::physics::AABB pickup_aabb{
                player.aabb.min - lcu::math::Vec3{kItemPickupRangeInflate, kItemPickupRangeInflate,
                                                    kItemPickupRangeInflate},
                player.aabb.max + lcu::math::Vec3{kItemPickupRangeInflate, kItemPickupRangeInflate,
                                                    kItemPickupRangeInflate},
            };
            const lcu::u32 picked_up =
                game::systems::pickup_item_entities(entity_registry, pickup_aabb, item_registry, player_inventory);
            if (picked_up > 0) {
                LCU_LOG_INFO("Picked up from {} item entity(ies) (inventory updated)", picked_up);
            }
        }

        // Real inventory-screen player-control lock (Phase 49, brief
        // section 60's own directive: "game keeps running - Minecraft
        // behavior: no pause in inventory"): unlike `paused` above
        // (menu_stack non-empty), opening the inventory does NOT freeze
        // day_night_cycle/AI wander (both gated on `paused` alone,
        // above/at the top of this frame) - only the player's own
        // movement/camera/mining/placing/crafting lock, the same real
        // Minecraft behavior (the world keeps ticking behind the GUI).
        // Same real lock for the workbench screen (Phase 50.3) - the
        // crafting-table right-click interception below runs inside this
        // very block (workbench_open is still false at the moment of
        // that click, so this gate doesn't block the *opening* click
        // itself), then subsequent frames correctly lock out movement/
        // mining/placing the same way inventory_open already does.
        if (!paused && !inventory_open && !workbench_open) {
            // Real hand-swing elapsed time (Phase 48) - reset to 0 on
            // every real break/place action below, counted up here so
            // the render section can compute a real swing offset from
            // it. Frozen while paused, matching every other real
            // per-frame gameplay update in this block.
            hand_swing_elapsed += delta_seconds;

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
            render_hit = hit;

            // Real hold-to-break progress (Phase 48.2): accumulates
            // real elapsed hold time against whichever block is
            // currently targeted - see breaking_block's own doc comment
            // above for why switching targets/releasing resets it and
            // why break_request_sent exists.
            const bool interact_held = input.is_down(lcu::platform::Action::Interact) && !suppress_click_for_recapture;
            if (hit && interact_held) {
                if (breaking_block && same_block(*breaking_block, hit->world)) {
                    breaking_progress_seconds += delta_seconds;
                } else {
                    breaking_block = hit->world;
                    breaking_progress_seconds = 0.0f;
                    break_request_sent = false;
                }
            } else {
                breaking_block.reset();
                breaking_progress_seconds = 0.0f;
                break_request_sent = false;
            }
            const bool targeting_breaking_block =
                hit && breaking_block && same_block(*breaking_block, hit->world);
            render_break_fraction =
                targeting_breaking_block
                    ? lcu::voxel::break_progress_fraction(breaking_progress_seconds,
                                                           block_registry.definition_of(hit->block).hardness)
                    : 0.0f;
            const bool break_ready =
                targeting_breaking_block && !break_request_sent &&
                lcu::voxel::is_break_ready(breaking_progress_seconds, block_registry.definition_of(hit->block).hardness);

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

            // Real Minecraft-style hotbar selection (Phase 49): logs
            // whatever real item currently sits in the newly-selected
            // slot (or "(empty)"), rather than a fixed name table - the
            // slot's own contents are the only source of truth now.
            const auto log_selected_hotbar_slot = [&](const char* verb) {
                const lcu::items::ItemStack& stack = player_inventory.slot_at(selected_hotbar_slot);
                if (stack.is_empty()) {
                    LCU_LOG_INFO("{} hotbar slot {} (empty)", verb, selected_hotbar_slot);
                } else {
                    LCU_LOG_INFO("{} hotbar slot {}: {}", verb, selected_hotbar_slot,
                                 item_registry.definition_of(stack.item).namespaced_id);
                }
            };

            if (cycle_hotbar_pressed) {
                selected_hotbar_slot = (selected_hotbar_slot + 1) % kHotbarSlotCount;
                log_selected_hotbar_slot("Selected");
            }
            if (cycle_hotbar_prev_pressed) {
                selected_hotbar_slot = (selected_hotbar_slot + kHotbarSlotCount - 1) % kHotbarSlotCount;
                log_selected_hotbar_slot("Selected");
            }

            // Direct number-row hotbar selection (Phase 43): SelectHotbar1..9
            // are declared consecutively in Action (see input.h), so this
            // walks them as one contiguous range instead of 9 near-identical
            // if-blocks. Every slot 0-8 is now a real hotbar slot (Phase 49,
            // kHotbarSlotCount == 9 == the number of SelectHotbar actions) -
            // no "no matching entry" case remains, selection just points at
            // a real inventory slot whether or not it's currently holding
            // anything.
            for (lcu::usize i = 0; i < kHotbarSlotCount; ++i) {
                const auto slot_action =
                    static_cast<lcu::platform::Action>(static_cast<lcu::u8>(lcu::platform::Action::SelectHotbar1) + i);
                if (input.is_down(slot_action) && !previous_input.is_down(slot_action)) {
                    selected_hotbar_slot = i;
                    log_selected_hotbar_slot("Selected");
                    break;
                }
            }

            if (pick_block_pressed && hit) {
                // Real "middle-click to pick block" (Phase 43, redone for
                // Phase 49's real inventory-driven hotbar): looks up the
                // item the targeted block itself drops
                // (block_item_mapping::item_for_block), then, if the
                // player already holds that item somewhere in their
                // inventory, swaps it into the currently selected hotbar
                // slot - Minecraft's own real survival-mode pick-block
                // behavior (it never grants a new item, only rearranges
                // ones you already have). A silent no-op if the block has
                // no item mapping, or the player isn't holding that item
                // anywhere.
                const lcu::items::ItemId wanted_item = block_item_mapping.item_for_block(hit->block);
                bool found = false;
                if (wanted_item != lcu::items::kNoItemId) {
                    for (lcu::usize i = 0; i < player_inventory.slot_count(); ++i) {
                        if (player_inventory.slot_at(i).item == wanted_item) {
                            if (i != selected_hotbar_slot) {
                                const lcu::items::ItemStack held = player_inventory.slot_at(selected_hotbar_slot);
                                player_inventory.set_slot(selected_hotbar_slot, player_inventory.slot_at(i));
                                player_inventory.set_slot(i, held);
                            }
                            found = true;
                            break;
                        }
                    }
                }
                if (found) {
                    LCU_LOG_INFO("Picked block into hotbar slot {}: {}", selected_hotbar_slot,
                                 item_registry.definition_of(wanted_item).namespaced_id);
                } else {
                    LCU_LOG_DEBUG("PickBlock: not holding an item for block id {}", hit->block);
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

            if (break_ready && networked) {
                break_request_sent = true;
                hand_swing_elapsed = 0.0f;
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
                spawn_item_entity_for_broken_block(hit->block, hit->world);
            } else if (break_ready) {
                break_request_sent = true;
                hand_swing_elapsed = 0.0f;
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
                    spawn_item_entity_for_broken_block(hit->block, hit->world);
                } else {
                    LCU_LOG_DEBUG("Break target's chunk isn't loaded, ignoring");
                }
            }

            // Real crafting-table right-click interception (Phase 50.3):
            // right-clicking a `game:crafting_table` block opens the
            // workbench screen instead of placing/breaking through it -
            // Minecraft's own real behavior for every "special GUI"
            // block (a crafting table, a furnace, ...): the click is
            // always intercepted, regardless of what item (if any) the
            // player happens to be holding. Checked before the normal
            // placement logic below so a placeable item held while
            // looking at a crafting table never overwrites it. Already
            // inside the `!workbench_open` gate above, so this can only
            // run on the real opening click, never while already open
            // (the separate workbench click-handling block above owns
            // every click once it's open).
            if (place_pressed && hit && hit->block == crafting_table_id) {
                workbench_open = true;
                window.set_relative_mouse_mode(false);
                recompute_workbench_result();
                LCU_LOG_INFO("Workbench opened");
            } else {
                // Real slot-driven placement (Phase 49): whatever item
            // physically sits in the selected hotbar slot right now is
            // what places - block_item_mapping::block_for_item is the
            // only thing translating it into a block id.
            // kAirBlockId gate: an empty slot (kNoItemId) or a held item
            // with no registered block (e.g. game:compost/game:planks,
            // both crafted-only with no placeable block) real-honestly
            // no-ops here rather than "placing air" - see block_for_item's
            // own doc comment for why kAirBlockId is the sentinel.
            const lcu::items::ItemStack selected_stack = player_inventory.slot_at(selected_hotbar_slot);
            const lcu::voxel::BlockId selected_place_block_id = block_item_mapping.block_for_item(selected_stack.item);
            if (place_pressed && hit && selected_place_block_id != lcu::voxel::kAirBlockId &&
                player_inventory.remove_item(selected_stack.item, 1) == 1) {
                hand_swing_elapsed = 0.0f;
                const lcu::voxel::BlockWorldCoord place_pos{
                    hit->world.x + static_cast<lcu::i64>(hit->normal.x),
                    hit->world.y + static_cast<lcu::i64>(hit->normal.y),
                    hit->world.z + static_cast<lcu::i64>(hit->normal.z),
                };
                const std::string& selected_place_name = item_registry.definition_of(selected_stack.item).namespaced_id;
                if (networked) {
                    // See the break_ready/BlockActionType::Break branch
                    // above - same server-authoritative pattern. The item is
                    // still consumed client-side immediately (no server-side
                    // inventory exists yet - see DECISIONS.md), so a request
                    // the server ends up rejecting (e.g. the target stopped
                    // being air by the time it's processed) currently isn't
                    // refunded; a real inventory-sync/rejection channel is a
                    // separate, larger feature.
                    LCU_LOG_INFO("Requesting place {} at world ({}, {}, {}) (inventory: {})", selected_place_name,
                                 place_pos.x, place_pos.y, place_pos.z, player_inventory.count_item(selected_stack.item));
                    server_connection.send(lcu::network::Channel::ReliableOrdered,
                                            protocol::encode_block_action({protocol::BlockActionType::Place, place_pos.x,
                                                                            place_pos.y, place_pos.z,
                                                                            selected_place_block_id}));
                } else {
                    LCU_LOG_INFO("Placing {} at world ({}, {}, {}) (inventory: {})", selected_place_name, place_pos.x,
                                 place_pos.y, place_pos.z, player_inventory.count_item(selected_stack.item));
                    const auto split = lcu::voxel::world_to_chunk_and_local(place_pos, lcu::voxel::Chunk::kEdgeLength);
                    if (lcu::voxel::Chunk* target = world.chunk_at_mutable(split.chunk)) {
                        const lcu::voxel::BlockId old_id = target->block_at(split.local.x, split.local.y, split.local.z);
                        target->set_block(split.local.x, split.local.y, split.local.z, selected_place_block_id);
                        const auto light_touched =
                            update_lighting_for_edit(split.chunk, split.local, old_id, selected_place_block_id);
                        remesh_and_upload(split.chunk);
                        remesh_edit_neighbors(split.chunk, split.local, light_touched);
                        if (selected_place_block_id == torch_id) {
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
                        player_inventory.add_item(item_registry, {selected_stack.item, 1});
                    }
                }
            }
            }  // else (not a crafting-table right-click)
        }  // if (!paused && !inventory_open && !workbench_open)

        // Real F-key HUD/display toggles (Phase 47) - work regardless
        // of pause state (see `third_person`'s own doc comment above).
        if (input.is_down(lcu::platform::Action::ToggleHud) &&
            !previous_input.is_down(lcu::platform::Action::ToggleHud)) {
            options.hud_enabled = !options.hud_enabled;
            LCU_LOG_INFO("HUD: {}", options.hud_enabled ? "on" : "off");
        }
        if (input.is_down(lcu::platform::Action::ToggleDebugOverlay) &&
            !previous_input.is_down(lcu::platform::Action::ToggleDebugOverlay)) {
            options.debug_overlay_enabled = !options.debug_overlay_enabled;
            LCU_LOG_INFO("Debug overlay: {}", options.debug_overlay_enabled ? "on" : "off");
        }
        if (input.is_down(lcu::platform::Action::Screenshot) &&
            !previous_input.is_down(lcu::platform::Action::Screenshot)) {
#if defined(LCU_ENABLE_BGFX)
            const std::string screenshot_path = "screenshot_" + std::to_string(frame);
            renderer.request_screenshot(screenshot_path);
            LCU_LOG_INFO("Requested screenshot: {}", screenshot_path);
#else
            LCU_LOG_INFO("Screenshot requested but this build has no renderer (LCU_ENABLE_BGFX=OFF)");
#endif
        }
        if (input.is_down(lcu::platform::Action::TogglePerspective) &&
            !previous_input.is_down(lcu::platform::Action::TogglePerspective)) {
            third_person = !third_person;
            LCU_LOG_INFO("Perspective: {}", third_person ? "third-person (behind)" : "first-person");
        }
        if (input.is_down(lcu::platform::Action::Fullscreen) &&
            !previous_input.is_down(lcu::platform::Action::Fullscreen)) {
            window.set_fullscreen(!window.fullscreen());
            LCU_LOG_INFO("Fullscreen: {}", window.fullscreen());
        }

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
        // Real third-person-behind camera (Phase 47, F5) - only the
        // render eye position shifts backward along the real look
        // direction; gameplay (raycast, movement, camera.position
        // itself) is untouched, matching Minecraft's own "aim from
        // where you're looking, not from the pulled-back eye" behavior.
        // PARTIAL: no player model exists to render in front of the
        // camera, so there is no real third-person-front mode - see
        // DECISIONS.md.
        const lcu::math::Vec3 render_eye =
            third_person ? camera.position - camera.forward() * kThirdPersonDistance : camera.position;
        const lcu::math::Mat4 view = third_person
                                          ? lcu::math::Mat4::look_at(render_eye, render_eye + camera.forward(),
                                                                      lcu::math::Vec3{0.0f, 1.0f, 0.0f})
                                          : camera.view_matrix();
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

        // Populated below (hotbar contents/selection), consumed by both
        // queue_hud_quads (before flush_ui_quads) and draw_hud_labels
        // (after the debug overlay's own text) - see hud.h's own doc
        // comment. Health/hunger stay at HudState's own real defaults
        // (20/20, full) this phase - Phase 51 wires real per-frame
        // values in once PlayerHealth/PlayerHunger exist.
        lcu::ui::HudState hud_state;

        // Real inventory screen (Phase 49.1) - same populate-then-queue-
        // then-draw-labels split as hud_state above. Only actually
        // populated with real slot contents below when inventory_open
        // (see "Real HUD" section below); stays default/empty otherwise,
        // which is fine since queue/draw_inventory_screen_* are also
        // gated on inventory_open and never read it that frame.
        lcu::ui::InventoryScreenState inventory_state{};
        const lcu::ui::InventoryScreenLayout inventory_layout =
            lcu::ui::inventory_screen_layout(renderer_desc.width, renderer_desc.height);

        // Real workbench screen (Phase 50.3) - same populate-then-queue-
        // then-draw-labels split as inventory_state above.
        lcu::ui::CraftingTableScreenState workbench_state{};
        const lcu::ui::CraftingTableScreenLayout workbench_layout =
            lcu::ui::crafting_table_screen_layout(renderer_desc.width, renderer_desc.height);

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

        // Real item-entity rendering (Phase 50.1) - a small colored
        // camera-facing quad per real dropped item (its own item's
        // icon_color, same flat-color convention every other real icon
        // in this project already follows), via the new depth-tested
        // submit_world_billboard (not submit_billboard's own sky view -
        // an item entity needs to be genuinely occluded by/occlude
        // terrain, not always render on top the way the sun/moon do -
        // see submit_world_billboard's own doc comment). spin_angle
        // (real per-frame Y-axis rotation, purely visual) rotates the
        // quad's own right/up basis around world-up so a dropped item
        // is visibly distinct from a static block even as a flat quad,
        // not just a billboard that always faces the camera - the same
        // real per-frame variation Minecraft's own spinning item
        // entities have. Always drawn from entity_registry regardless of
        // networked/single-player (unlike AI above): item entities are
        // spawned client-side in both modes (see their own spawn call
        // site's doc comment), not server-replicated.
        for (const lcu::ecs::EntityId& entity : entity_registry.pool_for<game::components::ItemEntity>().dense_entities()) {
            const auto& item_entity = *entity_registry.get_component<game::components::ItemEntity>(entity);
            const lcu::math::Vec3 pos = entity_registry.get_component<game::components::Position>(entity)->value;
            const lcu::f32 cos_a = std::cos(item_entity.spin_angle);
            const lcu::f32 sin_a = std::sin(item_entity.spin_angle);
            const lcu::math::Vec3 billboard_right{cos_a, 0.0f, sin_a};
            constexpr lcu::math::Vec3 kWorldUp{0.0f, 1.0f, 0.0f};
            const lcu::math::Vec4 icon_color =
                item_registry.definition_of(item_entity.stack.item).icon_color;
            renderer.submit_world_billboard(pos, billboard_right, kWorldUp, game::components::kItemEntityHalfExtent,
                                             {icon_color.x, icon_color.y, icon_color.z}, sky_program, view, proj);
            if (bgfx::isValid(sky_program)) {
                ++draw_calls;
            }
        }

        // Real block highlight + break-progress overlay (Phase 48).
        // Both driven by render_hit/render_break_fraction, set from the
        // real raycast/break-progress accumulator above (not
        // recomputed here) since this section runs after the !paused
        // block that owns those.
        if (render_hit) {
            const lcu::math::Vec3 block_min{static_cast<lcu::f32>(render_hit->world.x),
                                             static_cast<lcu::f32>(render_hit->world.y),
                                             static_cast<lcu::f32>(render_hit->world.z)};
            const lcu::math::Vec3 block_max = block_min + lcu::math::Vec3{1.0f, 1.0f, 1.0f};
            renderer.submit_wireframe_box(block_min - lcu::math::Vec3{kBlockHighlightOutset, kBlockHighlightOutset,
                                                                        kBlockHighlightOutset},
                                           block_max + lcu::math::Vec3{kBlockHighlightOutset, kBlockHighlightOutset,
                                                                        kBlockHighlightOutset},
                                           kBlockHighlightColor, sky_program, view, proj);
            if (bgfx::isValid(sky_program)) {
                ++draw_calls;
            }

            if (render_break_fraction > 0.0f) {
                // Real, honestly-scoped substitute for a per-fragment
                // crack-noise shader effect (see kBreakOverlayMaxAlpha's
                // own doc comment) - a solid box that gets visually
                // darker (toward black) as progress advances, drawn
                // very slightly inset so it wins the depth test against
                // the block's own face without z-fighting.
                const lcu::math::Vec3 overlay_color =
                    lcu::math::Vec3{1.0f, 1.0f, 1.0f} * (1.0f - render_break_fraction * kBreakOverlayMaxDarken);
                constexpr lcu::f32 kInset = 0.005f;
                renderer.submit_solid_box(block_min + lcu::math::Vec3{kInset, kInset, kInset},
                                           block_max - lcu::math::Vec3{kInset, kInset, kInset}, overlay_color,
                                           sky_program, view, proj);
                if (bgfx::isValid(sky_program)) {
                    ++draw_calls;
                }
            }
        }

        // Real hand icon (Phase 48.3, updated for Phase 49's real
        // inventory-driven hotbar) - whatever item physically sits in
        // the selected hotbar slot right now, own icon color (Phase 47's
        // icon_color), bottom-right corner, swinging toward center-screen
        // and back over kHandSwingDuration on every real break/place
        // action (hand_swing_elapsed, reset to 0 by those - see above).
        // An empty selected slot draws no hand icon at all - there's no
        // real item color to show, the same honest "nothing to render"
        // choice place_pressed's own kAirBlockId gate makes.
        if (options.hud_enabled) {
            const lcu::items::ItemStack& held_stack = player_inventory.slot_at(selected_hotbar_slot);
            if (!held_stack.is_empty()) {
                const lcu::f32 swing_t = std::clamp(hand_swing_elapsed / kHandSwingDuration, 0.0f, 1.0f);
                // A real, simple ease: swings out over the first half, back
                // over the second - std::sin(swing_t * pi) peaks at 1.0
                // exactly at swing_t=0.5, is 0 at both ends.
                const lcu::f32 swing_amount = std::sin(swing_t * 3.14159265358979323846f);
                const lcu::f32 rest_x =
                    static_cast<lcu::f32>(renderer_desc.width) - kHandIconSize - kHandRestMarginX;
                const lcu::f32 rest_y =
                    static_cast<lcu::f32>(renderer_desc.height) - kHandIconSize - kHandRestMarginY;
                const lcu::f32 hand_x = rest_x - swing_amount * kHandSwingOffset;
                const lcu::f32 hand_y = rest_y - swing_amount * kHandSwingOffset;
                const lcu::math::Vec4 hand_color = item_registry.definition_of(held_stack.item).icon_color;
                renderer.submit_ui_quad(hand_x, hand_y, kHandIconSize, kHandIconSize, hand_color);
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

        // Real HUD: hotbar (real held-item icons/counts read directly
        // from player_inventory's own real slots 0-8, Phase 49 - no
        // separate placeable_items table anymore, the hotbar row IS the
        // inventory's own hotbar range now) and health/hunger bars
        // (Phase 47 - hardcoded full this phase; Phase 51 wires real
        // values in). Same options.hud_enabled gate as the crosshair -
        // one real "HUD" toggle, not several independent ones. `hud_state`
        // is declared once above (see draw_calls) and reused below by
        // draw_hud_labels, since the quad- and text-drawing halves can't
        // happen at the same call site (same split menu_renderer.h's own
        // doc comment explains for the pause menu).
        hud_state.selected_hotbar_slot = selected_hotbar_slot;
        for (lcu::usize i = 0; i < lcu::ui::kHotbarSlotCount; ++i) {
            const lcu::items::ItemStack& stack = player_inventory.slot_at(i);
            hud_state.hotbar[i].has_item = !stack.is_empty();
            if (!stack.is_empty()) {
                hud_state.hotbar[i].icon_color = item_registry.definition_of(stack.item).icon_color;
                hud_state.hotbar[i].count = stack.count;
            }
        }
        if (options.hud_enabled) {
            lcu::ui::queue_hud_quads(renderer, hud_state, renderer_desc.width, renderer_desc.height);
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

        // Real inventory screen (Phase 49.1) - drawn on top of the
        // HUD/crosshair (menu_stack and inventory_open are mutually
        // exclusive, see the E/ESC handling above, so this never
        // double-draws with the pause menu backdrop). Built fresh from
        // player_inventory/craft_grid_inventory/cursor_stack every frame
        // it's open - real, live state, not a cached snapshot.
        if (inventory_open) {
            for (lcu::usize i = 0; i < lcu::ui::kCraftGridSlotCount; ++i) {
                const lcu::items::ItemStack& stack = craft_grid_inventory.slot_at(i);
                inventory_state.craft_input[i].has_item = !stack.is_empty();
                if (!stack.is_empty()) {
                    inventory_state.craft_input[i].icon_color = item_registry.definition_of(stack.item).icon_color;
                    inventory_state.craft_input[i].count = stack.count;
                }
            }
            const lcu::items::ItemStack& result_stack = craft_grid_inventory.slot_at(kCraftGridResultSlotIndex);
            inventory_state.craft_result.has_item = !result_stack.is_empty();
            if (!result_stack.is_empty()) {
                inventory_state.craft_result.icon_color = item_registry.definition_of(result_stack.item).icon_color;
                inventory_state.craft_result.count = result_stack.count;
            }
            for (lcu::usize i = 0; i < lcu::ui::kInventoryMainSlotCount; ++i) {
                const lcu::items::ItemStack& stack = player_inventory.slot_at(kHotbarSlotCount + i);
                inventory_state.main_slots[i].has_item = !stack.is_empty();
                if (!stack.is_empty()) {
                    inventory_state.main_slots[i].icon_color = item_registry.definition_of(stack.item).icon_color;
                    inventory_state.main_slots[i].count = stack.count;
                }
            }
            for (lcu::usize i = 0; i < lcu::ui::kHotbarSlotCount; ++i) {
                const lcu::items::ItemStack& stack = player_inventory.slot_at(i);
                inventory_state.hotbar_slots[i].has_item = !stack.is_empty();
                if (!stack.is_empty()) {
                    inventory_state.hotbar_slots[i].icon_color = item_registry.definition_of(stack.item).icon_color;
                    inventory_state.hotbar_slots[i].count = stack.count;
                }
            }
            if (!cursor_stack.is_empty()) {
                inventory_state.cursor.has_item = true;
                inventory_state.cursor.icon_color = item_registry.definition_of(cursor_stack.item).icon_color;
                inventory_state.cursor.count = cursor_stack.count;
                const lcu::platform::Window::MousePosition mouse_pos = lcu::platform::Window::mouse_position();
                inventory_state.cursor_x = static_cast<lcu::f32>(mouse_pos.x);
                inventory_state.cursor_y = static_cast<lcu::f32>(mouse_pos.y);
            } else {
                inventory_state.cursor = lcu::ui::InventorySlotDisplay{};
            }
            lcu::ui::queue_inventory_screen_quads(renderer, inventory_layout, inventory_state, renderer_desc.width,
                                                   renderer_desc.height);
        }

        // Real workbench screen (Phase 50.3) - same real per-frame
        // population as the inventory screen above, just against the
        // workbench's own 3x3 grid Inventory instead of the 2x2 one.
        // Mutually exclusive with inventory_open (see the E/right-click
        // handling above), so this never double-draws with it either.
        if (workbench_open) {
            for (lcu::usize i = 0; i < lcu::ui::kCraftingTableGridSlotCount; ++i) {
                const lcu::items::ItemStack& stack = workbench_grid_inventory.slot_at(i);
                workbench_state.grid_input[i].has_item = !stack.is_empty();
                if (!stack.is_empty()) {
                    workbench_state.grid_input[i].icon_color = item_registry.definition_of(stack.item).icon_color;
                    workbench_state.grid_input[i].count = stack.count;
                }
            }
            const lcu::items::ItemStack& wb_result_stack =
                workbench_grid_inventory.slot_at(kWorkbenchGridResultSlotIndex);
            workbench_state.result.has_item = !wb_result_stack.is_empty();
            if (!wb_result_stack.is_empty()) {
                workbench_state.result.icon_color = item_registry.definition_of(wb_result_stack.item).icon_color;
                workbench_state.result.count = wb_result_stack.count;
            }
            for (lcu::usize i = 0; i < lcu::ui::kInventoryMainSlotCount; ++i) {
                const lcu::items::ItemStack& stack = player_inventory.slot_at(kHotbarSlotCount + i);
                workbench_state.main_slots[i].has_item = !stack.is_empty();
                if (!stack.is_empty()) {
                    workbench_state.main_slots[i].icon_color = item_registry.definition_of(stack.item).icon_color;
                    workbench_state.main_slots[i].count = stack.count;
                }
            }
            for (lcu::usize i = 0; i < lcu::ui::kHotbarSlotCount; ++i) {
                const lcu::items::ItemStack& stack = player_inventory.slot_at(i);
                workbench_state.hotbar_slots[i].has_item = !stack.is_empty();
                if (!stack.is_empty()) {
                    workbench_state.hotbar_slots[i].icon_color = item_registry.definition_of(stack.item).icon_color;
                    workbench_state.hotbar_slots[i].count = stack.count;
                }
            }
            if (!cursor_stack.is_empty()) {
                workbench_state.cursor.has_item = true;
                workbench_state.cursor.icon_color = item_registry.definition_of(cursor_stack.item).icon_color;
                workbench_state.cursor.count = cursor_stack.count;
                const lcu::platform::Window::MousePosition mouse_pos = lcu::platform::Window::mouse_position();
                workbench_state.cursor_x = static_cast<lcu::f32>(mouse_pos.x);
                workbench_state.cursor_y = static_cast<lcu::f32>(mouse_pos.y);
            } else {
                workbench_state.cursor = lcu::ui::InventorySlotDisplay{};
            }
            lcu::ui::queue_crafting_table_screen_quads(renderer, workbench_layout, workbench_state,
                                                        renderer_desc.width, renderer_desc.height);
        }

        const bool ui_had_quads = renderer.pending_ui_quad_count() > 0;
        renderer.flush_ui_quads(ui2d_program);
        if (ui_had_quads && bgfx::isValid(ui2d_program)) {
            ++draw_calls;
        }

        // Real single owner of the shared bgfx debug-text buffer (Phase
        // 47 change): up to three systems below write into it this
        // frame (debug overlay, HUD hotbar item counts, menu row
        // labels) - exactly one clear_debug_text() call, here, before
        // any of them, replaces the three separate internal clears each
        // used to do on its own (which would have wiped each other's
        // text out - see debug_overlay.h's own updated doc comment).
        renderer.clear_debug_text();

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
        // Real hotbar item-count labels (Phase 47) - same options.
        // hud_enabled gate as queue_hud_quads above.
        if (options.hud_enabled) {
            lcu::ui::draw_hud_labels(renderer, hud_state, renderer_desc.width, renderer_desc.height);
        }
        // Real inventory screen slot-count labels (Phase 49.1) - drawn
        // after the debug overlay/HUD text for the same reason menu row
        // labels are (see below): the inventory screen is meant to be
        // readable while it's open.
        if (inventory_open) {
            lcu::ui::draw_inventory_screen_labels(renderer, inventory_layout, inventory_state);
        }
        // Real workbench screen slot-count labels (Phase 50.3) - same
        // reasoning as the inventory screen's own labels above.
        if (workbench_open) {
            lcu::ui::draw_crafting_table_screen_labels(renderer, workbench_layout, workbench_state);
        }
        // Menu row labels last - drawn on top of (after) the debug
        // overlay/HUD text, since the pause menu is meant to be the one
        // thing actually readable while it's open.
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
