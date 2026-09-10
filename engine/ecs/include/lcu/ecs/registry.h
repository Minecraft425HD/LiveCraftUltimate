#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lcu/core/assert.h"
#include "lcu/core/types.h"
#include "lcu/ecs/entity.h"

namespace lcu::ecs {

namespace detail {

// Type-erased base so Registry::destroy_entity can clean up a component
// of every registered type without knowing what those types are.
class IComponentPool {
   public:
    virtual ~IComponentPool() = default;
    virtual void remove(EntityId id) = 0;
};

// Sparse-set component storage for one component type `T`: a dense,
// contiguous `T` array (cache-friendly iteration over every live
// component - the same data-oriented reasoning as
// lcu::voxel::ChunkStorage's flat BlockId array, brief section 13/15)
// plus a sparse index-to-dense lookup keyed by entity slot index.
// Removal is swap-and-pop, so the dense array never develops holes to
// skip over during iteration.
template <typename T>
class ComponentPool : public IComponentPool {
   public:
    T& insert(EntityId id, T component) {
        LCU_ASSERT(!has(id));
        if (id.index >= sparse_.size()) {
            sparse_.resize(static_cast<usize>(id.index) + 1, kInvalidDenseIndex);
        }
        sparse_[id.index] = static_cast<u32>(dense_.size());
        dense_.push_back(std::move(component));
        dense_entities_.push_back(id);
        return dense_.back();
    }

    void remove(EntityId id) override {
        if (!has(id)) {
            return;
        }
        const u32 dense_index = sparse_[id.index];
        const u32 last_index = static_cast<u32>(dense_.size() - 1);
        dense_[dense_index] = std::move(dense_[last_index]);
        dense_entities_[dense_index] = dense_entities_[last_index];
        sparse_[dense_entities_[dense_index].index] = dense_index;
        dense_.pop_back();
        dense_entities_.pop_back();
        sparse_[id.index] = kInvalidDenseIndex;
    }

    bool has(EntityId id) const {
        return id.index < sparse_.size() && sparse_[id.index] != kInvalidDenseIndex &&
               dense_entities_[sparse_[id.index]] == id;
    }

    T* get(EntityId id) { return has(id) ? &dense_[sparse_[id.index]] : nullptr; }
    const T* get(EntityId id) const { return has(id) ? &dense_[sparse_[id.index]] : nullptr; }

    // The point of a sparse set: iterate every live component of this
    // type with no gaps, in whatever order swap-and-pop removal has left
    // them (not insertion order, and not entity-index order).
    std::vector<T>& dense() { return dense_; }
    const std::vector<T>& dense() const { return dense_; }
    const std::vector<EntityId>& dense_entities() const { return dense_entities_; }

   private:
    static constexpr u32 kInvalidDenseIndex = static_cast<u32>(-1);
    std::vector<u32> sparse_;
    std::vector<T> dense_;
    std::vector<EntityId> dense_entities_;
};

}  // namespace detail

// Minimal data-oriented entity/component storage (brief section 59's
// engine/ecs). Deliberately does not include a query/system-scheduling
// DSL, archetypes, or multithreaded system dispatch - nothing in this
// codebase needs more than "create/destroy entities, attach/query
// components, iterate one component type at a time" yet (a handful of
// AI-driven entities, see game/systems). Add more once a real system
// actually needs it (brief section 98).
class Registry : public NonCopyable {
   public:
    Registry();

    EntityId create_entity();

    // Removes every component this entity has (across every registered
    // component type) and recycles its slot - a later create_entity()
    // may reuse `id.index`, but with an incremented generation, so any
    // EntityId still referring to the old generation is safely stale
    // (is_alive returns false for it, get_component returns nullptr).
    void destroy_entity(EntityId id);

    bool is_alive(EntityId id) const {
        return id.index < slots_.size() && slots_[id.index].alive && slots_[id.index].generation == id.generation;
    }

    usize entity_count() const { return alive_count_; }

    template <typename T>
    T& add_component(EntityId id, T component) {
        LCU_ASSERT(is_alive(id));
        return pool_for<T>().insert(id, std::move(component));
    }

    template <typename T>
    void remove_component(EntityId id) {
        pool_for<T>().remove(id);
    }

    template <typename T>
    bool has_component(EntityId id) const {
        const detail::ComponentPool<T>* pool = find_pool<T>();
        return pool != nullptr && pool->has(id);
    }

    template <typename T>
    T* get_component(EntityId id) {
        detail::ComponentPool<T>* pool = find_pool<T>();
        return pool != nullptr ? pool->get(id) : nullptr;
    }

    template <typename T>
    const T* get_component(EntityId id) const {
        const detail::ComponentPool<T>* pool = find_pool<T>();
        return pool != nullptr ? pool->get(id) : nullptr;
    }

    // Direct access to a component type's dense storage, for systems
    // that iterate every entity with `T` (e.g. "every entity with an
    // AIWander component"). Creates the pool (empty) if `T` has never
    // been used on this registry before.
    template <typename T>
    detail::ComponentPool<T>& pool_for() {
        const std::type_index key(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            auto pool = std::make_unique<detail::ComponentPool<T>>();
            detail::ComponentPool<T>& ref = *pool;
            pools_.emplace(key, std::move(pool));
            return ref;
        }
        return static_cast<detail::ComponentPool<T>&>(*it->second);
    }

   private:
    template <typename T>
    detail::ComponentPool<T>* find_pool() const {
        const auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end()) {
            return nullptr;
        }
        return static_cast<detail::ComponentPool<T>*>(it->second.get());
    }

    struct Slot {
        u32 generation = 0;
        bool alive = false;
    };

    std::vector<Slot> slots_;
    std::vector<u32> free_indices_;
    usize alive_count_ = 0;
    std::unordered_map<std::type_index, std::unique_ptr<detail::IComponentPool>> pools_;
};

}  // namespace lcu::ecs
