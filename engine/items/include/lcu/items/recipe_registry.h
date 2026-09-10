#pragma once

#include <vector>

#include "lcu/core/types.h"
#include "lcu/items/item_id.h"
#include "lcu/items/item_stack.h"

namespace lcu::items {

// Matches purely on the multiset of non-empty items present in a
// crafting grid - position doesn't matter. `ingredients` may list the
// same ItemId more than once to require more than one of it (e.g. two
// entries of "game:stick" means "needs exactly 2 sticks somewhere in
// the grid").
struct ShapelessRecipe {
    std::vector<ItemId> ingredients;
    ItemStack result;
};

// Matches a specific width x height arrangement. `pattern` is row-major,
// length width*height, with kNoItemId marking an empty cell. Recipes are
// expected to be authored already trimmed (no fully-empty border row or
// column) - RecipeRegistry doesn't second-guess that; only the *queried*
// grid gets trimmed to its own bounding box before comparison.
//
// Known simplification: matching checks the pattern at its exact stored
// orientation only, not a horizontally-mirrored version (unlike e.g.
// Minecraft's default mirroring). No recipe has needed mirroring yet
// (every planned recipe is symmetric or order-independent) - see
// DECISIONS.md; add it if an asymmetric recipe ever needs it.
struct ShapedRecipe {
    u32 width = 0;
    u32 height = 0;
    std::vector<ItemId> pattern;
    ItemStack result;
};

// Brief section 55's RecipeRegistry: shaped/shapeless crafting matching.
// No crafting-UI caller exists yet (no crafting table/grid interaction
// is wired up anywhere) - this is tested standalone, same as
// BlockRegistry/ItemRegistry were before their first real callers
// existed.
class RecipeRegistry {
   public:
    void add_shapeless(ShapelessRecipe recipe);
    void add_shaped(ShapedRecipe recipe);

    // `grid` is a row-major width*height array of ItemIds (kNoItemId =
    // empty cell). Checks shaped recipes first, then shapeless; returns
    // the first match's result, or nullptr if nothing matches.
    const ItemStack* find_match(const std::vector<ItemId>& grid, u32 width, u32 height) const;

    usize shaped_count() const { return shaped_.size(); }
    usize shapeless_count() const { return shapeless_.size(); }

   private:
    std::vector<ShapedRecipe> shaped_;
    std::vector<ShapelessRecipe> shapeless_;
};

}  // namespace lcu::items
