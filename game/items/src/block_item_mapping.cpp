#include "game/items/block_item_mapping.h"

namespace game::items {

void BlockItemMapping::register_pair(lcu::voxel::BlockId block_id, lcu::items::ItemId item_id) {
    const auto previous = block_to_item_.find(block_id);
    if (previous != block_to_item_.end() && previous->second != item_id) {
        // block_id is being reassigned to a different item - drop the old
        // item's reverse entry if it still points back at this block_id,
        // so a stale item doesn't keep "placing" a block it no longer
        // represents. (Doesn't touch it if some other, later register_pair
        // call already claimed that reverse slot for a different block.)
        const auto stale = item_to_block_.find(previous->second);
        if (stale != item_to_block_.end() && stale->second == block_id) {
            item_to_block_.erase(stale);
        }
    }
    block_to_item_[block_id] = item_id;
    item_to_block_[item_id] = block_id;
}

lcu::items::ItemId BlockItemMapping::item_for_block(lcu::voxel::BlockId block_id) const {
    const auto it = block_to_item_.find(block_id);
    return it != block_to_item_.end() ? it->second : lcu::items::kNoItemId;
}

lcu::voxel::BlockId BlockItemMapping::block_for_item(lcu::items::ItemId item_id) const {
    const auto it = item_to_block_.find(item_id);
    return it != item_to_block_.end() ? it->second : lcu::voxel::kAirBlockId;
}

}  // namespace game::items
