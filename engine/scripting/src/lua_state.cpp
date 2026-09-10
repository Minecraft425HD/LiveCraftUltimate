#include "lcu/scripting/lua_state.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "lcu/core/log.h"

namespace lcu::scripting {

namespace {

// Logs and pops the error message luaL_dostring/luaL_dofile leave on
// top of the stack after a non-zero return.
void log_and_pop_error(lua_State* state, const char* context) {
    const char* message = lua_tostring(state, -1);
    LCU_LOG_ERROR("Lua error ({}): {}", context, message != nullptr ? message : "<no message>");
    lua_pop(state, 1);
}

}  // namespace

LuaState::LuaState() {
    state_ = luaL_newstate();
    // Only base/table/string/math - not io/os/package (see the header's
    // doc comment on why: no filesystem/process access for a mod script
    // by default).
    static const luaL_Reg kSandboxedLibraries[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {nullptr, nullptr},
    };
    for (const luaL_Reg* lib = kSandboxedLibraries; lib->func != nullptr; ++lib) {
        luaL_requiref(state_, lib->name, lib->func, 1);
        lua_pop(state_, 1);
    }
}

LuaState::~LuaState() {
    if (state_ != nullptr) {
        lua_close(state_);
    }
}

bool LuaState::run_string(const std::string& code) {
    if (luaL_dostring(state_, code.c_str()) != LUA_OK) {
        log_and_pop_error(state_, "run_string");
        return false;
    }
    return true;
}

bool LuaState::run_file(const std::string& path) {
    if (luaL_dofile(state_, path.c_str()) != LUA_OK) {
        log_and_pop_error(state_, path.c_str());
        return false;
    }
    return true;
}

void LuaState::register_function(const std::string& name, LuaCFunction fn, void* user_data) {
    lua_pushlightuserdata(state_, user_data);
    lua_pushcclosure(state_, fn, 1);
    lua_setglobal(state_, name.c_str());
}

}  // namespace lcu::scripting
