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

    // Clears view 0 to `rgba` and marks it touched. Call once per frame,
    // before any submit_chunk_mesh() calls, then end_frame() after.
    void begin_frame(u32 clear_rgba);

    // Draws one chunk mesh with the given program/transforms into view 0.
    // No-op (logged at debug level would be noisy per-frame - silently
    // skipped) if `mesh` has no geometry or `program` is invalid, both
    // of which are legitimate states today (an empty chunk; no shader
    // compiled because LCU_BUILD_SHADER_TOOLS is off - see BUILDING.md).
    void submit_chunk_mesh(const GpuChunkMesh& mesh, bgfx::ProgramHandle program, const math::Mat4& model,
                            const math::Mat4& view, const math::Mat4& proj);

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
};

}  // namespace lcu::rendering
