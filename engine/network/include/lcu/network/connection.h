#pragma once

#include <array>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/network/channel.h"
#include "lcu/network/packet_header.h"

namespace lcu::network {

struct ReceivedMessage {
    Channel channel = Channel::UnreliableUnordered;
    std::vector<u8> payload;
};

// How often an unacknowledged reliable packet is resent. Fixed, not
// RTT-adaptive - see DECISIONS.md; simple and correct beats
// congestion-aware and speculative for a first working transport with
// no real network conditions to tune it against yet.
constexpr f32 kDefaultRetransmitInterval = 0.2f;

// Implements the four Channel semantics (channel.h) over an abstract
// byte-packet transport: Connection never touches a socket itself - it
// only produces raw outgoing packets (take_outgoing_packets()) and
// consumes raw incoming ones (on_packet_received()), the same
// dependency-injection shape as lcu::physics::raycast taking an
// `is_solid` predicate instead of reaching into BlockRegistry itself.
// This keeps the protocol logic testable with zero real I/O (feed it
// bytes directly, inspect what it produces) while still being exactly
// what a real UdpSocket-backed caller uses in production - see
// tests/network/connection_test.cpp for the no-socket-needed tests and
// tests/network/loopback_integration_test.cpp for the real-socket one.
//
// One Connection represents one peer. A UDP-based server holds one
// Connection per connected client since UDP itself has no built-in
// notion of "a connection" (see DECISIONS.md for how VoxelServer
// actually manages a table of these).
class Connection : public NonCopyable {
   public:
    explicit Connection(f32 retransmit_interval = kDefaultRetransmitInterval);

    // Queues `payload` for sending on `channel`. Reliable channels are
    // buffered for retransmission until acknowledged (see update());
    // unreliable channels are sent exactly once. Either way, the raw
    // packet is queued into take_outgoing_packets() immediately.
    void send(Channel channel, std::vector<u8> payload);

    // Advances retransmission timers by `dt` seconds; any reliable
    // packet still unacknowledged past the retransmit interval is
    // re-queued into take_outgoing_packets().
    void update(f32 dt);

    // Drains and returns every raw packet queued for transmission since
    // the last call. The caller (a real UdpSocket in production, a test
    // harness in tests) is responsible for actually putting these on
    // the wire to this Connection's peer.
    std::vector<std::vector<u8>> take_outgoing_packets();

    // Feeds one raw packet received from this Connection's peer into the
    // protocol state machine. Returns the application-level messages
    // now ready for delivery: already deduplicated (retransmitted
    // reliable packets aren't delivered twice) and, for ReliableOrdered,
    // already reordered into sequence. Malformed input (fails
    // deserialize_header) is silently ignored - a defensive no-op, not
    // an exception, since a corrupt/foreign UDP datagram is routine on
    // an open port, not exceptional.
    std::vector<ReceivedMessage> on_packet_received(const std::vector<u8>& raw_packet);

    // Number of reliable packets still awaiting acknowledgment across
    // all channels - mainly for tests/diagnostics.
    usize pending_reliable_count() const { return pending_acks_.size(); }

   private:
    struct PendingReliablePacket {
        std::vector<u8> packet;
        f32 time_since_send = 0.0f;
    };

    void queue_ack(Channel channel, u16 sequence);
    static u32 pending_key(Channel channel, u16 sequence);

    f32 retransmit_interval_;
    std::array<u16, 4> next_sequence_by_channel_{};
    std::vector<std::vector<u8>> outgoing_;

    // Keyed by pending_key(channel, sequence) - see packet_header.h for
    // why acks need the channel too (each channel has its own sequence
    // space).
    std::unordered_map<u32, PendingReliablePacket> pending_acks_;

    // UnreliableSequenced: newest sequence delivered so far (per
    // channel instance - one Connection only ever uses this for that
    // one channel, so a single counter suffices).
    bool has_seen_unreliable_sequenced_ = false;
    u16 highest_unreliable_sequenced_seen_ = 0;

    // ReliableUnordered: sequence numbers already delivered, so a
    // retransmitted duplicate isn't handed to the application twice.
    // Known limitation: grows unboundedly over a very long connection
    // (see DECISIONS.md) - fine for this vertical slice's traffic
    // volumes.
    std::unordered_set<u16> delivered_reliable_unordered_;

    // ReliableOrdered: the next sequence the application is waiting for,
    // and any later-arriving packets buffered until the gap closes.
    u16 next_expected_ordered_ = 0;
    std::map<u16, std::vector<u8>> reorder_buffer_;
};

}  // namespace lcu::network
