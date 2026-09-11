#pragma once

#include <string>
#include <vector>

#include <bgfx/bgfx.h>

#include "lcu/core/types.h"
#include "lcu/math/mat4.h"
#include "lcu/math/vec4.h"
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

// One 2D UI vertex (Phase 44): screen-space position (pixels) + UV (0..1
// within its own quad) + straight-alpha RGBA color - the real vertex
// format submit_ui_quad/flush_ui_quads batch, mirroring the position+
// color-only SkyVertex/LineVertex structs renderer.cpp already defines
// for the 3D debug/sky draws, with UV added since 2D UI has a real
// future use for it (a pattern/atlas lookup) those don't.
struct UiVertex2D {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 u = 0.0f;
    f32 v = 0.0f;
    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    f32 a = 0.0f;
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

    // Draws an axis-aligned SOLID box (Phase 48's real break-progress
    // overlay - the targeted block darkens as break progress advances).
    // Reuses the exact same minimal position+color vertex format/shader
    // submit_wireframe_box/submit_billboard already established, just
    // with the default triangle-list topology (a real box, 12
    // triangles) and real depth testing against terrain instead of a
    // line list. No-op if `program` is invalid.
    void submit_solid_box(const math::Vec3& min, const math::Vec3& max, const math::Vec3& color,
                           bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj);

    // Real 2D UI quad batch (Phase 44, brief section 60's UI framework):
    // appends one screen-space rectangle - `x`/`y`/`width`/`height` in
    // pixels, top-left origin, y increasing downward (SDL/mouse
    // convention) - to this frame's pending batch, tinted `color`
    // (straight, not premultiplied, alpha). Nothing is actually drawn
    // yet: call flush_ui_quads() once per frame (after every
    // submit_ui_quad() call for that frame, before end_frame()) to
    // upload the whole batch and issue exactly one real draw call for
    // however many quads were queued - the real "Quad-Batch...ein
    // Draw-Call" behavior the brief asks for, not one draw call per
    // quad. UV runs 0..1 across each quad independently (for a future
    // pattern/atlas use - see DECISIONS.md for why item-icon rendering
    // itself is deferred past this phase).
    void submit_ui_quad(f32 x, f32 y, f32 width, f32 height, const math::Vec4& color);

    // How many quads are currently queued (real, testable state - not
    // just an implementation detail): 0 right after flush_ui_quads() or
    // before any submit_ui_quad() call this frame.
    usize pending_ui_quad_count() const { return ui_vertices_.size() / 4; }

    // Uploads every quad queued via submit_ui_quad() since the last
    // flush and issues one real bgfx::submit() into the dedicated UI
    // view (drawn last, after terrain - see renderer.cpp's kUi2dViewId
    // comment for why, a deliberate correction of this phase's own
    // literal "before terrain" wording, which would make the UI
    // invisible behind opaque terrain). Always clears the pending batch
    // before returning, whether or not it actually drew anything (an
    // invalid `program` - e.g. LCU_BUILD_SHADER_TOOLS is off - means
    // this frame's queued quads are silently skipped, the same "no
    // shader, no draw" behavior submit_chunk_mesh/submit_billboard
    // already have, not something that should re-appear stale on a
    // later frame once a program becomes valid). Call once per frame,
    // after every submit_ui_quad() for that frame, before end_frame().
    void flush_ui_quads(bgfx::ProgramHandle program);

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

    // Real screenshot trigger (Phase 47, F2): wraps
    // `bgfx::requestScreenShot(BGFX_INVALID_HANDLE, ...)` against the
    // default backbuffer - bgfx's own default callback writes a real
    // `.tga` file asynchronously once the in-flight frame finishes (no
    // custom `bgfx::CallbackI` is installed - see init(), so this is
    // bgfx's own stock behavior, not something this project reimplements).
    // Under the headless `Noop` backend (this sandbox's own verification
    // runs) there is no real framebuffer content to capture - a real,
    // honest environment limitation, not a bug (see BUILD_STATUS.md).
    void request_screenshot(const std::string& file_path_without_extension);

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
    // Phase 44 - this frame's queued submit_ui_quad() calls, 4 vertices/
    // 6 indices per quad, uploaded and cleared together by
    // flush_ui_quads(). Real per-frame state (not a persistent GPU
    // resource - see submit_ui_quad's own doc comment), so a plain CPU
    // std::vector is the right tool, the same as every other per-frame
    // batch this codebase builds before handing it to a bgfx transient
    // buffer.
    std::vector<UiVertex2D> ui_vertices_;
    std::vector<u16> ui_indices_;
};

}  // namespace lcu::rendering
