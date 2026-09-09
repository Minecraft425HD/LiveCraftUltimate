#pragma once

#include "lcu/core/types.h"

namespace lcu::items {

// 16-bit item type id: an index into ItemRegistry, mirroring
// lcu::voxel::BlockId's design (never a pointer or a per-item C++
// instance).
using ItemId = u16;

// ItemId 0 always means "no item" (an empty inventory slot's item field,
// or a recipe pattern cell with nothing in it) - ItemRegistry registers
// it automatically, same convention as BlockRegistry's kAirBlockId, so
// the constant and the registry can never disagree, and a
// zero-initialized ItemStack is always a valid empty slot.
constexpr ItemId kNoItemId = 0;

}  // namespace lcu::items
