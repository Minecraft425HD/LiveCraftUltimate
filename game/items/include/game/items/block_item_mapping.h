#pragma once

#include <unordered_map>

#include "lcu/items/item_id.h"
#include "lcu/voxel/block_id.h"

namespace game::items {

// Data-driven block<->item association (Phase 22), replacing the
// hardcoded if/else chains VoxelClient's grant_item_for_broken_block and
// VoxelServer's item_for_block both carried since Phase 17-19 (a new
// block/item pair meant a new `if (id == X)` branch in two separate
// places that had to be kept in sync by hand). Registering a pair here
// once, right after registering the block and item themselves, is now
// the only step needed - both client and server populate the same table
// from the same content-registration code, and the lookup itself never
// changes shape as content grows. Lives in game/items (not
// engine/items) because it's gameplay content wiring a block registry
// to an item registry, not a generic engine primitive - the same
// GAME -> ENGINE layering game/systems already follows (see
// ARCHITECTURE.md).
class BlockItemMapping {
   public:
    // Associates block_id with item_id. Overwrites any previous mapping
    // for the same block_id (last registration wins) - callers are
    // expected to register each block id once, same discipline
    // BlockRegistry/ItemRegistry themselves expect of namespaced ids.
    void register_pair(lcu::voxel::BlockId block_id, lcu::items::ItemId item_id);

    // Returns the item mapped to block_id, or lcu::items::kNoItemId if
    // block_id has no mapping (not every block is item-backed - a block
    // with no registered pair simply isn't gated/granted on break/place,
    // same behavior the old hardcoded chains had for anything past
    // their last explicit check).
    lcu::items::ItemId item_for_block(lcu::voxel::BlockId block_id) const;

    // The reverse direction (Phase 49, real inventory-driven hotbar
    // placement): which block a held item places, or
    // lcu::voxel::kAirBlockId if item_id isn't a real placeable item
    // (kAirBlockId, not some other sentinel, so a caller that
    // mistakenly tries to place it ends up placing "nothing" rather
    // than needing a second special case - see client/main.cpp).
    // register_pair keeps both directions in sync from one call, so
    // this can never drift from item_for_block for today's real 1:1
    // content - a real, documented limit for the many-to-one case
    // item_for_block itself already supports (several blocks dropping
    // the same item): the reverse lookup can only ever remember one
    // block per item (last-registered-for-that-item wins), since
    // placing needs a single real answer, not a set. No content
    // registered so far actually needs a many-to-one item, so this
    // hasn't mattered in practice.
    lcu::voxel::BlockId block_for_item(lcu::items::ItemId item_id) const;

   private:
    std::unordered_map<lcu::voxel::BlockId, lcu::items::ItemId> block_to_item_;
    std::unordered_map<lcu::items::ItemId, lcu::voxel::BlockId> item_to_block_;
};

}  // namespace game::items
