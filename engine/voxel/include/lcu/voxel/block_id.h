#pragma once

#include "lcu/core/types.h"

namespace lcu::voxel {

// 16-bit block type id: an index into BlockRegistry, never a pointer or a
// per-block C++ instance (brief section 15-16). 65536 possible block
// types is generous headroom for base game + mods; revisit in
// DECISIONS.md if that ever needs to grow.
using BlockId = u16;

// BlockId 0 always means air. BlockRegistry registers it automatically as
// the first entry so this constant and the registry can never disagree.
constexpr BlockId kAirBlockId = 0;

}  // namespace lcu::voxel
