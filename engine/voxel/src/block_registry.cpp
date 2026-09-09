#include "lcu/voxel/block_registry.h"

#include <climits>
#include <utility>

#include "lcu/core/assert.h"

namespace lcu::voxel {

BlockRegistry::BlockRegistry() {
    BlockDefinition air;
    air.namespaced_id = "game:air";
    air.display_name = "Air";
    air.hardness = 0.0f;
    air.is_transparent = true;
    air.has_collision = false;
    air.light_emission = 0;
    register_block(std::move(air));
}

BlockId BlockRegistry::register_block(BlockDefinition definition) {
    LCU_VERIFY(id_lookup_.find(definition.namespaced_id) == id_lookup_.end());
    LCU_VERIFY(definitions_.size() <= static_cast<usize>(USHRT_MAX));

    const BlockId id = static_cast<BlockId>(definitions_.size());
    id_lookup_.emplace(definition.namespaced_id, id);
    definitions_.push_back(std::move(definition));
    return id;
}

const BlockDefinition& BlockRegistry::definition_of(BlockId id) const {
    LCU_ASSERT(id < definitions_.size());
    return definitions_[id];
}

const BlockDefinition* BlockRegistry::find_by_namespaced_id(const std::string& namespaced_id) const {
    const auto it = id_lookup_.find(namespaced_id);
    if (it == id_lookup_.end()) {
        return nullptr;
    }
    return &definitions_[it->second];
}

}  // namespace lcu::voxel
