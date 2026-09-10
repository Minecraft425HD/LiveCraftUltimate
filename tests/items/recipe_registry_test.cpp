#include "lcu/items/recipe_registry.h"

#include <gtest/gtest.h>

using lcu::items::ItemStack;
using lcu::items::kNoItemId;
using lcu::items::RecipeRegistry;
using lcu::items::ShapedRecipe;
using lcu::items::ShapelessRecipe;

namespace {
// Arbitrary item ids for test purposes - these tests don't need a real
// ItemRegistry, just distinct ItemId values.
constexpr lcu::items::ItemId kStick = 1;
constexpr lcu::items::ItemId kPlank = 2;
constexpr lcu::items::ItemId kStone = 3;
constexpr lcu::items::ItemId kTorch = 100;
constexpr lcu::items::ItemId kPickaxe = 200;
}  // namespace

TEST(RecipeRegistry, ShapelessMatchesRegardlessOfSlotPosition) {
    RecipeRegistry registry;
    registry.add_shapeless(ShapelessRecipe{{kStick, kStone}, ItemStack{kTorch, 4}});

    // 3x3 grid, ingredients scattered in different slots than another
    // valid arrangement would use.
    const std::vector<lcu::items::ItemId> grid = {
        kNoItemId, kStone,    kNoItemId,  //
        kNoItemId, kNoItemId, kStick,     //
        kNoItemId, kNoItemId, kNoItemId,
    };

    const ItemStack* result = registry.find_match(grid, 3, 3);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->item, kTorch);
    EXPECT_EQ(result->count, 4u);
}

TEST(RecipeRegistry, ShapelessRequiresExactIngredientMultiset) {
    RecipeRegistry registry;
    // Needs exactly 2 sticks.
    registry.add_shapeless(ShapelessRecipe{{kStick, kStick}, ItemStack{kTorch, 4}});

    const std::vector<lcu::items::ItemId> one_stick = {kStick, kNoItemId, kNoItemId, kNoItemId};
    EXPECT_EQ(registry.find_match(one_stick, 2, 2), nullptr);

    const std::vector<lcu::items::ItemId> two_sticks_plus_extra = {kStick, kStick, kStone, kNoItemId};
    EXPECT_EQ(registry.find_match(two_sticks_plus_extra, 2, 2), nullptr);

    const std::vector<lcu::items::ItemId> exactly_two_sticks = {kStick, kStick, kNoItemId, kNoItemId};
    EXPECT_NE(registry.find_match(exactly_two_sticks, 2, 2), nullptr);
}

TEST(RecipeRegistry, ShapedMatchesAnywhereInALargerGridViaBoundingBoxTrim) {
    RecipeRegistry registry;
    // A vertical 1x2 pickaxe handle: stick over stick.
    ShapedRecipe recipe;
    recipe.width = 1;
    recipe.height = 2;
    recipe.pattern = {kStick, kStick};
    recipe.result = ItemStack{kPickaxe, 1};
    registry.add_shaped(recipe);

    // Same shape, but positioned off-center in a 3x3 grid.
    const std::vector<lcu::items::ItemId> grid = {
        kNoItemId, kNoItemId, kStick,     //
        kNoItemId, kNoItemId, kStick,     //
        kNoItemId, kNoItemId, kNoItemId,
    };

    const ItemStack* result = registry.find_match(grid, 3, 3);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->item, kPickaxe);
}

TEST(RecipeRegistry, ShapedRequiresMatchingBoundingBoxDimensions) {
    RecipeRegistry registry;
    ShapedRecipe recipe;
    recipe.width = 1;
    recipe.height = 2;
    recipe.pattern = {kStick, kStick};
    recipe.result = ItemStack{kPickaxe, 1};
    registry.add_shaped(recipe);

    // A 2x1 (horizontal) arrangement of the same two sticks doesn't
    // match a 1x2 (vertical) recipe.
    const std::vector<lcu::items::ItemId> horizontal = {kStick, kStick};
    EXPECT_EQ(registry.find_match(horizontal, 2, 1), nullptr);
}

TEST(RecipeRegistry, ShapedRequiresExactCellContents) {
    RecipeRegistry registry;
    ShapedRecipe recipe;
    recipe.width = 2;
    recipe.height = 1;
    recipe.pattern = {kPlank, kStick};
    recipe.result = ItemStack{kPickaxe, 1};
    registry.add_shaped(recipe);

    const std::vector<lcu::items::ItemId> swapped = {kStick, kPlank};
    EXPECT_EQ(registry.find_match(swapped, 2, 1), nullptr);

    const std::vector<lcu::items::ItemId> correct = {kPlank, kStick};
    EXPECT_NE(registry.find_match(correct, 2, 1), nullptr);
}

TEST(RecipeRegistry, EmptyGridMatchesNoShapedRecipe) {
    RecipeRegistry registry;
    ShapedRecipe recipe;
    recipe.width = 1;
    recipe.height = 1;
    recipe.pattern = {kStick};
    recipe.result = ItemStack{kTorch, 1};
    registry.add_shaped(recipe);

    const std::vector<lcu::items::ItemId> empty_grid = {kNoItemId, kNoItemId, kNoItemId, kNoItemId};
    EXPECT_EQ(registry.find_match(empty_grid, 2, 2), nullptr);
}

TEST(RecipeRegistry, NoMatchReturnsNullptr) {
    RecipeRegistry registry;
    registry.add_shapeless(ShapelessRecipe{{kStick}, ItemStack{kTorch, 1}});

    const std::vector<lcu::items::ItemId> grid = {kStone, kStone};
    EXPECT_EQ(registry.find_match(grid, 2, 1), nullptr);
}

TEST(RecipeRegistry, ShapedCheckedBeforeShapelessWhenBothCouldMatch) {
    RecipeRegistry registry;
    ShapedRecipe shaped;
    shaped.width = 1;
    shaped.height = 1;
    shaped.pattern = {kStick};
    shaped.result = ItemStack{kTorch, 1};
    registry.add_shaped(shaped);
    registry.add_shapeless(ShapelessRecipe{{kStick}, ItemStack{kPickaxe, 1}});

    const std::vector<lcu::items::ItemId> grid = {kStick};
    const ItemStack* result = registry.find_match(grid, 1, 1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->item, kTorch);
}

TEST(RecipeRegistry, CountsReflectRegisteredRecipes) {
    RecipeRegistry registry;
    EXPECT_EQ(registry.shaped_count(), 0u);
    EXPECT_EQ(registry.shapeless_count(), 0u);

    registry.add_shapeless(ShapelessRecipe{{kStick}, ItemStack{kTorch, 1}});
    ShapedRecipe recipe;
    recipe.width = 1;
    recipe.height = 1;
    recipe.pattern = {kStone};
    recipe.result = ItemStack{kPickaxe, 1};
    registry.add_shaped(recipe);

    EXPECT_EQ(registry.shaped_count(), 1u);
    EXPECT_EQ(registry.shapeless_count(), 1u);
}
