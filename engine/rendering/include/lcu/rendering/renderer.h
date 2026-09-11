#pragma once

#include <string>

#include <bgfx/bgfx.h>

#include "lcu/core/types.h"
#include "lcu/math/mat4.h"
#include "lcu/platform/native_handle.h"
#include "lcu/rendering/chunk_mesh_upload.h"

namespace lcu::rendering {

struct RendererDesc {
    platform::NativeWindowHandle window_handle;
    u32 width = 1280;
    u32 height = 720;
    // Forces bgfx's headless Noop backend regardless of window_handle -
    // used for CI/sandbox verification where no real GPU/display exists.
    // When false and window_handle.nwh is null, Renderer::init falls back
    // to Noop automatically anyway (there is nothing else it could do).
    bool force_headless = false;
};

// Thin wrapper around bgfx's global init/frame/shutdown lifecycle. This is
// the only engine subsystem below the client allowed to include bgfx
// headers, per ARCHITECTURE.md's rendering abstraction layering
// (game -> engine -> engine/rendering -> bgfx -> platform backend).
class Renderer : public NonCopyable {
   public:
    Renderer() = default;
    ~Renderer();

    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // Returns false if bgfx failed to initialize (logged). Safe to call
    // once per Renderer instance.
    bool init(const RendererDesc& desc);

    // Clears view 0 to `color` (each channel 0-1, alpha separate - Phase
    // 27's real sky color, interpolated from DayNightCycle::
    // sky_light_scale() by the caller, replaces the fixed
    // brief-Phase-1-era sky-blue constant this used to always be) and
    // marks it touched. Call once per frame, before any
    // submit_chunk_mesh() calls, then end_frame() after.
    void begin_frame(const math::Vec3& clear_color, f32 alpha = 1.0f);

    // Draws one chunk mesh with the given program/transforms into view 0.
    // No-op (logged at debug level would be noisy per-frame - silently
    // skipped) if `mesh` has no geometry or `program` is invalid, both
    // of which are legitimate states today (an empty chunk; no shader
    // compiled because LCU_BUILD_SHADER_TOOLS is off - see BUILDING.md).
    // `sky_light_scale` (Phase 28) is DayNightCycle::sky_light_scale()
    // for the frame being drawn - the one real time signal fs_chunk.sc's
    // u_skyLightScale uniform needs to weight each vertex's already-
    // computed, already-packed sky light by; set once per draw call
    // here, never recomputed per-voxel/per-frame in meshing itself (see
    // DECISIONS.md "Light is never computed per frame").
    void submit_chunk_mesh(const GpuChunkMesh& mesh, bgfx::ProgramHandle program, const math::Mat4& model,
                            const math::Mat4& view, const math::Mat4& proj, f32 sky_light_scale = 1.0f);

    // Draws one camera-facing colored quad (Phase 27 - the sun/moon)
    // into a dedicated sky view, executed before the terrain view so
    // terrain naturally occludes it (see renderer.cpp's kSkyViewId
    // comment) - no depth test/write, no texture (no atlas exists yet,
    // same as chunk rendering before Phase 26). `right`/`up` should be
    // the camera's own basis vectors so the quad actually faces the
    // camera; `center` is the quad's world-space position, `half_size`
    // its half-width/height in world units. No-op if `program` is
    // invalid (e.g. LCU_BUILD_SHADER_TOOLS is off).
    void submit_billboard(const math::Vec3& center, const math::Vec3& right, const math::Vec3& up, f32 half_size,
                           const math::Vec3& color, bgfx::ProgramHandle program, const math::Mat4& view,
                           const math::Mat4& proj);

    // Draws an axis-aligned wireframe box (Phase 36 - entity debug
    // boxes, brief section 60) as 12 line-list edges from `min` to
    // `max` in world space, tinted `color`. Reuses the same minimal
    // position+color vertex format/shader submit_billboard already
    // established (vs_sky.sc/fs_sky.sc - unlit, no lighting concept
    // needed for a debug aid) rather than adding a third shader pair
    // for what's visually the same "flat-colored, no lighting" need.
    // Drawn into view 0 (the terrain view, not the sky view) with real
    // depth testing against solid terrain - a box behind a wall is
    // correctly hidden, not painted through it. No-op if `program` is
    // invalid (e.g. LCU_BUILD_SHADER_TOOLS is off).
    void submit_wireframe_box(const math::Vec3& min, const math::Vec3& max, const math::Vec3& color,
                               bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj);

    // Advances one bgfx frame. Returns the frame count bgfx reports,
    // mainly useful for tests/logging.
    u32 end_frame();

    // Real on-screen text (Phase 12, brief section 60's debug overlay/
    // engine/ui): bgfx's built-in VGA-style debug-text character buffer
    // (BGFX_DEBUG_TEXT, enabled in init()) - no font/texture-atlas
    // renderer exists yet (see DECISIONS.md), but this is genuinely
    // rendered on screen, not a log line. `x`/`y` are character-cell
    // coordinates (8x16 cells from the top-left), not pixels. This is the
    // only place besides submit_chunk_mesh() that touches bgfx per frame -
    // engine/ui (and anything else) calls through here rather than
    // including bgfx itself, keeping ARCHITECTURE.md's "only
    // engine/rendering includes bgfx headers" rule intact.
    void clear_debug_text();
    void draw_debug_text(u16 x, u16 y, u8 color_attr, const std::string& text);

    void resize(u32 width, u32 height);

    bool is_headless() const { return headless_; }

   private:
    bool initialized_ = false;
    bool headless_ = false;
    u32 width_ = 0;
    u32 height_ = 0;
    // Phase 28 - see submit_chunk_mesh's doc comment. Created
    // unconditionally in init() and destroyed in ~Renderer(); bgfx
    // uniforms don't require any particular shader to be compiled or
    // bound (setting one a currently-submitted program doesn't declare
    // is simply ignored, not an error), so this doesn't need to be
    // gated on whether LCU_BUILD_SHADER_TOOLS built real chunk shaders.
    bgfx::UniformHandle sky_light_scale_uniform_ = BGFX_INVALID_HANDLE;
};

}  // namespace lcu::rendering
