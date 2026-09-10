#include "lcu/modding/registry_bindings.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace lcu::modding {

namespace {

// Shared by both bindings: an optional boolean argument at `arg_index`
// defaults to `default_value` when the Lua caller passed nil or omitted it
// entirely (deliberately not luaL_opt* - explicit here avoids a
// macro-usage mistake for a two-way boolean default).
bool optional_bool(lua_State* state, int arg_index, bool default_value) {
    if (lua_isnoneornil(state, arg_index)) {
        return default_value;
    }
    return lua_toboolean(state, arg_index) != 0;
}

int lcu_register_block_binding(lua_State* state) {
    auto* registry = static_cast<voxel::BlockRegistry*>(lua_touserdata(state, lua_upvalueindex(1)));

    voxel::BlockDefinition definition;
    definition.namespaced_id = luaL_checkstring(state, 1);
    definition.display_name = luaL_checkstring(state, 2);
    definition.is_transparent = optional_bool(state, 3, false);
    definition.has_collision = optional_bool(state, 4, true);

    voxel::BlockId id = registry->register_block(std::move(definition));
    lua_pushinteger(state, static_cast<lua_Integer>(id));
    return 1;
}

int lcu_register_item_binding(lua_State* state) {
    auto* registry = static_cast<items::ItemRegistry*>(lua_touserdata(state, lua_upvalueindex(1)));

    items::ItemDefinition definition;
    definition.namespaced_id = luaL_checkstring(state, 1);
    definition.display_name = luaL_checkstring(state, 2);
    definition.max_stack_size =
        lua_isnoneornil(state, 3) ? definition.max_stack_size : static_cast<u32>(luaL_checkinteger(state, 3));

    items::ItemId id = registry->register_item(std::move(definition));
    lua_pushinteger(state, static_cast<lua_Integer>(id));
    return 1;
}

}  // namespace

void bind_block_registry(scripting::LuaState& lua, voxel::BlockRegistry& registry) {
    lua.register_function("register_block", lcu_register_block_binding, &registry);
}

void bind_item_registry(scripting::LuaState& lua, items::ItemRegistry& registry) {
    lua.register_function("register_item", lcu_register_item_binding, &registry);
}

}  // namespace lcu::modding
