#include "lcu/platform/input.h"

#include <SDL3/SDL.h>

namespace lcu::platform {

namespace {

struct Binding {
    SDL_Scancode scancode;
    Action action;
};

// Default (and currently only) keyboard bindings. Rebinding support is
// deferred until a settings/config system exists to persist it.
constexpr Binding kDefaultBindings[] = {
    {SDL_SCANCODE_W, Action::MoveForward},
    {SDL_SCANCODE_S, Action::MoveBackward},
    {SDL_SCANCODE_A, Action::MoveLeft},
    {SDL_SCANCODE_D, Action::MoveRight},
    {SDL_SCANCODE_SPACE, Action::Jump},
    {SDL_SCANCODE_LCTRL, Action::Crouch},
    {SDL_SCANCODE_LSHIFT, Action::Sprint},
    {SDL_SCANCODE_E, Action::Interact},
    {SDL_SCANCODE_I, Action::Inventory},
    {SDL_SCANCODE_UP, Action::LookUp},
    {SDL_SCANCODE_DOWN, Action::LookDown},
    {SDL_SCANCODE_LEFT, Action::LookLeft},
    {SDL_SCANCODE_RIGHT, Action::LookRight},
    {SDL_SCANCODE_F, Action::PlaceBlock},
    {SDL_SCANCODE_R, Action::CycleHotbar},
};

}  // namespace

void KeyboardInputBackend::update(InputState& state) const {
    int num_keys = 0;
    const bool* keys = SDL_GetKeyboardState(&num_keys);

    for (const Binding& binding : kDefaultBindings) {
        const bool down = keys != nullptr && binding.scancode < num_keys && keys[binding.scancode];
        state.set_down(binding.action, down);
    }
}

}  // namespace lcu::platform
