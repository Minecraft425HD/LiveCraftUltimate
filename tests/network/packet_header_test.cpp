#include "lcu/network/packet_header.h"

#include <gtest/gtest.h>

using lcu::network::Channel;
using lcu::network::deserialize_header;
using lcu::network::PacketHeader;
using lcu::network::serialize_header;

TEST(PacketHeader, RoundTripsDataPacket) {
    PacketHeader header;
    header.type = PacketHeader::Type::Data;
    header.sequence = 4242;
    header.channel = Channel::ReliableOrdered;

    const std::vector<lcu::u8> bytes = serialize_header(header);
    EXPECT_EQ(bytes.size(), PacketHeader::kWireSize);

    const auto decoded = deserialize_header(bytes.data(), bytes.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->type, PacketHeader::Type::Data);
    EXPECT_EQ(decoded->sequence, 4242);
    EXPECT_EQ(decoded->channel, Channel::ReliableOrdered);
}

TEST(PacketHeader, RoundTripsAckPacket) {
    PacketHeader header;
    header.type = PacketHeader::Type::Ack;
    header.sequence = 1;
    header.channel = Channel::ReliableUnordered;

    const std::vector<lcu::u8> bytes = serialize_header(header);
    const auto decoded = deserialize_header(bytes.data(), bytes.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->type, PacketHeader::Type::Ack);
    EXPECT_EQ(decoded->channel, Channel::ReliableUnordered);
}

TEST(PacketHeader, SequenceMaxValueRoundTrips) {
    PacketHeader header;
    header.sequence = 65535;
    const std::vector<lcu::u8> bytes = serialize_header(header);
    const auto decoded = deserialize_header(bytes.data(), bytes.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, 65535);
}

TEST(PacketHeader, TooShortBufferFailsToDeserialize) {
    const std::vector<lcu::u8> bytes = {0, 1, 2};  // one byte short
    EXPECT_FALSE(deserialize_header(bytes.data(), bytes.size()).has_value());
}

TEST(PacketHeader, EmptyBufferFailsToDeserialize) {
    EXPECT_FALSE(deserialize_header(nullptr, 0).has_value());
}

TEST(PacketHeader, UnrecognizedTypeFailsToDeserialize) {
    std::vector<lcu::u8> bytes = {99, 0, 0, 0};  // type byte out of range
    EXPECT_FALSE(deserialize_header(bytes.data(), bytes.size()).has_value());
}

TEST(PacketHeader, UnrecognizedChannelFailsToDeserialize) {
    std::vector<lcu::u8> bytes = {0, 0, 0, 200};  // channel byte out of range
    EXPECT_FALSE(deserialize_header(bytes.data(), bytes.size()).has_value());
}

TEST(PacketHeader, ExtraTrailingBytesAreIgnoredByDeserialize) {
    // A payload following the header shouldn't confuse header decoding -
    // the caller (Connection) is responsible for splitting header from
    // payload afterward.
    std::vector<lcu::u8> bytes = {0, 0, 5, 0, 1, 2, 3};
    const auto decoded = deserialize_header(bytes.data(), bytes.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, 5);
}
