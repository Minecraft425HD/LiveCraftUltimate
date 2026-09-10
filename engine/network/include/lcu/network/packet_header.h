#pragma once

#include <optional>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/network/channel.h"

namespace lcu::network {

// Every packet on the wire starts with this 4-byte header. UDP already
// delimits message boundaries (one recvfrom() == one sent datagram), so
// unlike a TCP byte stream this needs no length prefix - the payload is
// simply everything after the header in the same datagram.
struct PacketHeader {
    enum class Type : u8 { Data = 0, Ack = 1 };

    Type type = Type::Data;
    // Data: this packet's own sequence number (per-channel counter, see
    // Connection). Ack: the sequence number being acknowledged.
    u16 sequence = 0;
    // Data: which channel this payload was sent on. Ack: which channel
    // the acknowledged packet was sent on - acks need this too, since
    // each channel has its own independent sequence space (see
    // DECISIONS.md), so "sequence 5" is ambiguous without it.
    Channel channel = Channel::UnreliableUnordered;

    static constexpr usize kWireSize = 4;
};

std::vector<u8> serialize_header(const PacketHeader& header);

// Returns std::nullopt if `data` is too short to contain a full header
// or names an unrecognized Type/Channel value (a malformed or
// foreign packet, not necessarily an attack - just discarded).
std::optional<PacketHeader> deserialize_header(const u8* data, usize size);

}  // namespace lcu::network
