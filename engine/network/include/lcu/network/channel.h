#pragma once

#include "lcu/core/types.h"

namespace lcu::network {

// The four channel semantics ARCHITECTURE.md commits to (brief section
// 63). Each is a genuinely different delivery guarantee, not just a
// naming variant:
//  - UnreliableUnordered: fire and forget. May be lost; may arrive out
//    of send order; duplicates (from a retransmitting router, say)
//    aren't suppressed. Cheapest, for high-frequency data where a lost
//    or stale update doesn't matter (the next one is coming soon
//    anyway).
//  - UnreliableSequenced: like UnreliableUnordered, but the receiver
//    discards any packet older than the newest one already seen on this
//    channel - useful for state where only the latest value matters
//    (e.g. a position update: an out-of-order-arriving older position
//    should never overwrite a newer one already applied).
//  - ReliableUnordered: guaranteed delivery (retransmitted until
//    acknowledged) but handed to the application as soon as it arrives,
//    not held back to preserve send order.
//  - ReliableOrdered: guaranteed delivery AND guaranteed in-order
//    delivery to the application - a later-arriving packet is buffered
//    until every earlier one has been delivered first.
enum class Channel : u8 {
    UnreliableUnordered = 0,
    UnreliableSequenced = 1,
    ReliableUnordered = 2,
    ReliableOrdered = 3,
};

constexpr bool is_reliable(Channel channel) {
    return channel == Channel::ReliableUnordered || channel == Channel::ReliableOrdered;
}

}  // namespace lcu::network
