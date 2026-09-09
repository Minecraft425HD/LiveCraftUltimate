#pragma once

#include <string>

#include "lcu/core/types.h"
#include "lcu/scripting/lua_state.h"

namespace lcu::modding {

// Enumerates and loads mods from a mods directory (brief section 84). Each
// immediate subdirectory is one mod, keyed by its directory name, with a
// single fixed entry point: `<mods_directory>/<mod_name>/init.lua`.
//
// Deliberately no separate manifest/JSON format yet - that's a real
// feature (dependency ordering, versioning, metadata) nothing in this
// codebase needs today; adding a bare directory-name + init.lua convention
// now doesn't foreclose a manifest later; see DECISIONS.md.
class ModLoader {
   public:
    explicit ModLoader(scripting::LuaState& lua);

    // Enumerates immediate subdirectories of `mods_directory` and runs each
    // one's init.lua in turn. A mod directory without an init.lua, or
    // whose init.lua errors, is logged and skipped - it does not stop the
    // remaining mods from loading. Returns the number of mods successfully
    // loaded (their init.lua ran to completion without erroring).
    //
    // All mods share the single LuaState passed to the constructor - mods
    // are not sandboxed from each other (only from the host filesystem/
    // process, via LuaState's own library sandboxing).
    usize load_all(const std::string& mods_directory);

   private:
    scripting::LuaState& lua_;
};

}  // namespace lcu::modding
