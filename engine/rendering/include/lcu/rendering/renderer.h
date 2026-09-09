#pragma once

#include "lcu/core/types.h"
#include "lcu/platform/native_handle.h"

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

    // Clears view 0 to `rgba` and advances one bgfx frame. Returns the
    // frame count bgfx reports, mainly useful for tests/logging.
    u32 render_clear_frame(u32 rgba);

    void resize(u32 width, u32 height);

    bool is_headless() const { return headless_; }

   private:
    bool initialized_ = false;
    bool headless_ = false;
    u32 width_ = 0;
    u32 height_ = 0;
};

}  // namespace lcu::rendering
