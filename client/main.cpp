#include <chrono>
#include <cstdlib>
#include <optional>

#include "lcu/core/log.h"
#include "lcu/debug/frame_stats.h"
#include "lcu/jobs/job_system.h"
#include "lcu/platform/input.h"
#include "lcu/platform/window.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"
#include "lcu/voxel/greedy_mesher.h"

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

// Builds a single flat ground slab (brief section 80's vertical slice
// needs "a voxel chunk" - this is the smallest honest thing that
// qualifies: real BlockRegistry, real Chunk storage, real greedy
// meshing, not a mock). Not real world content - there is no world
// generation yet (Phase 3).
lcu::voxel::Chunk build_placeholder_chunk(lcu::voxel::BlockId stone) {
    lcu::voxel::Chunk chunk;
    for (lcu::u32 x = 0; x < lcu::voxel::Chunk::kEdgeLength; ++x) {
        for (lcu::u32 z = 0; z < lcu::voxel::Chunk::kEdgeLength; ++z) {
            chunk.set_block(x, 0, z, stone);
        }
    }
    return chunk;
}

}  // namespace

int main() {
    LCU_LOG_INFO("LiveCraftUltimate client starting (Phase 2: chunk -> greedy mesh -> GPU draw)");

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

    // --- Chunk -> job-dispatched greedy mesh -> (bgfx) GPU upload/draw ---
    lcu::voxel::BlockRegistry block_registry;
    lcu::voxel::BlockDefinition stone_def;
    stone_def.namespaced_id = "game:stone";
    stone_def.display_name = "Stone";
    stone_def.is_transparent = false;
    const lcu::voxel::BlockId stone_id = block_registry.register_block(stone_def);

    const lcu::voxel::Chunk chunk = build_placeholder_chunk(stone_id);

    lcu::jobs::JobSystem job_system;
    lcu::voxel::ChunkMesh chunk_mesh;
    const auto mesh_job = job_system.submit(
        [&] { chunk_mesh = lcu::voxel::mesh_chunk_greedy(chunk, block_registry); },
        lcu::jobs::JobPriority::High);
    job_system.wait(mesh_job);

    LCU_LOG_INFO("Meshed placeholder chunk: opaque {} vertices / {} indices", chunk_mesh.opaque.vertices.size(),
                 chunk_mesh.opaque.indices.size());

#if defined(LCU_ENABLE_BGFX)
    lcu::rendering::GpuChunkMesh gpu_mesh = lcu::rendering::upload_chunk_mesh_layer(chunk_mesh.opaque);
    LCU_LOG_INFO("Uploaded chunk mesh to GPU buffers: valid={} index_count={}", gpu_mesh.is_valid(),
                 gpu_mesh.index_count);

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

    const lcu::math::Mat4 model = lcu::math::Mat4::identity();
    const lcu::math::Mat4 view =
        lcu::math::Mat4::look_at({8.0f, 12.0f, 24.0f}, {8.0f, 0.0f, 8.0f}, {0.0f, 1.0f, 0.0f});
    const lcu::f32 aspect =
        static_cast<lcu::f32>(renderer_desc.width) / static_cast<lcu::f32>(renderer_desc.height);
    const lcu::math::Mat4 proj = lcu::math::Mat4::perspective(1.0f, aspect, 0.1f, 500.0f);
#endif

    lcu::platform::KeyboardInputBackend keyboard;
    lcu::platform::InputState input;
    lcu::debug::FrameStats frame_stats;

    const std::optional<lcu::u64> max_frames = max_frames_from_env();
    lcu::u64 frame = 0;
    auto last_tick = std::chrono::steady_clock::now();

    while (window.pump_events()) {
        keyboard.update(input);
        if (input.is_down(lcu::platform::Action::Interact)) {
            LCU_LOG_DEBUG("Interact held");
        }

#if defined(LCU_ENABLE_BGFX)
        renderer.begin_frame(0x303030ff);
        renderer.submit_chunk_mesh(gpu_mesh, chunk_program, model, view, proj);
        renderer.end_frame();
#endif

        const auto now = std::chrono::steady_clock::now();
        const lcu::f32 delta_seconds =
            std::chrono::duration<lcu::f32>(now - last_tick).count();
        last_tick = now;
        if (const auto report = frame_stats.update(delta_seconds)) {
            LCU_LOG_INFO("fps={:.1f} frame_ms={:.2f} total_frames={}", report->fps,
                         report->avg_frame_ms, report->frame_count);
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
    lcu::rendering::destroy_gpu_chunk_mesh(gpu_mesh);
#endif

    LCU_LOG_INFO("LiveCraftUltimate client shutting down after {} frames", frame);
    return 0;
}
