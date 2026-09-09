#include "lcu/items/recipe_registry.h"

#include <algorithm>
#include <map>
#include <utility>

#include "lcu/core/assert.h"

namespace lcu::items {

namespace {

struct TrimmedGrid {
    std::vector<ItemId> cells;
    u32 width = 0;
    u32 height = 0;
};

// Shrinks `grid` (width x height, row-major) down to the smallest
// rectangle containing every non-kNoItemId cell. Returns a
// zero-sized TrimmedGrid if `grid` is entirely empty.
TrimmedGrid trim_to_bounding_box(const std::vector<ItemId>& grid, u32 width, u32 height) {
    u32 min_x = width;
    u32 max_x = 0;
    u32 min_y = height;
    u32 max_y = 0;
    bool any = false;

    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            if (grid[y * width + x] != kNoItemId) {
                any = true;
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }
        }
    }

    if (!any) {
        return {};
    }

    TrimmedGrid result;
    result.width = max_x - min_x + 1;
    result.height = max_y - min_y + 1;
    result.cells.resize(static_cast<usize>(result.width) * result.height);
    for (u32 y = 0; y < result.height; ++y) {
        for (u32 x = 0; x < result.width; ++x) {
            result.cells[y * result.width + x] = grid[(min_y + y) * width + (min_x + x)];
        }
    }
    return result;
}

bool matches_shaped(const ShapedRecipe& recipe, const TrimmedGrid& trimmed) {
    return recipe.width == trimmed.width && recipe.height == trimmed.height && recipe.pattern == trimmed.cells;
}

bool matches_shapeless(const ShapelessRecipe& recipe, const std::vector<ItemId>& grid) {
    std::map<ItemId, u32> grid_counts;
    for (ItemId id : grid) {
        if (id != kNoItemId) {
            ++grid_counts[id];
        }
    }
    std::map<ItemId, u32> recipe_counts;
    for (ItemId id : recipe.ingredients) {
        ++recipe_counts[id];
    }
    return grid_counts == recipe_counts;
}

}  // namespace

void RecipeRegistry::add_shapeless(ShapelessRecipe recipe) { shapeless_.push_back(std::move(recipe)); }

void RecipeRegistry::add_shaped(ShapedRecipe recipe) {
    LCU_VERIFY(recipe.pattern.size() == static_cast<usize>(recipe.width) * recipe.height);
    shaped_.push_back(std::move(recipe));
}

const ItemStack* RecipeRegistry::find_match(const std::vector<ItemId>& grid, u32 width, u32 height) const {
    LCU_ASSERT(grid.size() == static_cast<usize>(width) * height);

    const TrimmedGrid trimmed = trim_to_bounding_box(grid, width, height);
    if (!trimmed.cells.empty()) {
        for (const ShapedRecipe& recipe : shaped_) {
            if (matches_shaped(recipe, trimmed)) {
                return &recipe.result;
            }
        }
    }

    for (const ShapelessRecipe& recipe : shapeless_) {
        if (matches_shapeless(recipe, grid)) {
            return &recipe.result;
        }
    }

    return nullptr;
}

}  // namespace lcu::items
