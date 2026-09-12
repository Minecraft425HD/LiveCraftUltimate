#pragma once

#include <string>

#include "lcu/core/types.h"
#include "lcu/platform/key_bindings.h"

namespace lcu::platform {

// Real persistent client settings (Phase 45, brief section 60's options
// menu groundwork): mouse sensitivity, field of view, HUD/debug-overlay
// toggles, and the full rebindable keymap (KeyBindings) - starts at
// real, working defaults (KeyBindings' own constructor already gives
// real Minecraft-parity bindings; the scalar fields below match this
// project's existing hardcoded constants as of Phase 43/44 - see
// client/main.cpp's own kMouseSensitivity), loadable from and savable
// to a real text file. Phase 46's options/controls screens read/write
// this struct directly; this phase only builds the real, tested
// load/save mechanics underneath them.
struct Options {
    f32 mouse_sensitivity = 0.0022f;
    i32 fov = 70;
    bool hud_enabled = true;
    bool debug_overlay_enabled = false;
    KeyBindings key_bindings;

    // Real, persisted player-skin choice (Phase 62, brief section
    // 62.4) - either one of the 5 builtin lcu::assets::SkinPreset
    // names or a custom uploaded skin's own file stem, resolved
    // against a real lcu::assets::SkinCatalog at startup (client/
    // main.cpp), falling back to "Steve" there if this name isn't
    // found in that catalog (a skins-folder file the player deleted
    // since, or a stale/corrupt value) - Options itself stays a plain
    // string holder with no SkinCatalog dependency at all, the same
    // separation KeyBindings already keeps from any real input device.
    std::string skin_name = "Steve";

    // Real, human-editable `key=value` text format, one setting per
    // line, `#`-prefixed comment lines ignored - see options.cpp for
    // the exact key names (`mouse_sensitivity`, `fov`, `hud_enabled`,
    // `debug_overlay_enabled`, `skin`, and one `key.<action_name>`/
    // `key.<action_name>.alt` pair per rebindable Action, `.alt` only
    // written when that Action's second binding slot is actually
    // bound).
    //
    // Returns false if `path` couldn't be opened (a real, expected
    // first-run state - "missing file -> keep the caller's current
    // values", which for a freshly-constructed Options are the real
    // defaults above, not an error condition to log loudly about).
    // Tolerant of a corrupt/unrecognized individual line: that one line
    // is skipped (this Options instance's existing value for whatever
    // it would have set is left alone) and every other real line still
    // loads - never aborts the whole load over one bad line.
    bool load(const std::string& path);

    // Overwrites `path` with this Options' current values. Logs (not
    // fatal) if the file couldn't be opened for writing - the caller
    // keeps running either way, the same "real failure, not a crash"
    // pattern engine/rendering::load_shader_from_file already uses for
    // a missing/unreadable file.
    void save(const std::string& path) const;

    // The real, per-OS location a live install should actually load
    // from/save to (SDL_GetPrefPath("LiveCraftUltimate",
    // "LiveCraftUltimate") + "options.txt") - not a hardcoded
    // CWD-relative path (see DECISIONS.md's Phase 43 shader-path fix
    // for why "works from wherever this sandbox happens to run it"
    // isn't good enough). Falls back to a plain "options.txt" (current
    // working directory) if SDL_GetPrefPath itself fails (e.g. no
    // writable per-user directory available) - a real, honest degrade,
    // not a crash.
    static std::string default_path();
};

}  // namespace lcu::platform
