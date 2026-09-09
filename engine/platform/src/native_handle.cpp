#include "lcu/platform/native_handle.h"

#include <SDL3/SDL.h>

#include "lcu/core/log.h"
#include "lcu/platform/window.h"

// This is the one file in the engine allowed to branch on platform macros
// (ARCHITECTURE.md: platform-specific code lives in engine/platform). It
// only ever extracts a native handle from the SDL window properties SDL3
// already exposes per-backend; it never talks to a graphics API directly.

namespace lcu::platform {

NativeWindowHandle get_native_window_handle(const Window& window) {
    SDL_Window* sdl_window = window.native_handle();
    if (!sdl_window) {
        return {};
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(sdl_window);
    NativeWindowHandle handle;

#if defined(__linux__) && !defined(__ANDROID__)
    // Prefer X11; fall back to Wayland. Under SDL's dummy/offscreen video
    // driver (used for headless verification, e.g. this sandbox), neither
    // property is set and both pointers stay null - callers must handle
    // that (see native_handle.h).
    void* x11_window = reinterpret_cast<void*>(
        SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    void* x11_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    if (x11_window && x11_display) {
        handle.nwh = x11_window;
        handle.ndt = x11_display;
        return handle;
    }

    void* wl_surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    void* wl_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    if (wl_surface && wl_display) {
        handle.nwh = wl_surface;
        handle.ndt = wl_display;
        return handle;
    }
#elif defined(_WIN32)
    handle.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__)
    // SDL_PROP_WINDOW_COCOA_WINDOW_POINTER on macOS, UIKIT on iOS - both
    // resolve at compile time via SDL's own TARGET_OS_* dispatch in its
    // public headers, so no extra ifdef nesting is needed here.
    handle.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    if (!handle.nwh) {
        handle.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
    }
#elif defined(__ANDROID__)
    handle.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
#endif

    if (!handle.nwh) {
        LCU_LOG_WARN(
            "No native window handle available from SDL (headless/dummy video driver, or "
            "unsupported platform branch) - renderer will fall back to a headless backend");
    }

    return handle;
}

}  // namespace lcu::platform
