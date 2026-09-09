#include "lcu/network/connection.h"

#include <string>

#include <gtest/gtest.h>

using lcu::network::Channel;
using lcu::network::Connection;
using lcu::network::PacketHeader;
using lcu::network::ReceivedMessage;

namespace {

std::vector<lcu::u8> to_bytes(const std::string& s) { return std::vector<lcu::u8>(s.begin(), s.end()); }

std::string to_string(const std::vector<lcu::u8>& bytes) { return std::string(bytes.begin(), bytes.end()); }

// Delivers every packet `from` has queued for sending into `to`,
// simulating a perfect (lossless) link. Returns whatever application
// messages `to` produced.
std::vector<ReceivedMessage> deliver_all(Connection& from, Connection& to) {
    std::vector<ReceivedMessage> all_received;
    for (const auto& packet : from.take_outgoing_packets()) {
        auto received = to.on_packet_received(packet);
        all_received.insert(all_received.end(), received.begin(), received.end());
    }
    return all_received;
}

}  // namespace

TEST(Connection, UnreliableUnorderedDeliversImmediately) {
    Connection sender;
    Connection receiver;
    sender.send(Channel::UnreliableUnordered, to_bytes("hello"));

    const auto received = deliver_all(sender, receiver);
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].channel, Channel::UnreliableUnordered);
    EXPECT_EQ(to_string(received[0].payload), "hello");
}

TEST(Connection, UnreliableUnorderedGeneratesNoAck) {
    Connection sender;
    Connection receiver;
    sender.send(Channel::UnreliableUnordered, to_bytes("x"));
    deliver_all(sender, receiver);

    // Nothing should flow back from an unreliable channel.
    EXPECT_TRUE(receiver.take_outgoing_packets().empty());
}

TEST(Connection, UnreliableSequencedDropsStalePackets) {
    Connection sender;
    Connection receiver;

    sender.send(Channel::UnreliableSequenced, to_bytes("first"));
    sender.send(Channel::UnreliableSequenced, to_bytes("second"));
    auto packets = sender.take_outgoing_packets();
    ASSERT_EQ(packets.size(), 2u);

    // Deliver "second" first, then the now-stale "first" out of order.
    const auto from_second = receiver.on_packet_received(packets[1]);
    const auto from_first = receiver.on_packet_received(packets[0]);

    ASSERT_EQ(from_second.size(), 1u);
    EXPECT_EQ(to_string(from_second[0].payload), "second");
    EXPECT_TRUE(from_first.empty());  // stale, dropped
}

TEST(Connection, ReliableUnorderedDeliversAsSoonAsItArrives) {
    Connection sender;
    Connection receiver;

    sender.send(Channel::ReliableUnordered, to_bytes("a"));
    sender.send(Channel::ReliableUnordered, to_bytes("b"));
    auto packets = sender.take_outgoing_packets();
    ASSERT_EQ(packets.size(), 2u);

    // Deliver out of send order - ReliableUnordered doesn't care.
    const auto from_b = receiver.on_packet_received(packets[1]);
    const auto from_a = receiver.on_packet_received(packets[0]);
    ASSERT_EQ(from_b.size(), 1u);
    EXPECT_EQ(to_string(from_b[0].payload), "b");
    ASSERT_EQ(from_a.size(), 1u);
    EXPECT_EQ(to_string(from_a[0].payload), "a");
}

TEST(Connection, ReliableUnorderedSuppressesDuplicateDelivery) {
    Connection sender;
    Connection receiver;
    sender.send(Channel::ReliableUnordered, to_bytes("once"));
    auto packets = sender.take_outgoing_packets();
    ASSERT_EQ(packets.size(), 1u);

    const auto first = receiver.on_packet_received(packets[0]);
    const auto duplicate = receiver.on_packet_received(packets[0]);  // simulated retransmit
    EXPECT_EQ(first.size(), 1u);
    EXPECT_TRUE(duplicate.empty());
}

TEST(Connection, ReliableOrderedBuffersOutOfOrderArrivalsThenDrainsInOrder) {
    Connection sender;
    Connection receiver;

    sender.send(Channel::ReliableOrdered, to_bytes("1"));
    sender.send(Channel::ReliableOrdered, to_bytes("2"));
    sender.send(Channel::ReliableOrdered, to_bytes("3"));
    auto packets = sender.take_outgoing_packets();
    ASSERT_EQ(packets.size(), 3u);

    // Arrival order: 3, 1, 2. Application order must still be 1, 2, 3.
    const auto from_3 = receiver.on_packet_received(packets[2]);
    EXPECT_TRUE(from_3.empty());  // buffered, waiting for 1 and 2

    const auto from_1 = receiver.on_packet_received(packets[0]);
    ASSERT_EQ(from_1.size(), 1u);
    EXPECT_EQ(to_string(from_1[0].payload), "1");

    const auto from_2 = receiver.on_packet_received(packets[1]);
    // 2 arriving should immediately drain the buffered 3 as well.
    ASSERT_EQ(from_2.size(), 2u);
    EXPECT_EQ(to_string(from_2[0].payload), "2");
    EXPECT_EQ(to_string(from_2[1].payload), "3");
}

TEST(Connection, ReliableOrderedDropsDuplicateOfAlreadyDeliveredPacket) {
    Connection sender;
    Connection receiver;
    sender.send(Channel::ReliableOrdered, to_bytes("only"));
    auto packets = sender.take_outgoing_packets();

    const auto first = receiver.on_packet_received(packets[0]);
    const auto duplicate = receiver.on_packet_received(packets[0]);
    EXPECT_EQ(first.size(), 1u);
    EXPECT_TRUE(duplicate.empty());
}

TEST(Connection, ReliableChannelsGenerateAnAckThatClearsThePendingPacket) {
    Connection sender;
    Connection receiver;
    sender.send(Channel::ReliableOrdered, to_bytes("data"));
    EXPECT_EQ(sender.pending_reliable_count(), 1u);

    deliver_all(sender, receiver);       // receiver processes it, queues an ack
    deliver_all(receiver, sender);       // sender processes the ack

    EXPECT_EQ(sender.pending_reliable_count(), 0u);
}

TEST(Connection, UnacknowledgedReliablePacketIsRetransmittedAfterTheInterval) {
    Connection sender(0.5f);  // retransmit interval
    sender.send(Channel::ReliableOrdered, to_bytes("resend-me"));
    ASSERT_EQ(sender.take_outgoing_packets().size(), 1u);  // the original send

    sender.update(0.3f);
    EXPECT_TRUE(sender.take_outgoing_packets().empty());  // not yet due

    sender.update(0.3f);  // total 0.6s > 0.5s interval
    EXPECT_EQ(sender.take_outgoing_packets().size(), 1u);  // retransmitted
}

TEST(Connection, AckedPacketIsNotRetransmitted) {
    Connection sender(0.1f);
    Connection receiver;
    sender.send(Channel::ReliableUnordered, to_bytes("once"));
    deliver_all(sender, receiver);  // receiver acks
    deliver_all(receiver, sender);  // sender clears pending

    sender.update(1.0f);  // well past the retransmit interval
    EXPECT_TRUE(sender.take_outgoing_packets().empty());
}

TEST(Connection, IndependentChannelsHaveIndependentSequenceSpaces) {
    // A ReliableOrdered send shouldn't be affected by unrelated
    // UnreliableUnordered traffic sharing the connection - each channel
    // counts its own sequence numbers from 0.
    Connection sender;
    Connection receiver;

    sender.send(Channel::UnreliableUnordered, to_bytes("noise-1"));
    sender.send(Channel::UnreliableUnordered, to_bytes("noise-2"));
    sender.send(Channel::ReliableOrdered, to_bytes("first-ordered"));

    const auto received = deliver_all(sender, receiver);
    // All three should be delivered; the ReliableOrdered one, still at
    // its own sequence 0, isn't waiting on any gap.
    bool found_ordered = false;
    for (const auto& msg : received) {
        if (msg.channel == Channel::ReliableOrdered) {
            found_ordered = true;
            EXPECT_EQ(to_string(msg.payload), "first-ordered");
        }
    }
    EXPECT_TRUE(found_ordered);
}

TEST(Connection, MalformedPacketIsIgnoredWithoutCrashing) {
    Connection receiver;
    const std::vector<lcu::u8> garbage = {1, 2};  // too short to be a valid header
    EXPECT_TRUE(receiver.on_packet_received(garbage).empty());
}
