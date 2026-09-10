#pragma once

namespace lcu::platform {

class Window;

// Native platform handles bgfx needs to attach a swapchain to a window.
// `nwh` (native window handle) is required; `ndt` (native display type) is
// only needed on backends that separate the two (X11/Wayland). Both are
// void* by design so this header never needs to include SDL or any
// platform SDK header — those live only in native_handle.cpp, the one
// sanctioned place per ARCHITECTURE.md for this kind of platform ifdef
// outside engine/platform's other internals.
struct NativeWindowHandle {
    void* nwh = nullptr;
    void* ndt = nullptr;
};

// Returns {nullptr, nullptr} if the running backend/driver does not expose
// a native handle (e.g. SDL's dummy video driver, used for headless
// verification in CI/sandboxes with no display). Callers must treat a null
// nwh as "no real GPU surface available" and fall back accordingly (see
// engine/rendering's Renderer, which falls back to bgfx's Noop backend).
NativeWindowHandle get_native_window_handle(const Window& window);

}  // namespace lcu::platform
