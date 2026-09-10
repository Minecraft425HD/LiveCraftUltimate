#include "lcu/items/item_registry.h"

#include <gtest/gtest.h>

using lcu::items::ItemDefinition;
using lcu::items::ItemRegistry;
using lcu::items::kNoItemId;

TEST(ItemRegistry, RegistersNoneAsIdZero) {
    ItemRegistry registry;
    EXPECT_EQ(registry.count(), 1u);

    const ItemDefinition& none = registry.definition_of(kNoItemId);
    EXPECT_EQ(none.namespaced_id, "lcu:none");
    EXPECT_EQ(none.max_stack_size, 0u);
}

TEST(ItemRegistry, RegisterItemReturnsSequentialIds) {
    ItemRegistry registry;

    ItemDefinition stone;
    stone.namespaced_id = "game:stone";
    stone.display_name = "Stone";
    stone.max_stack_size = 64;
    const auto stone_id = registry.register_item(stone);

    ItemDefinition stick;
    stick.namespaced_id = "game:stick";
    stick.display_name = "Stick";
    const auto stick_id = registry.register_item(stick);

    EXPECT_EQ(stone_id, 1);
    EXPECT_EQ(stick_id, 2);
    EXPECT_EQ(registry.count(), 3u);

    EXPECT_EQ(registry.definition_of(stone_id).display_name, "Stone");
    EXPECT_EQ(registry.definition_of(stone_id).max_stack_size, 64u);
}

TEST(ItemRegistry, FindByNamespacedId) {
    ItemRegistry registry;
    ItemDefinition modded;
    modded.namespaced_id = "example_mod:magic_wand";
    modded.display_name = "Magic Wand";
    modded.max_stack_size = 1;
    const auto id = registry.register_item(modded);

    const ItemDefinition* found = registry.find_by_namespaced_id("example_mod:magic_wand");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->display_name, "Magic Wand");
    EXPECT_EQ(registry.definition_of(id).namespaced_id, "example_mod:magic_wand");
}

TEST(ItemRegistry, FindByNamespacedIdReturnsNullForUnknown) {
    ItemRegistry registry;
    EXPECT_EQ(registry.find_by_namespaced_id("game:does_not_exist"), nullptr);
}

TEST(ItemRegistry, DuplicateNamespacedIdAsserts) {
    ItemRegistry registry;
    ItemDefinition stone;
    stone.namespaced_id = "game:stone";
    registry.register_item(stone);

    ItemDefinition duplicate;
    duplicate.namespaced_id = "game:stone";
    EXPECT_DEATH(registry.register_item(duplicate), "");
}
