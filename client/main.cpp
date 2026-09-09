#include <chrono>
#include <cstdlib>
#include <optional>
#include <unordered_map>
#include <vector>

#include "lcu/core/log.h"
#include "lcu/debug/frame_stats.h"
#include "lcu/jobs/job_system.h"
#include "lcu/physics/collision.h"
#include "lcu/physics/raycast.h"
#include "lcu/platform/input.h"
#include "lcu/platform/window.h"
#include "lcu/player/camera.h"
#include "lcu/player/movement_input.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"
#include "lcu/voxel/chunk_coord.h"
#include "lcu/voxel/greedy_mesher.h"
#include "lcu/world/world.h"
#include "lcu/world/worldgen.h"

#if defined(LCU_ENABLE_BGFX)
#include "lcu/math/mat4.h"
#include "lcu/platform/native_handle.h"
#include "lcu/rendering/chunk_mesh_upload.h"
#include "lcu/rendering/renderer.h"
#include "lcu/rendering/shader_program.h"
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

// How far (in chunks, Chebyshev distance) around spawn to keep loaded.
// Small and fixed for this vertical slice - real streaming driven by the
// player's current position/view direction (brief section 22) is a later
// refinement once World::update_streaming has a moving center to react to
// every frame; this client loads a static area once at startup.
constexpr lcu::i32 kLoadRadiusXZ = 1;
// Vertical range of chunks to load, covering worldgen's height range
// (kBaseHeight=32 +/- kHeightVariation=24 => world Y in [8,56]; edge
// length 16 means chunk Y in [0,3] covers world Y in [0,63]).
constexpr lcu::i32 kMinChunkY = 0;
constexpr lcu::i32 kMaxChunkY = 3;

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

    lcu::jobs::JobSystem job_system;

    lcu::world::World world(kWorldSeed, [&](lcu::voxel::Chunk& chunk, lcu::voxel::ChunkCoord coord) {
        lcu::world::worldgen::generate_terrain_chunk(chunk, coord, kWorldSeed, stone_id);
    });

#if defined(LCU_ENABLE_BGFX)
    std::unordered_map<lcu::voxel::ChunkCoord, lcu::rendering::GpuChunkMesh> gpu_meshes;
#endif

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

    LCU_LOG_INFO("Loading world (seed={}) around spawn...", kWorldSeed);
    for (lcu::i32 cx = -kLoadRadiusXZ; cx <= kLoadRadiusXZ; ++cx) {
        for (lcu::i32 cz = -kLoadRadiusXZ; cz <= kLoadRadiusXZ; ++cz) {
            for (lcu::i32 cy = kMinChunkY; cy <= kMaxChunkY; ++cy) {
                const lcu::voxel::ChunkCoord coord{cx, cy, cz};
                world.load_chunk(coord);
                remesh_and_upload(coord);
            }
        }
    }
    LCU_LOG_INFO("Loaded {} chunks", world.loaded_chunk_count());

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
    const lcu::i32 spawn_ground_y = lcu::world::worldgen::terrain_height(kWorldSeed, 0, 0);
    lcu::physics::PlayerPhysicsState player;
    player.aabb = make_player_aabb({0.0f, static_cast<lcu::f32>(spawn_ground_y), 0.0f});
    lcu::physics::PlayerPhysicsConfig physics_config;

    const auto is_solid = [&](lcu::voxel::BlockId id) { return block_registry.definition_of(id).has_collision; };

    lcu::player::FirstPersonCamera camera;
    // Start looking mostly straight down so the raycast this vertical
    // slice exercises has a guaranteed target (the ground right below
    // spawn) without needing any look input first - see DECISIONS.md.
    camera.pitch = -1.4f;

    lcu::platform::KeyboardInputBackend keyboard;
    lcu::platform::InputState input;
    lcu::platform::InputState previous_input;
    lcu::debug::FrameStats frame_stats;

    const bool verify_break_place = std::getenv("LCU_VERIFY_BREAK_PLACE") != nullptr;

    const std::optional<lcu::u64> max_frames = max_frames_from_env();
    lcu::u64 frame = 0;
    auto last_tick = std::chrono::steady_clock::now();

    while (window.pump_events()) {
        keyboard.update(input);

        if (verify_break_place) {
            input.set_down(lcu::platform::Action::Interact, frame == kVerifyBreakFrame);
            input.set_down(lcu::platform::Action::PlaceBlock, frame == kVerifyPlaceFrame);
        }

        const auto now = std::chrono::steady_clock::now();
        const lcu::f32 delta_seconds = std::chrono::duration<lcu::f32>(now - last_tick).count();
        last_tick = now;

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
        lcu::physics::apply_gravity(player, physics_config, delta_seconds);
        lcu::physics::integrate_player(world, player, horizontal_delta, physics_config, delta_seconds, is_solid);

        camera.position = {player.aabb.center().x, player.aabb.min.y + kEyeHeight, player.aabb.center().z};

        const auto hit = lcu::physics::raycast(world, camera.position, camera.forward(), kInteractRange, is_solid);

        const bool interact_pressed = input.is_down(lcu::platform::Action::Interact) &&
                                       !previous_input.is_down(lcu::platform::Action::Interact);
        const bool place_pressed = input.is_down(lcu::platform::Action::PlaceBlock) &&
                                    !previous_input.is_down(lcu::platform::Action::PlaceBlock);

        if (interact_pressed && hit) {
            LCU_LOG_INFO("Breaking block at world ({}, {}, {})", hit->world.x, hit->world.y, hit->world.z);
            const auto split = lcu::voxel::world_to_chunk_and_local(hit->world, lcu::voxel::Chunk::kEdgeLength);
            if (lcu::voxel::Chunk* target = world.chunk_at_mutable(split.chunk)) {
                target->set_block(split.local.x, split.local.y, split.local.z, lcu::voxel::kAirBlockId);
                remesh_and_upload(split.chunk);
                for (const lcu::voxel::ChunkCoord& neighbor : neighbors_sharing_boundary(split.chunk, split.local)) {
                    remesh_and_upload(neighbor);
                }
            } else {
                LCU_LOG_DEBUG("Break target's chunk isn't loaded, ignoring");
            }
        }

        if (place_pressed && hit) {
            const lcu::voxel::BlockWorldCoord place_pos{
                hit->world.x + static_cast<lcu::i64>(hit->normal.x),
                hit->world.y + static_cast<lcu::i64>(hit->normal.y),
                hit->world.z + static_cast<lcu::i64>(hit->normal.z),
            };
            LCU_LOG_INFO("Placing block at world ({}, {}, {})", place_pos.x, place_pos.y, place_pos.z);
            const auto split = lcu::voxel::world_to_chunk_and_local(place_pos, lcu::voxel::Chunk::kEdgeLength);
            if (lcu::voxel::Chunk* target = world.chunk_at_mutable(split.chunk)) {
                target->set_block(split.local.x, split.local.y, split.local.z, stone_id);
                remesh_and_upload(split.chunk);
                for (const lcu::voxel::ChunkCoord& neighbor : neighbors_sharing_boundary(split.chunk, split.local)) {
                    remesh_and_upload(neighbor);
                }
            } else {
                LCU_LOG_DEBUG("Place target's chunk isn't loaded, ignoring");
            }
        }

        previous_input = input;

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
        renderer.end_frame();
#endif

        if (const auto report = frame_stats.update(delta_seconds)) {
            LCU_LOG_INFO("fps={:.1f} frame_ms={:.2f} total_frames={}", report->fps, report->avg_frame_ms,
                         report->frame_count);
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

    LCU_LOG_INFO("LiveCraftUltimate client shutting down after {} frames", frame);
    return 0;
}
