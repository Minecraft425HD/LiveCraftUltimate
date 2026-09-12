#pragma once

#include <optional>
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

    // Absolute cursor position, in window pixels from the top-left
    // (SDL/mouse convention, same origin submit_ui_quad already uses) -
    // only meaningful while relative_mouse_mode() is false (Phase 46's
    // pause menu mouse hit-testing: ESC already releases capture before
    // the menu needs a real click position, see client/main.cpp). Real
    // SDL_GetMouseState under the hood, the same call
    // DesktopInputBackend already uses for button state - no live
    // Window needed, so this is static like executable_base_path().
    struct MousePosition {
        f32 x = 0.0f;
        f32 y = 0.0f;
    };
    static MousePosition mouse_position();

    // Moves the OS cursor to an absolute window-pixel position
    // (SDL_WarpMouseInWindow) - real headless verification's only way to
    // exercise a real mouse-position-driven click path (Phase 49's
    // inventory screen hit-testing) deterministically, the same way
    // synthesizing a real key press via InputState::set_down already
    // exercises keyboard-driven paths. Requires a live window, so it's
    // an instance method (unlike the static mouse_position() getter).
    void warp_mouse(f32 x, f32 y);

    // Real fullscreen toggle (Phase 47, F11) - wraps
    // `SDL_SetWindowFullscreen`. Logs a warning (not fatal, same
    // tolerance `set_relative_mouse_mode` already has) if SDL reports
    // failure - a headless/dummy video driver has no real display to
    // occupy fullscreen, so this is expected to be a real, harmless
    // no-op there.
    void set_fullscreen(bool enabled);
    bool fullscreen() const { return fullscreen_; }

   private:
    SDL_Window* handle_ = nullptr;
    i32 width_ = 0;
    i32 height_ = 0;
    bool should_close_ = false;
    bool relative_mouse_mode_ = false;
    f32 wheel_delta_y_ = 0.0f;
    bool focus_lost_ = false;
    bool fullscreen_ = false;
};

// Real Phase 62.3 "Load own skin..." file picker - a thin async wrapper
// around SDL_ShowOpenFileDialog (native platform dialog: a GTK/Cocoa/
// Windows picker, or the XDG desktop portal on Linux) so gameplay code
// stays free of a direct SDL3 dependency, matching Window's own role
// for every other SDL surface this project touches. Not a Window
// method: SDL's own documented contract allows the real callback to
// run on a different thread than the one that requested it, so the
// real result is buffered in a small thread-safe, module-local mailbox
// (window.cpp) rather than mutating a live Window instance from a
// background thread. Ignored (with a logged warning) if a request is
// already pending - this project never needs more than one open file
// dialog at a time.
void request_open_png_file_dialog(Window& window);

// Non-blocking poll for request_open_png_file_dialog()'s own real
// result. Returns std::nullopt while no answer is available yet (no
// request was ever made, or one is still open) - keep polling once per
// frame. Once available, the outer std::optional is consumed (a second
// poll right after goes back to "nothing pending"); the *inner*
// std::optional is the real answer: a value is the user's chosen
// file's real path, std::nullopt means the user cancelled or the
// platform has no dialog backend available at all (this project's own
// headless sandbox - a real, expected, already-logged environment
// limitation, not a caller-visible error).
std::optional<std::optional<std::string>> poll_open_png_file_dialog_result();

}  // namespace lcu::platform
