#include "lcu/platform/window.h"

#include <SDL3/SDL.h>

#include "lcu/core/assert.h"
#include "lcu/core/log.h"

namespace lcu::platform {

namespace {
// SDL_Init/Quit are reference-counted per subsystem by SDL itself, but we
// still track whether *this process* initialized the video subsystem so
// the last Window to close can shut it down deterministically instead of
// relying on process exit.
i32 g_window_count = 0;
}  // namespace

Window::Window(const WindowDesc& desc) {
    if (g_window_count == 0) {
        LCU_VERIFY(SDL_Init(SDL_INIT_VIDEO));
    }

    SDL_WindowFlags flags = 0;
    if (desc.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    handle_ = SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height, flags);
    if (!handle_) {
        LCU_LOG_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
        LCU_VERIFY(handle_ != nullptr);
    }

    width_ = desc.width;
    height_ = desc.height;
    ++g_window_count;

    LCU_LOG_INFO("Window created: \"{}\" {}x{}", desc.title, width_, height_);
}

Window::~Window() {
    if (handle_) {
        SDL_DestroyWindow(handle_);
        --g_window_count;
        if (g_window_count == 0) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    }
}

Window::Window(Window&& other) noexcept
    : handle_(other.handle_),
      width_(other.width_),
      height_(other.height_),
      should_close_(other.should_close_),
      relative_mouse_mode_(other.relative_mouse_mode_),
      wheel_delta_y_(other.wheel_delta_y_),
      focus_lost_(other.focus_lost_) {
    other.handle_ = nullptr;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        if (handle_) {
            SDL_DestroyWindow(handle_);
            --g_window_count;
        }
        handle_ = other.handle_;
        width_ = other.width_;
        height_ = other.height_;
        should_close_ = other.should_close_;
        relative_mouse_mode_ = other.relative_mouse_mode_;
        wheel_delta_y_ = other.wheel_delta_y_;
        focus_lost_ = other.focus_lost_;
        other.handle_ = nullptr;
    }
    return *this;
}

bool Window::pump_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                should_close_ = true;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                should_close_ = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                width_ = event.window.data1;
                height_ = event.window.data2;
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                // Real per-frame accumulation (Phase 43) - a frame with
                // multiple wheel events (a fast scroll, or just several
                // queued since the last pump) sums them, matching how a
                // real scroll gesture's total magnitude should read.
                wheel_delta_y_ += event.wheel.y;
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                // Latched, not overwritten - consume_focus_lost() clears
                // it, so a focus-lost that happens between two
                // consume_focus_lost() calls (unlikely at one poll per
                // frame, but real) isn't silently dropped.
                focus_lost_ = true;
                break;
            default:
                break;
        }
    }
    return !should_close_;
}

void Window::set_relative_mouse_mode(bool enabled) {
    if (handle_ != nullptr) {
        // Real return-value check, not ignored: under a headless/dummy
        // SDL video driver (this sandbox's own verification runs - see
        // BUILDING.md) there's no real mouse device to capture, so this
        // genuinely fails - logged, not fatal, since the caller (client/
        // main.cpp) still needs to keep running headlessly either way.
        if (!SDL_SetWindowRelativeMouseMode(handle_, enabled)) {
            LCU_LOG_WARN("SDL_SetWindowRelativeMouseMode({}) failed: {} (expected under a headless/dummy video driver)",
                         enabled, SDL_GetError());
        }
    }
    relative_mouse_mode_ = enabled;
}

f32 Window::consume_wheel_delta_y() {
    const f32 delta = wheel_delta_y_;
    wheel_delta_y_ = 0.0f;
    return delta;
}

bool Window::consume_focus_lost() {
    const bool was_lost = focus_lost_;
    focus_lost_ = false;
    return was_lost;
}

std::string Window::executable_base_path() {
    const char* base = SDL_GetBasePath();
    return base != nullptr ? std::string(base) : std::string();
}

Window::MousePosition Window::mouse_position() {
    float x = 0.0f;
    float y = 0.0f;
    SDL_GetMouseState(&x, &y);
    return {x, y};
}

}  // namespace lcu::platform
