-- example_mod: a real, working demonstration mod (Phase 9, extended
-- Phase 23) exercising every binding engine/modding exposes:
-- register_block, register_item, lcu.subscribe("block_broken", ...),
-- and lcu.subscribe("item_crafted", ...).
--
-- Entry point convention: a mod is a directory under mods/ whose init.lua
-- is run once, at startup, sharing one sandboxed Lua VM with every other
-- loaded mod (see ModLoader).

local magic_stone_id = register_block("example_mod:magic_stone", "Magic Stone", false, true)
print(string.format("[example_mod] registered block 'example_mod:magic_stone' -> id %d", magic_stone_id))

local magic_wand_id = register_item("example_mod:magic_wand", "Magic Wand", 1)
print(string.format("[example_mod] registered item 'example_mod:magic_wand' -> id %d", magic_wand_id))

local blocks_broken_seen = 0

lcu.subscribe("block_broken", function(world_x, world_y, world_z, block_id)
    blocks_broken_seen = blocks_broken_seen + 1
    print(string.format(
        "[example_mod] block_broken #%d: block id %d broken at (%d, %d, %d)",
        blocks_broken_seen, block_id, world_x, world_y, world_z
    ))
end)

-- item_crafted (Phase 23): fires only on VoxelClient (crafting is
-- purely client-side, see server/main.cpp) - this mod loads on both
-- hosts identically either way, it just never sees this event fire on
-- the server.
local items_crafted_seen = 0

lcu.subscribe("item_crafted", function(item_id, count)
    items_crafted_seen = items_crafted_seen + 1
    print(string.format(
        "[example_mod] item_crafted #%d: %d x item id %d",
        items_crafted_seen, count, item_id
    ))
end)

print("[example_mod] loaded")
