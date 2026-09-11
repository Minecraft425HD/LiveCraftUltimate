#pragma once

#include <array>
#include <string>

#include "lcu/core/types.h"
#include "lcu/platform/input.h"

namespace lcu::platform {

// A single physical input source a KeyBindings entry can point at - an SDL
// scancode (>= 0, see SDL_Scancode) for a keyboard key, or one of the three
// mouse-button constants below. A real, explicit small set: the mouse
// wheel is a direction event, not a "held" button, so it isn't
// representable here - it's handled separately (Window's own wheel-delta
// accessor, consumed directly into Action::CycleHotbar/CycleHotbarPrev in
// client/main.cpp), not through a binding.
using PhysicalKey = i32;

constexpr PhysicalKey kUnboundKey = -1;
constexpr PhysicalKey kMouseLeftKey = -2;
constexpr PhysicalKey kMouseRightKey = -3;
constexpr PhysicalKey kMouseMiddleKey = -4;

// Human-readable name for a PhysicalKey, real and round-trippable via
// parse_physical_key (Phase 45's options.txt persistence, and a future
// controls-menu label - Phase 46). A keyboard key's name comes straight
// from SDL_GetScancodeName/SDL_GetScancodeFromName, so it may read
// slightly differently than a hand-picked short form (e.g. "Left Ctrl"
// rather than "LCTRL") - real, SDL-provided, and always round-trips
// correctly, rather than a hand-rolled scancode<->string table that would
// need to be kept in sync with every SDL_SCANCODE_* by hand for a purely
// cosmetic difference.
std::string physical_key_name(PhysicalKey key);
PhysicalKey parse_physical_key(const std::string& name);

// Real rebindable keymap (Phase 43, brief section 27's "configurable
// keymapping"): each Action maps to up to kMaxBindingsPerAction physical
// keys - letting one action (e.g. Interact) be reachable from both a
// mouse button and a keyboard key at once, without forcing a strict 1:1
// mapping. Starts from real Minecraft-parity defaults (reset_to_defaults,
// also run by the constructor) and can be rebound in place via bind().
// This is the real data structure a controls menu (Phase 46) reads/writes
// and an options file (Phase 45) persists - the rebinding *UI* and actual
// persistence are those phases' job, not this one; KeyBindings itself is
// just the map.
class KeyBindings {
   public:
    static constexpr usize kMaxBindingsPerAction = 2;

    KeyBindings();

    // Whether `key` is one of `action`'s bound physical keys (kUnboundKey
    // never matches anything, including an empty slot).
    bool triggers(Action action, PhysicalKey key) const;

    // Overwrites one binding slot in place. `slot` must be < kMaxBindingsPerAction.
    void bind(Action action, usize slot, PhysicalKey key);

    // Restores every action to its Minecraft-parity default binding (see
    // key_bindings.cpp) - real "Zurücksetzen" support for Phase 46's
    // controls screen, not a stub: this is the exact same table the
    // constructor runs, not a separate hand-maintained copy that could
    // silently drift from it.
    void reset_to_defaults();

    const std::array<PhysicalKey, kMaxBindingsPerAction>& bindings_for(Action action) const {
        return bindings_[static_cast<usize>(action)];
    }

   private:
    std::array<std::array<PhysicalKey, kMaxBindingsPerAction>, static_cast<usize>(Action::Count)> bindings_{};
};

}  // namespace lcu::platform
