#pragma once

#include "lcu/core/types.h"

namespace lcu::network {

// True if `a` is a later sequence number than `b`, correctly handling
// u16 wraparound (after 65535 the next sequence is 0, which is still
// "later"). The standard half-range comparison (the same technique TCP
// uses for its own 32-bit sequence numbers): `a` is newer if it's within
// half the number space "ahead" of `b`, treating the other half as
// having wrapped around and therefore "behind" it. Two sequence numbers
// exactly half the range apart are an inherently ambiguous case for any
// such scheme (could be interpreted either way) - not resolved
// differently here than the reference technique resolves it, since nothing
// in this codebase's actual traffic volume gets remotely close to 32768
// packets in flight at once for it to matter.
constexpr bool sequence_greater_than(u16 a, u16 b) {
    constexpr u16 kHalfRange = 32768;
    return ((a > b) && (static_cast<u16>(a - b) <= kHalfRange)) ||
           ((a < b) && (static_cast<u16>(b - a) > kHalfRange));
}

}  // namespace lcu::network
