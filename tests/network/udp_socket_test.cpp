#include "lcu/network/udp_socket.h"

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

using lcu::network::Address;
using lcu::network::UdpSocket;

TEST(UdpSocket, BindSucceedsOnAnEphemeralPort) {
    UdpSocket socket;
    // Port 0 asks the OS to pick a free ephemeral port - avoids any
    // flakiness from a hardcoded port already being in use.
    EXPECT_TRUE(socket.bind(0));
    EXPECT_TRUE(socket.is_open());
}

TEST(UdpSocket, TryReceiveReturnsNulloptWhenNothingIsWaiting) {
    UdpSocket socket;
    ASSERT_TRUE(socket.bind(0));
    Address from;
    EXPECT_FALSE(socket.try_receive(from).has_value());
}

TEST(UdpSocket, SendToUnboundSocketFails) {
    UdpSocket socket;  // never bound
    EXPECT_FALSE(socket.send_to(Address::loopback(12345), {1, 2, 3}));
}

TEST(UdpSocket, LoopbackSendAndReceiveRoundTrips) {
    UdpSocket sender;
    UdpSocket receiver;
    ASSERT_TRUE(sender.bind(0));
    ASSERT_TRUE(receiver.bind(19876));

    const std::vector<lcu::u8> payload = {1, 2, 3, 4, 5};
    ASSERT_TRUE(sender.send_to(Address::loopback(19876), payload));

    // Real network I/O even over loopback isn't instantaneous - poll
    // briefly rather than asserting the datagram is there on the very
    // first try.
    std::optional<std::vector<lcu::u8>> received;
    Address from;
    for (int attempt = 0; attempt < 100 && !received; ++attempt) {
        received = receiver.try_receive(from);
        if (!received) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, payload);
    EXPECT_EQ(from.ip[0], 127);
    EXPECT_EQ(from.ip[3], 1);
}

TEST(UdpSocket, OversizedPayloadIsRejected) {
    UdpSocket socket;
    ASSERT_TRUE(socket.bind(0));
    const std::vector<lcu::u8> too_big(lcu::network::kMaxDatagramSize + 1, 0);
    EXPECT_FALSE(socket.send_to(Address::loopback(19877), too_big));
}

TEST(Address, LoopbackConstructsExpectedOctets) {
    const Address addr = Address::loopback(8080);
    EXPECT_EQ(addr.ip[0], 127);
    EXPECT_EQ(addr.ip[1], 0);
    EXPECT_EQ(addr.ip[2], 0);
    EXPECT_EQ(addr.ip[3], 1);
    EXPECT_EQ(addr.port, 8080);
}

TEST(Address, ToStringFormatsDottedQuad) {
    const Address addr = Address::loopback(25565);
    EXPECT_EQ(addr.to_string(), "127.0.0.1:25565");
}

TEST(Address, EqualityComparesAllFields) {
    EXPECT_EQ(Address::loopback(1), Address::loopback(1));
    EXPECT_NE(Address::loopback(1), Address::loopback(2));
}
