#include "lcu/modding/registry_bindings.h"

#include <gtest/gtest.h>

extern "C" {
#include <lua.h>
}

#include "lcu/items/item_registry.h"
#include "lcu/scripting/lua_state.h"
#include "lcu/voxel/block_registry.h"

namespace lcu::modding {
namespace {

TEST(RegistryBindings, RegisterBlockAddsDefinitionToRegistry) {
    scripting::LuaState lua;
    voxel::BlockRegistry registry;
    bind_block_registry(lua, registry);

    ASSERT_TRUE(lua.run_string(R"(
        id = register_block("example_mod:magic_stone", "Magic Stone", true, false)
    )"));

    const voxel::BlockDefinition* definition = registry.find_by_namespaced_id("example_mod:magic_stone");
    ASSERT_NE(definition, nullptr);
    EXPECT_EQ(definition->display_name, "Magic Stone");
    EXPECT_TRUE(definition->is_transparent);
    EXPECT_FALSE(definition->has_collision);
}

TEST(RegistryBindings, RegisterBlockOmittedBooleansUseDefaults) {
    scripting::LuaState lua;
    voxel::BlockRegistry registry;
    bind_block_registry(lua, registry);

    ASSERT_TRUE(lua.run_string(R"(register_block("example_mod:plain_block", "Plain Block"))"));

    const voxel::BlockDefinition* definition = registry.find_by_namespaced_id("example_mod:plain_block");
    ASSERT_NE(definition, nullptr);
    EXPECT_FALSE(definition->is_transparent);
    EXPECT_TRUE(definition->has_collision);
}

TEST(RegistryBindings, RegisterBlockReturnsUsableBlockId) {
    scripting::LuaState lua;
    voxel::BlockRegistry registry;
    bind_block_registry(lua, registry);

    ASSERT_TRUE(lua.run_string(R"(id = register_block("example_mod:tagged", "Tagged"))"));

    lua_getglobal(lua.raw(), "id");
    auto returned_id = static_cast<voxel::BlockId>(lua_tointeger(lua.raw(), -1));
    lua_pop(lua.raw(), 1);

    EXPECT_EQ(registry.definition_of(returned_id).namespaced_id, "example_mod:tagged");
}

TEST(RegistryBindings, RegisterItemAddsDefinitionToRegistry) {
    scripting::LuaState lua;
    items::ItemRegistry registry;
    bind_item_registry(lua, registry);

    ASSERT_TRUE(lua.run_string(R"(
        id = register_item("example_mod:magic_wand", "Magic Wand", 1)
    )"));

    const items::ItemDefinition* definition = registry.find_by_namespaced_id("example_mod:magic_wand");
    ASSERT_NE(definition, nullptr);
    EXPECT_EQ(definition->display_name, "Magic Wand");
    EXPECT_EQ(definition->max_stack_size, 1u);
}

TEST(RegistryBindings, RegisterItemOmittedStackSizeUsesDefault) {
    scripting::LuaState lua;
    items::ItemRegistry registry;
    bind_item_registry(lua, registry);

    ASSERT_TRUE(lua.run_string(R"(register_item("example_mod:plain_item", "Plain Item"))"));

    const items::ItemDefinition* definition = registry.find_by_namespaced_id("example_mod:plain_item");
    ASSERT_NE(definition, nullptr);
    EXPECT_EQ(definition->max_stack_size, 64u);
}

TEST(RegistryBindings, BothRegistriesCanBeBoundToTheSameLuaState) {
    scripting::LuaState lua;
    voxel::BlockRegistry block_registry;
    items::ItemRegistry item_registry;
    bind_block_registry(lua, block_registry);
    bind_item_registry(lua, item_registry);

    EXPECT_TRUE(lua.run_string(R"(
        register_block("example_mod:ore", "Magic Ore")
        register_item("example_mod:ore_chunk", "Ore Chunk")
    )"));
}

}  // namespace
}  // namespace lcu::modding
