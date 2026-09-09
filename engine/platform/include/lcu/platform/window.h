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

   private:
    SDL_Window* handle_ = nullptr;
    i32 width_ = 0;
    i32 height_ = 0;
    bool should_close_ = false;
};

}  // namespace lcu::platform
