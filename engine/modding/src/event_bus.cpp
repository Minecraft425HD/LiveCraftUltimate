#include "lcu/modding/event_bus.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "lcu/core/log.h"

namespace lcu::modding {

namespace {

// lcu_subscribe(event_name, fn) - the native binding behind Lua's
// lcu.subscribe(). Bound via LuaState::register_function with the owning
// EventBus* as the upvalue (see LuaState::register_function's doc comment
// on why a plain lua_CFunction needs this instead of a capturing lambda).
int lcu_subscribe_binding(lua_State* state) {
    auto* bus = static_cast<EventBus*>(lua_touserdata(state, lua_upvalueindex(1)));
    const char* event_name = luaL_checkstring(state, 1);
    luaL_checktype(state, 2, LUA_TFUNCTION);

    lua_pushvalue(state, 2);
    int function_ref = luaL_ref(state, LUA_REGISTRYINDEX);

    bus->subscribe_from_lua(event_name, function_ref);
    return 0;
}

}  // namespace

EventBus::EventBus(scripting::LuaState& lua) : lua_(lua) {}

EventBus::~EventBus() = default;

void EventBus::expose_to_lua() {
    lua_.register_function("lcu_subscribe", lcu_subscribe_binding, this);
    // Ergonomic table-field wrapper so mod scripts write lcu.subscribe(...)
    // rather than the global lcu_subscribe(...) the binding actually
    // registers.
    lua_.run_string("lcu = lcu or {}\nlcu.subscribe = lcu_subscribe");
}

void EventBus::subscribe_from_lua(const std::string& event_name, int lua_function_ref) {
    subscribers_[event_name].push_back(lua_function_ref);
}

usize EventBus::subscriber_count(const std::string& event_name) const {
    auto it = subscribers_.find(event_name);
    return it == subscribers_.end() ? 0 : it->second.size();
}

void EventBus::emit_block_broken(i64 world_x, i64 world_y, i64 world_z, u16 block_id) {
    auto it = subscribers_.find("block_broken");
    if (it == subscribers_.end()) {
        return;
    }

    lua_State* state = lua_.raw();
    for (int function_ref : it->second) {
        lua_rawgeti(state, LUA_REGISTRYINDEX, function_ref);
        lua_pushinteger(state, static_cast<lua_Integer>(world_x));
        lua_pushinteger(state, static_cast<lua_Integer>(world_y));
        lua_pushinteger(state, static_cast<lua_Integer>(world_z));
        lua_pushinteger(state, static_cast<lua_Integer>(block_id));
        if (lua_pcall(state, 4, 0, 0) != LUA_OK) {
            const char* message = lua_tostring(state, -1);
            LCU_LOG_ERROR("Lua error (block_broken handler): {}", message != nullptr ? message : "<no message>");
            lua_pop(state, 1);
        }
    }
}

}  // namespace lcu::modding
