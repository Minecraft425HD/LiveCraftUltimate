#include "lcu/voxel/block_registry.h"

#include <gtest/gtest.h>

using lcu::voxel::BlockDefinition;
using lcu::voxel::BlockRegistry;
using lcu::voxel::kAirBlockId;

TEST(BlockRegistry, RegistersAirAsIdZero) {
    BlockRegistry registry;
    EXPECT_EQ(registry.count(), 1u);

    const BlockDefinition& air = registry.definition_of(kAirBlockId);
    EXPECT_EQ(air.namespaced_id, "game:air");
    EXPECT_FALSE(air.has_collision);
    EXPECT_TRUE(air.is_transparent);
}

TEST(BlockRegistry, RegisterBlockReturnsSequentialIds) {
    BlockRegistry registry;

    BlockDefinition stone;
    stone.namespaced_id = "game:stone";
    stone.display_name = "Stone";
    stone.hardness = 1.5f;
    stone.is_transparent = false;
    const auto stone_id = registry.register_block(stone);

    BlockDefinition dirt;
    dirt.namespaced_id = "game:dirt";
    dirt.display_name = "Dirt";
    const auto dirt_id = registry.register_block(dirt);

    EXPECT_EQ(stone_id, 1);
    EXPECT_EQ(dirt_id, 2);
    EXPECT_EQ(registry.count(), 3u);

    EXPECT_EQ(registry.definition_of(stone_id).display_name, "Stone");
    EXPECT_FLOAT_EQ(registry.definition_of(stone_id).hardness, 1.5f);
}

TEST(BlockRegistry, FindByNamespacedId) {
    BlockRegistry registry;
    BlockDefinition modded;
    modded.namespaced_id = "example_mod:magic_stone";
    modded.display_name = "Magic Stone";
    const auto id = registry.register_block(modded);

    const BlockDefinition* found = registry.find_by_namespaced_id("example_mod:magic_stone");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->display_name, "Magic Stone");

    // Round-trips through the id too.
    EXPECT_EQ(registry.definition_of(id).namespaced_id, "example_mod:magic_stone");
}

TEST(BlockRegistry, FindByNamespacedIdReturnsNullForUnknown) {
    BlockRegistry registry;
    EXPECT_EQ(registry.find_by_namespaced_id("game:does_not_exist"), nullptr);
}

TEST(BlockRegistry, DuplicateNamespacedIdAsserts) {
    BlockRegistry registry;
    BlockDefinition stone;
    stone.namespaced_id = "game:stone";
    registry.register_block(stone);

    BlockDefinition duplicate;
    duplicate.namespaced_id = "game:stone";
    EXPECT_DEATH(registry.register_block(duplicate), "");
}
