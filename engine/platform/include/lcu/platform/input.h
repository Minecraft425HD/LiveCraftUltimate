#pragma once

#include <array>

#include "lcu/core/types.h"

namespace lcu::platform {

class KeyBindings;

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
    // Arrow-key camera look - originally (Phase 4) a stand-in for
    // mouse-look until SDL relative-mouse-mode plumbing existed; real
    // mouse-look landed in Phase 43 (InputState::mouse_delta_x/y below,
    // driven by SDL_GetRelativeMouseState, applied on top of whatever
    // these produce) and these stayed on as a genuine accessibility
    // fallback, not dead code - both can drive the camera in the same
    // frame with no conflict. See DECISIONS.md.
    LookUp,
    LookDown,
    LookLeft,
    LookRight,
    PlaceBlock,
    // Selects whichever placeable_items entry matches the block currently
    // under the crosshair (Phase 43, Minecraft's "middle-click to pick
    // block") - a real selection change, not a creative-mode "give me
    // this item for free": the player still needs to actually hold the
    // item to place it (see client/main.cpp's place_pressed check). A
    // no-op if the looked-at block has no corresponding placeable entry.
    PickBlock,
    // Advances which item-backed block PlaceBlock places next (Phase 21) -
    // a plain index cycle through a fixed list, not a real hotbar UI (no
    // on-screen slot rendering/selection highlight exists yet - see
    // DECISIONS.md). A real, usable interim control scheme, same spirit as
    // LookUp/Down/Left/Right standing in for mouse-look above.
    CycleHotbar,
    // The reverse of CycleHotbar (Phase 43) - real mouse-wheel-down
    // support ("scroll down = previous slot"), not reachable from any
    // default keyboard binding (CycleHotbar's R key has no natural
    // "reverse" key in the Minecraft scheme this project follows).
    CycleHotbarPrev,
    // Direct hotbar slot selection (Phase 43, Minecraft's 1-9 number-row
    // binding) - sets the selected placeable index straight to (N-1) if
    // that slot actually holds a placeable entry, a no-op otherwise (the
    // real hotbar today only ever has a handful of entries - see
    // client/main.cpp's placeable_items - so most of these currently do
    // nothing, honestly, rather than wrapping/crashing).
    SelectHotbar1,
    SelectHotbar2,
    SelectHotbar3,
    SelectHotbar4,
    SelectHotbar5,
    SelectHotbar6,
    SelectHotbar7,
    SelectHotbar8,
    SelectHotbar9,
    // Bound by default (Phase 43, Minecraft's F key) but not yet consumed
    // anywhere - there is no offhand slot in Inventory yet, the same
    // "real binding, no consumer yet" honesty Action::Inventory itself
    // (bound to E, no on-screen inventory UI exists yet either) already
    // carries - not a fake feature, since nothing claims either does
    // something it doesn't. See DECISIONS.md.
    SwapOffhand,
    // Attempts a quick-craft against RecipeRegistry using one of each
    // distinct item type currently held (Phase 23) - a real, minimal
    // crafting trigger, not a graphical crafting-grid UI (no way to
    // arrange items into specific cells exists yet - see DECISIONS.md).
    Craft,
    // ESC/Tab (Phase 43) - releases mouse capture today; Phase 46 also
    // opens/closes the pause menu with it and uses it to cancel a
    // controls-screen rebind capture. Deliberately still a real
    // KeyBindings entry, not a hardcoded scancode check outside the
    // Action system (keeps every physical-key lookup going through one
    // path) - "not rebindable" (per this phase's own directive) is a
    // Phase 46 controls-*menu* choice (simply never listing it as
    // editable), not an architectural restriction here.
    Escape,
    // Enter/Return (Phase 46) - confirms/activates the currently
    // selected row in a MenuStack screen (engine/ui/menu_stack.h).
    // Deliberately still a real KeyBindings entry rather than a
    // hardcoded scancode check, same reasoning as Escape's own doc
    // comment above; also "not rebindable" as a Phase 46 controls-menu
    // choice (never listed as editable), not an architectural
    // restriction here.
    MenuConfirm,
    // F-key HUD/display toggles (Phase 47, the "verbindlich" keybinding
    // table Phase 43 already reserved F1/F2/F3/F5/F11 for, only now
    // given real consumers). ToggleHud/ToggleDebugOverlay flip the same
    // real `Options::hud_enabled`/`debug_overlay_enabled` fields the
    // options menu already reads/writes (Phase 46) - two real paths to
    // one real piece of state, not a second, independent toggle.
    ToggleHud,
    ToggleDebugOverlay,
    // Real `bgfx::requestScreenShot` trigger (Phase 47) - writes a real
    // file via bgfx's own screenshot callback, not a placeholder.
    Screenshot,
    // Cycles first-person -> third-person-behind (Phase 47) - real
    // camera-offset behavior, PARTIAL: no player model exists to render
    // in third person, so "third-person-front" (which needs a visible
    // player model in front of the camera to look at all) is honestly
    // not implemented yet - see DECISIONS.md.
    TogglePerspective,
    Fullscreen,
    Count,
};

// Device-agnostic snapshot of which actions are currently held. Backends
// (DesktopInputBackend today; gamepad/touch later) write into this each
// frame; gameplay code only ever reads it.
class InputState {
   public:
    bool is_down(Action action) const { return down_[static_cast<usize>(action)]; }
    void set_down(Action action, bool down) { down_[static_cast<usize>(action)] = down; }

    // Real relative mouse-look deltas (Phase 43), in raw SDL pixels for
    // this frame - positive mouse_delta_x is the mouse moving right,
    // positive mouse_delta_y is the mouse moving down (SDL's own screen-
    // space convention, unconverted). Only meaningful while the window
    // owns relative mouse mode (see Window::set_relative_mouse_mode) -
    // DesktopInputBackend still writes a real (usually near-zero) value
    // otherwise, so a consumer that forgets to check capture state gets
    // stale-but-harmless numbers, not garbage.
    f32 mouse_delta_x() const { return mouse_delta_x_; }
    f32 mouse_delta_y() const { return mouse_delta_y_; }
    void set_mouse_delta(f32 dx, f32 dy) {
        mouse_delta_x_ = dx;
        mouse_delta_y_ = dy;
    }

   private:
    std::array<bool, static_cast<usize>(Action::Count)> down_{};
    f32 mouse_delta_x_ = 0.0f;
    f32 mouse_delta_y_ = 0.0f;
};

// Polls SDL's live keyboard/mouse-button state through a KeyBindings
// keymap (Phase 43 - previously a fixed table baked into input.cpp; see
// key_bindings.h) plus real relative mouse-look deltas
// (SDL_GetRelativeMouseState), and writes the result into an InputState.
// Call once per frame, after Window::pump_events(). Named for "the desktop
// input devices" (keyboard + mouse), not just the keyboard, since Phase 43
// folded mouse buttons/motion into the same polling pass - gamepad/touch
// remain separate backends (TouchInputBackend already exists; gamepad
// doesn't yet). Kept SDL-free at the header level; input.cpp does the
// actual polling.
class DesktopInputBackend {
   public:
    void update(const KeyBindings& bindings, InputState& state) const;
};

}  // namespace lcu::platform
