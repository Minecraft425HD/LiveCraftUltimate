#include "lcu/ecs/registry.h"

namespace lcu::ecs {

// Slot 0 starts (and stays, until something explicitly recycles it -
// nothing does) permanently dead, so EntityId{0, 0} == kInvalidEntityId
// never collides with a real, live entity.
Registry::Registry() { slots_.push_back(Slot{0, false}); }

EntityId Registry::create_entity() {
    if (!free_indices_.empty()) {
        const u32 index = free_indices_.back();
        free_indices_.pop_back();
        slots_[index].alive = true;
        ++alive_count_;
        return EntityId{index, slots_[index].generation};
    }

    slots_.push_back(Slot{0, true});
    ++alive_count_;
    return EntityId{static_cast<u32>(slots_.size() - 1), 0};
}

void Registry::destroy_entity(EntityId id) {
    LCU_ASSERT(is_alive(id));

    for (auto& [type, pool] : pools_) {
        pool->remove(id);
    }

    slots_[id.index].alive = false;
    ++slots_[id.index].generation;
    free_indices_.push_back(id.index);
    --alive_count_;
}

}  // namespace lcu::ecs
