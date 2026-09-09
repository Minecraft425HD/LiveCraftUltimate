#include "lcu/network/packet_header.h"

namespace lcu::network {

std::vector<u8> serialize_header(const PacketHeader& header) {
    std::vector<u8> bytes(PacketHeader::kWireSize);
    bytes[0] = static_cast<u8>(header.type);
    // Sequence is big-endian ("network byte order") on the wire, same
    // convention as htons/ntohs at the socket boundary - this codebase
    // has no other multi-byte wire fields yet, so this is the first
    // place that convention needed to be picked explicitly.
    bytes[1] = static_cast<u8>((header.sequence >> 8) & 0xFF);
    bytes[2] = static_cast<u8>(header.sequence & 0xFF);
    bytes[3] = static_cast<u8>(header.channel);
    return bytes;
}

std::optional<PacketHeader> deserialize_header(const u8* data, usize size) {
    if (data == nullptr || size < PacketHeader::kWireSize) {
        return std::nullopt;
    }

    if (data[0] > static_cast<u8>(PacketHeader::Type::Ack)) {
        return std::nullopt;
    }
    if (data[3] > static_cast<u8>(Channel::ReliableOrdered)) {
        return std::nullopt;
    }

    PacketHeader header;
    header.type = static_cast<PacketHeader::Type>(data[0]);
    header.sequence = static_cast<u16>((static_cast<u16>(data[1]) << 8) | static_cast<u16>(data[2]));
    header.channel = static_cast<Channel>(data[3]);
    return header;
}

}  // namespace lcu::network
