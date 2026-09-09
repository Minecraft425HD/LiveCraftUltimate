#pragma once

#include <string>

#include "lcu/core/types.h"

// Forward-declared, not included: engine/scripting is the only place
// <lua.h> is actually #included (in the .cpp files), mirroring
// engine/rendering being the only place bgfx headers are included -
// see DECISIONS.md. Callers that need to write a native Lua binding
// function still need the real Lua C API themselves (LuaState::raw()
// hands out the pointer for exactly that), but nothing needs to
// #include <lua.h> merely to hold a LuaState or register one.
struct lua_State;

namespace lcu::scripting {

// Matches Lua's own `lua_CFunction` typedef exactly (`int (*)(lua_State*)`)
// - redeclared here rather than pulled from <lua.h> so this header stays
// Lua-header-free. C is fine with two compatible typedefs to the same
// function pointer type from different translation units.
using LuaCFunction = int (*)(lua_State*);

// RAII wrapper around one Lua 5.4 VM (brief section 84's modding
// scripting host). Opens only the base/table/string/math standard
// libraries - not `io`/`os`/`package` - so a mod script has no direct
// filesystem or process access by default (see DECISIONS.md
// "engine/scripting sandboxes the standard library").
class LuaState : public NonCopyable {
   public:
    LuaState();
    ~LuaState();

    LuaState(LuaState&&) = delete;
    LuaState& operator=(LuaState&&) = delete;

    // Executes Lua source code directly (mainly for tests - a real mod
    // is loaded via run_file). Returns false (logged, with the Lua
    // error message) on a syntax or runtime error.
    bool run_string(const std::string& code);

    // Loads and executes the Lua script at `path`. Returns false
    // (logged) if the file can't be read, or on a syntax/runtime error.
    bool run_file(const std::string& path);

    // Exposes a native function to Lua's global namespace under `name`.
    // `user_data` (may be null) is retrievable from inside `fn` via
    // `lua_touserdata(L, lua_upvalueindex(1))` - the standard technique
    // for binding a stateful C++ object (a registry, an event bus, ...)
    // to a plain Lua C-style callback, which can't capture like a C++
    // lambda.
    void register_function(const std::string& name, LuaCFunction fn, void* user_data);

    // Raw access to the underlying lua_State*, for binding-function
    // implementations (see engine/modding) that need the full Lua C API
    // (lua_tostring, luaL_checkinteger, ...) to read their arguments and
    // push results. Not intended for general application code, which
    // should go through run_file/run_string and the event system
    // instead of poking the VM directly.
    lua_State* raw() { return state_; }

   private:
    lua_State* state_ = nullptr;
};

}  // namespace lcu::scripting
