#include "lcu/platform/input.h"

#include <SDL3/SDL.h>

#include "lcu/platform/key_bindings.h"

namespace lcu::platform {

namespace {

// True if `key` is currently held, whichever physical device it names -
// a keyboard scancode (via SDL_GetKeyboardState) or one of the three
// mouse-button constants (via SDL_GetMouseState's bitmask). Shared by
// every Action below so DesktopInputBackend never special-cases "this
// action happens to be mouse-bound" - the whole point of folding mouse
// buttons into the same PhysicalKey space KeyBindings already uses.
bool physical_key_is_down(PhysicalKey key) {
    if (key == kUnboundKey) {
        return false;
    }
    if (key == kMouseLeftKey || key == kMouseRightKey || key == kMouseMiddleKey) {
        const SDL_MouseButtonFlags buttons = SDL_GetMouseState(nullptr, nullptr);
        if (key == kMouseLeftKey) {
            return (buttons & SDL_BUTTON_LMASK) != 0;
        }
        if (key == kMouseRightKey) {
            return (buttons & SDL_BUTTON_RMASK) != 0;
        }
        return (buttons & SDL_BUTTON_MMASK) != 0;
    }

    int num_keys = 0;
    const bool* keys = SDL_GetKeyboardState(&num_keys);
    return keys != nullptr && key < num_keys && keys[key];
}

}  // namespace

void DesktopInputBackend::update(const KeyBindings& bindings, InputState& state) const {
    for (usize i = 0; i < static_cast<usize>(Action::Count); ++i) {
        const Action action = static_cast<Action>(i);
        bool down = false;
        for (const PhysicalKey key : bindings.bindings_for(action)) {
            if (physical_key_is_down(key)) {
                down = true;
                break;
            }
        }
        state.set_down(action, down);
    }

    // Real relative mouse-look (Phase 43): SDL accumulates motion since
    // the last call to this and resets its own internal counter, so no
    // manual reset is needed here as long as this is called exactly once
    // per frame (same contract the rest of this backend already has).
    // Meaningless (near-zero, harmless) unless the window currently owns
    // relative mouse mode - see InputState::mouse_delta_x's own doc
    // comment in input.h.
    float dx = 0.0f;
    float dy = 0.0f;
    SDL_GetRelativeMouseState(&dx, &dy);
    state.set_mouse_delta(dx, dy);
}

}  // namespace lcu::platform
