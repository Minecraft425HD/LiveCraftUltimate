#pragma once

#include <functional>
#include <optional>

#include "lcu/math/vec3.h"
#include "lcu/voxel/block_id.h"
#include "lcu/voxel/chunk_coord.h"
#include "lcu/world/world.h"

namespace lcu::physics {

struct RaycastHit {
    voxel::ChunkCoord chunk;
    voxel::LocalBlockCoord local;
    voxel::BlockWorldCoord world;
    // Outward normal of the face the ray entered through - a unit
    // vector on exactly one axis (e.g. {-1,0,0}), except in the "origin
    // started inside a solid block" edge case (see raycast()), where no
    // face was ever crossed and this is {0,0,0}.
    math::Vec3 normal;
    f32 distance = 0.0f;
    voxel::BlockId block = voxel::kAirBlockId;
};

// Efficient voxel DDA raycast (brief section 25; Amanatides & Woo
// algorithm) against `world`, from `origin` along `direction` (need not
// be normalized - normalized internally) up to `max_distance` (in
// blocks). `is_solid` decides what counts as a hit for a given
// BlockId - callers typically pass something backed by
// BlockRegistry::definition_of(id).has_collision, but raycast() itself
// has no BlockRegistry dependency, so it stays usable for anything that
// needs "first block matching a predicate" (line-of-sight checks,
// different solidity rules, etc.), not just player block-breaking.
//
// Steps through world-block-space one voxel boundary at a time (O(1)
// work per voxel crossed, not O(voxel volume) - the actual "efficient"
// part), independent of chunk size or how far apart chunks are; chunks
// that aren't currently loaded in `world` are treated as passed-through
// (not solid), so a raycast never blocks on chunk generation.
//
// Returns std::nullopt if no matching block is found within
// max_distance, or `direction` is the zero vector.
std::optional<RaycastHit> raycast(const world::World& world, math::Vec3 origin, math::Vec3 direction,
                                   f32 max_distance, const std::function<bool(voxel::BlockId)>& is_solid);

}  // namespace lcu::physics
