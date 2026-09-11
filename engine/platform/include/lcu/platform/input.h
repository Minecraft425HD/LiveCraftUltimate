#pragma once

#include <array>

#include "lcu/core/types.h"

namespace lcu::platform {

// Gameplay-facing input actions. Game/client code must query these, never
// raw scancodes/keycodes directly (brief section 27) - this is what lets
// keyboard, gamepad and touch all drive the same gameplay code later
// (mobile touch backend: brief section 28, not implemented yet).
enum class Action : u8 {
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    Jump,
    Crouch,
    Sprint,
    Interact,
    Inventory,
    // Arrow-key camera look, standing in for mouse-look until SDL
    // relative-mouse-mode plumbing exists (bound to a real device; not
    // meaningfully testable/verifiable in this sandbox either way -
    // arrow keys are a real, immediately usable interim control scheme,
    // not a placeholder that does nothing). See DECISIONS.md.
    LookUp,
    LookDown,
    LookLeft,
    LookRight,
    PlaceBlock,
    // Advances which item-backed block PlaceBlock places next (Phase 21) -
    // a plain index cycle through a fixed list, not a real hotbar UI (no
    // on-screen slot rendering/selection highlight exists yet - see
    // DECISIONS.md). A real, usable interim control scheme, same spirit as
    // LookUp/Down/Left/Right standing in for mouse-look above.
    CycleHotbar,
    // Attempts a quick-craft against RecipeRegistry using one of each
    // distinct item type currently held (Phase 23) - a real, minimal
    // crafting trigger, not a graphical crafting-grid UI (no way to
    // arrange items into specific cells exists yet - see DECISIONS.md).
    Craft,
    Count,
};

// Device-agnostic snapshot of which actions are currently held. Backends
// (KeyboardInputBackend today; gamepad/touch later) write into this each
// frame; gameplay code only ever reads it.
class InputState {
   public:
    bool is_down(Action action) const { return down_[static_cast<usize>(action)]; }
    void set_down(Action action, bool down) { down_[static_cast<usize>(action)] = down; }

   private:
    std::array<bool, static_cast<usize>(Action::Count)> down_{};
};

// Polls SDL's live keyboard state (SDL_GetKeyboardState) through a fixed
// default binding table and writes the result into an InputState. Call
// once per frame, after Window::pump_events(). Rebindable configs are a
// later addition (not needed for the Phase 1 vertical slice); the default
// bindings live in input.cpp so this header stays SDL-free.
class KeyboardInputBackend {
   public:
    void update(InputState& state) const;
};

}  // namespace lcu::platform
