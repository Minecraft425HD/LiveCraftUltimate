#include "game/items/block_item_mapping.h"

#include <gtest/gtest.h>

using game::items::BlockItemMapping;
using lcu::items::kNoItemId;
using lcu::voxel::kAirBlockId;

TEST(BlockItemMapping, UnmappedBlockReturnsNoItemId) {
    BlockItemMapping mapping;
    EXPECT_EQ(mapping.item_for_block(7), kNoItemId);
}

TEST(BlockItemMapping, RegisteredPairRoundTrips) {
    BlockItemMapping mapping;
    mapping.register_pair(1, 100);
    mapping.register_pair(2, 200);

    EXPECT_EQ(mapping.item_for_block(1), 100);
    EXPECT_EQ(mapping.item_for_block(2), 200);
    EXPECT_EQ(mapping.item_for_block(3), kNoItemId);
}

TEST(BlockItemMapping, RegisteringSameBlockIdAgainOverwritesThePreviousMapping) {
    BlockItemMapping mapping;
    mapping.register_pair(1, 100);
    mapping.register_pair(1, 999);

    EXPECT_EQ(mapping.item_for_block(1), 999);
}

TEST(BlockItemMapping, MultipleBlocksCanMapToTheSameItem) {
    // Not every block/item relationship is 1:1 - a loot-table-style
    // "many blocks drop the same item" mapping is representable too,
    // even though today's actual content (stone/grass/dirt) happens to
    // be 1:1 for each.
    BlockItemMapping mapping;
    mapping.register_pair(1, 50);
    mapping.register_pair(2, 50);

    EXPECT_EQ(mapping.item_for_block(1), 50);
    EXPECT_EQ(mapping.item_for_block(2), 50);
}

TEST(BlockItemMapping, UnmappedItemReturnsAirBlockId) {
    BlockItemMapping mapping;
    EXPECT_EQ(mapping.block_for_item(999), kAirBlockId);
}

TEST(BlockItemMapping, RegisteredPairRoundTripsInReverseToo) {
    BlockItemMapping mapping;
    mapping.register_pair(1, 100);
    mapping.register_pair(2, 200);

    EXPECT_EQ(mapping.block_for_item(100), 1);
    EXPECT_EQ(mapping.block_for_item(200), 2);
    EXPECT_EQ(mapping.block_for_item(300), kAirBlockId);
}

TEST(BlockItemMapping, RegisteringSameBlockIdAgainOverwritesTheReverseMappingToo) {
    BlockItemMapping mapping;
    mapping.register_pair(1, 100);
    mapping.register_pair(1, 999);

    EXPECT_EQ(mapping.block_for_item(999), 1);
    EXPECT_EQ(mapping.block_for_item(100), kAirBlockId)
        << "the old item 100 no longer maps back to block 1 once it's been reassigned to item 999";
}
