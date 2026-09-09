# Networking

`engine/network`'s transport, as actually implemented and tested in Phase 7.
See `DECISIONS.md` for the reasoning behind each choice below, and
`BUILD_STATUS.md`/`PROJECT_STATE.md` for what's verified vs. outstanding.

## Transport

A single UDP socket per peer (`lcu::network::UdpSocket`), IPv4 only. UDP
was chosen over TCP specifically so the four channel semantics below
(particularly the two unreliable ones) can exist at all - TCP's own
in-order reliable byte stream can't express "unreliable" or "reliable but
unordered" without working around TCP's own guarantees. See DECISIONS.md
"UDP transport with a hand-rolled channel protocol, not TCP or a
third-party library".

There is no separate "connect" step at the transport level: UDP has no
built-in notion of a connection. `VoxelServer` treats the first datagram
it ever receives from a given `Address` as that peer connecting (see
"Server connection model" below).

## Wire format

Every packet starts with a 4-byte header (`lcu::network::PacketHeader`,
`serialize_header`/`deserialize_header` in `packet_header.h`/`.cpp`):

| Byte | Field | Meaning |
|---|---|---|
| 0 | `type` | `0` = Data, `1` = Ack |
| 1-2 | `sequence` | big-endian `u16`. Data: this packet's own sequence number. Ack: the sequence number being acknowledged. |
| 3 | `channel` | `0`=UnreliableUnordered, `1`=UnreliableSequenced, `2`=ReliableUnordered, `3`=ReliableOrdered. Data: which channel this payload was sent on. Ack: which channel the acknowledged packet was sent on (acks need this because each channel has its own independent sequence space - see below). |

For a Data packet, everything after byte 3 in the same UDP datagram is
the application payload - no length prefix is needed, since one
`recvfrom()` call already returns exactly one sent datagram (unlike a
TCP byte stream, which has no such boundary and would need one).

## Channel semantics

Implemented by `lcu::network::Connection` (`connection.h`/`.cpp`), which
never touches a socket itself - it only produces/consumes raw byte
packets, so the protocol logic is fully unit-testable with zero real
I/O (see `tests/network/connection_test.cpp`) as well as exercised over
real loopback sockets (`tests/network/loopback_integration_test.cpp`).

- **UnreliableUnordered** — sent once, no ack, no ordering. Delivered to
  the application immediately on arrival, duplicates and all.
- **UnreliableSequenced** — like UnreliableUnordered, but the receiver
  tracks the newest sequence number delivered so far on this channel and
  silently drops anything older (using `sequence_greater_than`'s
  wraparound-correct comparison, not raw numeric comparison). Useful for
  "only the latest value matters" data.
- **ReliableUnordered** — retransmitted (see below) until acknowledged,
  and delivered to the application as soon as it arrives - not held back
  to preserve send order. Duplicate deliveries (from a retransmit the
  receiver already saw) are suppressed via a per-connection set of
  already-delivered sequence numbers for this channel.
- **ReliableOrdered** — retransmitted until acknowledged, and buffered
  if it arrives ahead of an earlier packet that hasn't shown up yet;
  delivered to the application strictly in sequence order. A duplicate
  of an already-delivered packet is dropped (but still acknowledged, in
  case the sender never saw the first ack).

Each channel has its **own independent sequence counter**, starting at 0
per `Connection` instance - unrelated traffic on other channels never
creates gaps in a channel's own sequence space (this matters
specifically for ReliableOrdered's contiguity check).

## Reliability: ack + retransmit

Every reliable Data packet sent is kept in a per-connection pending-ack
table (keyed by `(channel, sequence)`) alongside a resend timer. Every
`Connection::update(dt)` call advances that timer; once it exceeds
`kDefaultRetransmitInterval` (200ms) without having been acknowledged,
the original packet is resent verbatim and the timer resets. Receiving
an Ack packet for a `(channel, sequence)` removes it from the pending
table.

**Known simplification:** the retransmit interval is fixed, not
RTT-adaptive, and there is no exponential backoff, congestion control,
or maximum-resend-count cutoff (a permanently-unreachable peer's
reliable packets are retried forever). This is deliberately the simplest
version that is still correct under real (tested) packet loss - see
DECISIONS.md "reliable channel has no RTT estimation or congestion
control yet".

## Application-level messages

`engine/network` itself has no opinion on what a payload *means* - that
framing lives in whichever caller uses it. `VoxelServer` currently
defines two, both a one-byte type tag followed by fixed big-endian
fields (hand-rolled, not a generic serialization framework - see
DECISIONS.md):

- **Welcome** (`type=0`, sent ReliableOrdered): `[world_seed: u32][tick_rate: u8]`.
  Sent once, the first time the server sees a new peer's address.
- **Heartbeat** (`type=1`, sent UnreliableSequenced): `[tick: u32][entity_count: u16]`.
  Sent to every known connection once per server tick.

## Server connection model

`VoxelServer` keeps `std::unordered_map<Address, Connection>`. Any
address a datagram arrives from that isn't already a key becomes a new
entry (logged, and immediately sent a Welcome). There is **no
authentication or handshake validation** beyond "a UDP packet arrived
from this address" - anyone who can send a UDP packet to the server's
port is treated as connected. This is acceptable for this phase's
vertical slice (proving real client-server messages flow over the
transport) but not for any real deployment - see PROJECT_STATE.md
"Known Limitations" and DECISIONS.md.

## What's verified

See `BUILD_STATUS.md` for the full table. Summary: the protocol logic
(ordering, dedup, retransmission timing) is unit-tested with simulated
packets; `UdpSocket` is tested over real loopback traffic; two full
integration tests run real `Connection` pairs over real sockets on
`127.0.0.1`, including one that deliberately drops the first real
datagram sent and confirms retransmission recovers it; and
`VoxelServer`'s real handshake was verified end-to-end against a
standalone Python UDP client script (not part of the automated test
suite - see BUILD_STATUS.md for the exact commands to reproduce).

**Not verified**: behavior over a real (non-loopback) network with real
latency/jitter/loss patterns, NAT traversal, IPv6, or any load beyond a
handful of connections and messages.

## What's deferred

- **RELIABLE_UNORDERED/RELIABLE_ORDERED reorder/ack-set correctness
  under sequence wraparound** (past 65536 messages on one channel) -
  `sequence_greater_than` itself handles wraparound correctly, but the
  `std::map`/`std::unordered_set` used for the reorder buffer and
  duplicate-detection set key on raw `u16` values, whose *numeric*
  ordering doesn't match wraparound-aware ordering. Not an issue at this
  vertical slice's message volume.
- **RTT estimation, congestion control, packet coalescing/batching,
  compression** - brief section 76: revisit only if profiling on real
  traffic shows a need, not speculatively now.
- **Client-side networking** (`VoxelClient` doesn't connect to a
  `VoxelServer` at all yet) - replication and client-side
  prediction/interpolation are Phase 8's job, not this phase's.
- **Authentication/encryption** - no player identity system exists yet
  to authenticate against (Phase 9+ territory).
