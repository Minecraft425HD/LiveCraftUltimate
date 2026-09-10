#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/scripting/lua_state.h"

namespace lcu::modding {

// A named pub/sub bus mod scripts subscribe Lua functions to (brief
// section 84's event system). The engine calls one of the emit_*
// methods at a meaningful moment (a block broken, ...); every Lua
// function subscribed to that event name is called, in subscription
// order, with that event's fixed argument list.
//
// Deliberately not a generic C++ <-> Lua argument-marshalling
// framework - only "block_broken" actually fires anywhere in this
// codebase yet, so only it gets a typed emit method. Add another
// emit_<event>() the same way once a second real event exists to
// validate the shape against - see DECISIONS.md.
class EventBus {
   public:
    explicit EventBus(scripting::LuaState& lua);
    ~EventBus();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // Registers `lcu.subscribe(event_name, fn)` in the wrapped
    // LuaState so mod scripts can call it directly.
    void expose_to_lua();

    // Calls every Lua function subscribed to "block_broken" with
    // (world_x, world_y, world_z, block_id) as arguments. A handler
    // that errors is logged and skipped - it doesn't stop the
    // remaining subscribers from running.
    void emit_block_broken(i64 world_x, i64 world_y, i64 world_z, u16 block_id);

    usize subscriber_count(const std::string& event_name) const;

    // Called by the native `lcu_subscribe` Lua binding (see .cpp) -
    // public so that free function can reach it via the EventBus*
    // passed as its upvalue; not meant for other C++ callers, which
    // should use expose_to_lua() + a mod script's own lcu.subscribe(),
    // matching how a real mod would do it.
    void subscribe_from_lua(const std::string& event_name, int lua_function_ref);

   private:
    scripting::LuaState& lua_;
    // event name -> luaL_ref handles (LUA_REGISTRYINDEX references to
    // each subscribed Lua function).
    std::unordered_map<std::string, std::vector<int>> subscribers_;
};

}  // namespace lcu::modding
