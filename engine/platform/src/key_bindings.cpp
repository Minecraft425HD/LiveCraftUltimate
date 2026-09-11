#include "lcu/platform/key_bindings.h"

#include <algorithm>

#include <SDL3/SDL.h>

namespace lcu::platform {

std::string physical_key_name(PhysicalKey key) {
    switch (key) {
        case kMouseLeftKey:
            return "MOUSE_LEFT";
        case kMouseRightKey:
            return "MOUSE_RIGHT";
        case kMouseMiddleKey:
            return "MOUSE_MIDDLE";
        default:
            break;
    }
    if (key < 0) {
        return "UNBOUND";
    }
    const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(key));
    return (name != nullptr && name[0] != '\0') ? std::string(name) : std::string("UNBOUND");
}

PhysicalKey parse_physical_key(const std::string& name) {
    if (name == "MOUSE_LEFT") {
        return kMouseLeftKey;
    }
    if (name == "MOUSE_RIGHT") {
        return kMouseRightKey;
    }
    if (name == "MOUSE_MIDDLE") {
        return kMouseMiddleKey;
    }
    if (name.empty() || name == "UNBOUND") {
        return kUnboundKey;
    }
    const SDL_Scancode code = SDL_GetScancodeFromName(name.c_str());
    return code != SDL_SCANCODE_UNKNOWN ? static_cast<PhysicalKey>(code) : kUnboundKey;
}

KeyBindings::KeyBindings() { reset_to_defaults(); }

bool KeyBindings::triggers(Action action, PhysicalKey key) const {
    if (key == kUnboundKey) {
        return false;
    }
    const auto& slots = bindings_[static_cast<usize>(action)];
    return std::find(slots.begin(), slots.end(), key) != slots.end();
}

void KeyBindings::bind(Action action, usize slot, PhysicalKey key) {
    bindings_[static_cast<usize>(action)][slot] = key;
}

void KeyBindings::reset_to_defaults() {
    for (auto& slots : bindings_) {
        slots.fill(kUnboundKey);
    }

    const auto set = [this](Action action, PhysicalKey key) { bindings_[static_cast<usize>(action)][0] = key; };

    // Movement - unchanged from every earlier phase.
    set(Action::MoveForward, SDL_SCANCODE_W);
    set(Action::MoveBackward, SDL_SCANCODE_S);
    set(Action::MoveLeft, SDL_SCANCODE_A);
    set(Action::MoveRight, SDL_SCANCODE_D);
    set(Action::Jump, SDL_SCANCODE_SPACE);
    // Minecraft-parity swap (Phase 43): this project's own pre-Phase-43
    // defaults had these backwards (Sprint=Shift, Crouch=Ctrl) - real
    // Minecraft binds Sprint to Ctrl and Sneak/Crouch to Shift.
    set(Action::Sprint, SDL_SCANCODE_LCTRL);
    set(Action::Crouch, SDL_SCANCODE_LSHIFT);

    // Mouse-driven actions (Phase 43) - Interact/PlaceBlock/PickBlock are
    // mouse-only by default now (no keyboard fallback binding), matching
    // the "Minecraft-Default-Keybindings" reference table this phase's
    // own directive specifies (attack_or_break/use_or_place/pick_block
    // are mouse-exclusive there).
    set(Action::Interact, kMouseLeftKey);
    set(Action::PlaceBlock, kMouseRightKey);
    set(Action::PickBlock, kMouseMiddleKey);

    // Hotbar (Phase 43): R still cycles forward (unchanged default from
    // Phase 21); CycleHotbarPrev has no keyboard default - it exists for
    // the mouse wheel's scroll-down direction only (client/main.cpp
    // drives it directly from Window::consume_wheel_delta_y, not through
    // this keymap - a wheel isn't a "held key" this table can represent).
    // Direct number-row selection is real too (SelectHotbar1..9).
    set(Action::CycleHotbar, SDL_SCANCODE_R);
    set(Action::SelectHotbar1, SDL_SCANCODE_1);
    set(Action::SelectHotbar2, SDL_SCANCODE_2);
    set(Action::SelectHotbar3, SDL_SCANCODE_3);
    set(Action::SelectHotbar4, SDL_SCANCODE_4);
    set(Action::SelectHotbar5, SDL_SCANCODE_5);
    set(Action::SelectHotbar6, SDL_SCANCODE_6);
    set(Action::SelectHotbar7, SDL_SCANCODE_7);
    set(Action::SelectHotbar8, SDL_SCANCODE_8);
    set(Action::SelectHotbar9, SDL_SCANCODE_9);

    // UI/misc (Phase 43 defaults table) - E now means Inventory (was
    // Interact pre-Phase-43, since Interact moved to the mouse above); F
    // is SwapOffhand (was PlaceBlock pre-Phase-43, for the same reason).
    set(Action::Inventory, SDL_SCANCODE_E);
    set(Action::SwapOffhand, SDL_SCANCODE_F);
    set(Action::Craft, SDL_SCANCODE_C);
    bindings_[static_cast<usize>(Action::Escape)][0] = SDL_SCANCODE_ESCAPE;
    bindings_[static_cast<usize>(Action::Escape)][1] = SDL_SCANCODE_TAB;

    // Arrow-key look fallback - unchanged (see Action::LookUp's own doc
    // comment in input.h for why this stays alongside real mouse-look).
    set(Action::LookUp, SDL_SCANCODE_UP);
    set(Action::LookDown, SDL_SCANCODE_DOWN);
    set(Action::LookLeft, SDL_SCANCODE_LEFT);
    set(Action::LookRight, SDL_SCANCODE_RIGHT);
}

}  // namespace lcu::platform
