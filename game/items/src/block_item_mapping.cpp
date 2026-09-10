#include "game/items/block_item_mapping.h"

namespace game::items {

void BlockItemMapping::register_pair(lcu::voxel::BlockId block_id, lcu::items::ItemId item_id) {
    block_to_item_[block_id] = item_id;
}

lcu::items::ItemId BlockItemMapping::item_for_block(lcu::voxel::BlockId block_id) const {
    const auto it = block_to_item_.find(block_id);
    return it != block_to_item_.end() ? it->second : lcu::items::kNoItemId;
}

}  // namespace game::items
