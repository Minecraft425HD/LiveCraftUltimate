#pragma once

#include "lcu/items/item_registry.h"
#include "lcu/scripting/lua_state.h"
#include "lcu/voxel/block_registry.h"

namespace lcu::modding {

// Exposes `register_block(namespaced_id, display_name, is_transparent,
// has_collision)` to Lua, writing straight into `registry`. The last two
// arguments are optional in Lua (nil/omitted defaults to false/true
// respectively, matching BlockDefinition's own defaults). Returns the new
// block's numeric BlockId.
//
// `registry` must outlive `lua` - both are normally owned together by
// whatever constructs a client/server's modding stack (see client/main.cpp
// / server/main.cpp).
void bind_block_registry(scripting::LuaState& lua, voxel::BlockRegistry& registry);

// Exposes `register_item(namespaced_id, display_name, max_stack_size)` to
// Lua, mirroring bind_block_registry. `max_stack_size` is optional in Lua
// (nil/omitted defaults to 64, matching ItemDefinition's default). Returns
// the new item's numeric ItemId.
void bind_item_registry(scripting::LuaState& lua, items::ItemRegistry& registry);

}  // namespace lcu::modding
