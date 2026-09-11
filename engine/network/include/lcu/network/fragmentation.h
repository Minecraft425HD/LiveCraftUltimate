#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "lcu/core/types.h"

namespace lcu::network {

// A payload too big for one UDP datagram (a compressed chunk is a few
// KB; kMaxDatagramSize is 1200 bytes, see udp_socket.h) needs splitting
// across several - this is that split/rejoin layer (brief section
// 19/NETWORKING.md "Chunk network streaming"). Deliberately NOT baked
// into Connection/PacketHeader itself: every existing message stays
// exactly as small and as fast as it already is, and a caller that never
// sends anything oversized (the large majority of this codebase's
// traffic) never pays for this at all. A caller that does have something
// oversized to send fragments it explicitly, sends each fragment through
// Connection::send() like any other payload (ReliableOrdered - a lost
// fragment must be recovered, and delivery order doesn't otherwise
// matter since FragmentReassembler indexes by position, not arrival
// order), and feeds each received fragment through FragmentReassembler
// before treating the result as a real message.
//
// Wire layout of one fragment's payload (big-endian, matching every
// other multi-byte field in this codebase - see replication_protocol.cpp):
// [message_id: u16][fragment_index: u16][fragment_count: u16][data...]

// Conservative per-fragment data budget: kMaxDatagramSize (1200) minus
// PacketHeader::kWireSize (4) minus this fragment header (6) minus a
// safety margin, rounded to a clean number - not pushed to the exact
// byte limit, since that leaves zero room for a slightly different
// PacketHeader/Connection revision without silently starting to drop
// oversized fragments again.
constexpr usize kMaxFragmentDataSize = 1024;

// Splits `payload` into fragments of at most `max_fragment_data_size`
// data bytes each, every fragment prefixed with the 6-byte header
// above. `message_id` distinguishes concurrently in-flight fragmented
// messages on one connection from each other - callers fragmenting more
// than one message at a time must give each a distinct id (a simple
// wrapping counter is enough; see server/main.cpp for a real one).
// Always returns at least one fragment, even for an empty payload.
std::vector<std::vector<u8>> fragment_payload(const std::vector<u8>& payload, u16 message_id,
                                               usize max_fragment_data_size = kMaxFragmentDataSize);

// Accumulates fragments (as produced by fragment_payload, received in
// any order) keyed by their message_id. Returns the fully reassembled
// payload once every fragment for that message_id has arrived;
// std::nullopt while still incomplete. A malformed fragment (too short
// to contain the 6-byte header) is ignored, not fatal. Not itself
// reliable/ordered - relies on the caller sending fragments over a
// reliable channel (see Connection::Channel::ReliableOrdered) so every
// fragment is guaranteed to eventually arrive exactly once.
class FragmentReassembler {
   public:
    std::optional<std::vector<u8>> add_fragment(const std::vector<u8>& fragment);

   private:
    struct InProgress {
        std::vector<std::optional<std::vector<u8>>> parts;
        usize received_count = 0;
    };

    std::unordered_map<u16, InProgress> in_progress_;
};

}  // namespace lcu::network
