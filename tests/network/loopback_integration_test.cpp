#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "lcu/network/connection.h"
#include "lcu/network/udp_socket.h"

using lcu::network::Address;
using lcu::network::Channel;
using lcu::network::Connection;
using lcu::network::ReceivedMessage;
using lcu::network::UdpSocket;

namespace {

std::vector<lcu::u8> to_bytes(const std::string& s) { return std::vector<lcu::u8>(s.begin(), s.end()); }
std::string to_string(const std::vector<lcu::u8>& bytes) { return std::string(bytes.begin(), bytes.end()); }

// Sends every packet `connection` has queued out through `socket` to
// `to`. `drop_first_send` deliberately loses the very first packet this
// function is ever called with (simulating one lost datagram) so the
// test can prove retransmission actually recovers from real loss, not
// just a hypothetical one - "sent from a real UdpSocket, blackholed
// before ever reaching the other socket" is a real dropped packet, not
// a mocked one.
void send_outgoing(Connection& connection, UdpSocket& socket, const Address& to, bool& drop_first_send) {
    for (auto& packet : connection.take_outgoing_packets()) {
        if (drop_first_send) {
            drop_first_send = false;
            continue;  // simulated loss: never actually sent
        }
        socket.send_to(to, packet);
    }
}

std::vector<ReceivedMessage> receive_all_available(Connection& connection, UdpSocket& socket) {
    std::vector<ReceivedMessage> received;
    Address from;
    while (auto packet = socket.try_receive(from)) {
        auto messages = connection.on_packet_received(*packet);
        received.insert(received.end(), messages.begin(), messages.end());
    }
    return received;
}

}  // namespace

// Two real UdpSockets on real loopback ports, driving two Connection
// instances end to end - the same code path VoxelServer's real network
// loop uses, just with both peers in one test process. Deliberately
// drops the very first packet each side sends to prove the reliable
// channel's retransmission actually recovers a real dropped datagram,
// not just a hypothetical one a unit test assumes away.
TEST(LoopbackIntegration, ReliableOrderedMessageArrivesDespiteOneDroppedPacket) {
    UdpSocket socket_a;
    UdpSocket socket_b;
    ASSERT_TRUE(socket_a.bind(0));
    ASSERT_TRUE(socket_b.bind(28881));
    const Address address_b = Address::loopback(28881);
    // socket_a's own port is ephemeral (bind(0)) - A only ever needs
    // B's fixed port to send to; B learns A's address from the sender
    // field of A's first arriving datagram, the same way a real server
    // learns a new client's address on first contact.

    Connection connection_a(0.15f);  // short retransmit interval to keep the test fast
    Connection connection_b(0.15f);

    connection_a.send(Channel::ReliableOrdered, to_bytes("mission-critical"));

    bool drop_first_a_to_b = true;
    bool drop_first_b_to_a = false;  // B hasn't sent anything yet to drop

    Address learned_a_address{};
    bool learned_a = false;

    std::vector<ReceivedMessage> b_received;
    constexpr int kMaxTicks = 60;
    constexpr float kDt = 0.05f;

    for (int tick = 0; tick < kMaxTicks; ++tick) {
        connection_a.update(kDt);
        connection_b.update(kDt);

        // A always knows B's fixed port, so it can send regardless of
        // whether B has learned A's address yet.
        send_outgoing(connection_a, socket_a, address_b, drop_first_a_to_b);

        // B receives from whoever sent to it, learning A's address from
        // the datagram itself (the same way a real server learns a
        // client's address on first contact).
        Address from;
        while (auto packet = socket_b.try_receive(from)) {
            if (!learned_a) {
                learned_a_address = from;
                learned_a = true;
            }
            auto messages = connection_b.on_packet_received(*packet);
            b_received.insert(b_received.end(), messages.begin(), messages.end());
        }

        if (learned_a) {
            send_outgoing(connection_b, socket_b, learned_a_address, drop_first_b_to_a);
        }
        receive_all_available(connection_a, socket_a);

        if (!b_received.empty() && connection_a.pending_reliable_count() == 0) {
            break;
        }
    }

    ASSERT_EQ(b_received.size(), 1u);
    EXPECT_EQ(b_received[0].channel, Channel::ReliableOrdered);
    EXPECT_EQ(to_string(b_received[0].payload), "mission-critical");
    // The sender's pending-ack table being empty confirms the ack for
    // the (eventually successful) retransmission made it all the way
    // back too, not just that B received the data.
    EXPECT_EQ(connection_a.pending_reliable_count(), 0u);
}

TEST(LoopbackIntegration, UnreliableMessageRoundTripsOverRealSockets) {
    UdpSocket socket_a;
    UdpSocket socket_b;
    ASSERT_TRUE(socket_a.bind(0));
    ASSERT_TRUE(socket_b.bind(28882));
    const Address address_b = Address::loopback(28882);

    Connection connection_a;
    Connection connection_b;
    connection_a.send(Channel::UnreliableSequenced, to_bytes("position-update"));

    for (auto& packet : connection_a.take_outgoing_packets()) {
        ASSERT_TRUE(socket_a.send_to(address_b, packet));
    }

    std::vector<ReceivedMessage> received;
    Address from;
    for (int attempt = 0; attempt < 100 && received.empty(); ++attempt) {
        if (auto packet = socket_b.try_receive(from)) {
            received = connection_b.on_packet_received(*packet);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(to_string(received[0].payload), "position-update");
}
