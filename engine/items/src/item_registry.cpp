#include "lcu/items/item_registry.h"

#include <climits>
#include <utility>

#include "lcu/core/assert.h"

namespace lcu::items {

ItemRegistry::ItemRegistry() {
    ItemDefinition none;
    none.namespaced_id = "lcu:none";
    none.display_name = "None";
    none.max_stack_size = 0;
    register_item(std::move(none));
}

ItemId ItemRegistry::register_item(ItemDefinition definition) {
    LCU_VERIFY(id_lookup_.find(definition.namespaced_id) == id_lookup_.end());
    LCU_VERIFY(definitions_.size() <= static_cast<usize>(USHRT_MAX));

    const ItemId id = static_cast<ItemId>(definitions_.size());
    id_lookup_.emplace(definition.namespaced_id, id);
    definitions_.push_back(std::move(definition));
    return id;
}

const ItemDefinition& ItemRegistry::definition_of(ItemId id) const {
    LCU_ASSERT(id < definitions_.size());
    return definitions_[id];
}

const ItemDefinition* ItemRegistry::find_by_namespaced_id(const std::string& namespaced_id) const {
    const auto it = id_lookup_.find(namespaced_id);
    if (it == id_lookup_.end()) {
        return nullptr;
    }
    return &definitions_[it->second];
}

}  // namespace lcu::items
