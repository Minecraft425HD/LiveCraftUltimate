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
      should_close_(other.should_close_) {
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
            default:
                break;
        }
    }
    return !should_close_;
}

}  // namespace lcu::platform
