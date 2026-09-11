#pragma once

#include <string>

#include "lcu/core/types.h"

struct SDL_Window;

namespace lcu::platform {

struct WindowDesc {
    std::string title = "LiveCraftUltimate";
    i32 width = 1280;
    i32 height = 720;
    bool resizable = true;
};

// Owns one SDL3 window. This is the only place in the engine that is
// allowed to include SDL headers outside engine/platform's own .cpp files
// and (later) engine/rendering's bgfx bootstrap, per the "no platform
// backend leaking into gameplay code" rule in ARCHITECTURE.md.
class Window : public NonCopyable {
   public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    // Pumps the SDL event queue. Returns false once a quit request (window
    // close, SDL_EVENT_QUIT) has been observed.
    bool pump_events();

    bool should_close() const { return should_close_; }

    i32 width() const { return width_; }
    i32 height() const { return height_; }

    SDL_Window* native_handle() const { return handle_; }

    // Real mouse-look capture (Phase 43): SDL's "relative mouse mode" -
    // hides the cursor, confines it to the window, and reports raw
    // motion deltas (DesktopInputBackend::update reads those via
    // SDL_GetRelativeMouseState) instead of an absolute on-screen
    // position. This is the standard FPS mouse-look mechanism; toggled
    // by ESC/Tab (release) and a click while free (recapture) in
    // client/main.cpp, and automatically released on focus loss (see
    // consume_focus_lost below).
    void set_relative_mouse_mode(bool enabled);
    bool relative_mouse_mode() const { return relative_mouse_mode_; }

    // This frame's total mouse wheel scroll (Phase 43), summed across
    // every SDL_EVENT_MOUSE_WHEEL event pump_events() saw since the
    // last call to this - positive means scrolled up/away from the
    // user, matching SDL's own sign convention. A real per-frame value,
    // not a polled one: SDL has no "wheel state" to poll, only discrete
    // scroll events, so pump_events() accumulates them and this
    // consumes (and resets) that accumulator. Call at most once per
    // frame, after pump_events().
    f32 consume_wheel_delta_y();

    // True exactly once, on the first call after this window lost
    // keyboard focus (SDL_EVENT_WINDOW_FOCUS_LOST) since the last call -
    // used to auto-release mouse capture (alt-tabbing away shouldn't
    // leave the cursor trapped in a window that's no longer focused).
    bool consume_focus_lost();

    // The directory the running executable lives in, trailing path
    // separator included (SDL_GetBasePath) - real, not a
    // current-working-directory assumption (see DECISIONS.md "shader
    // path resolves relative to CWD", Phase 43's fix): lets
    // asset-relative paths (shader binaries today) resolve correctly
    // regardless of which directory the client was launched from.
    // Static since it needs no live SDL_Window and is useful before one
    // exists.
    static std::string executable_base_path();

   private:
    SDL_Window* handle_ = nullptr;
    i32 width_ = 0;
    i32 height_ = 0;
    bool should_close_ = false;
    bool relative_mouse_mode_ = false;
    f32 wheel_delta_y_ = 0.0f;
    bool focus_lost_ = false;
};

}  // namespace lcu::platform
