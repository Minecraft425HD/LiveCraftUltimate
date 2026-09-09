#pragma once

#include <functional>

#include "lcu/core/types.h"

namespace lcu::ecs {

// A handle to an entity: a slot index plus a generation counter. The
// generation increments every time a slot is recycled (see
// Registry::destroy_entity), so a stale EntityId held past its entity's
// destruction compares unequal to whatever new entity later reuses that
// slot - the same stale-handle-detection idea as most handle-based
// systems in this codebase (e.g. lcu::jobs::JobHandle), just with an
// explicit generation field instead of a monotonically increasing
// counter, since slots need to be reused (entities churn far more than
// jobs do).
struct EntityId {
    u32 index = 0;
    u32 generation = 0;

    constexpr bool operator==(const EntityId& rhs) const { return index == rhs.index && generation == rhs.generation; }
    constexpr bool operator!=(const EntityId& rhs) const { return !(*this == rhs); }
};

// Registry never hands this out - index 0 is permanently reserved dead
// (see Registry's constructor) so this is always distinguishable from a
// real entity, the same convention as kAirBlockId/kNoItemId reserving
// id 0 in their respective registries.
constexpr EntityId kInvalidEntityId{0, 0};

}  // namespace lcu::ecs

namespace std {

template <>
struct hash<lcu::ecs::EntityId> {
    std::size_t operator()(const lcu::ecs::EntityId& id) const noexcept {
        return (static_cast<std::size_t>(id.index) << 32) ^ static_cast<std::size_t>(id.generation);
    }
};

}  // namespace std
