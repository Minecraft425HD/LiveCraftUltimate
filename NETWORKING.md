# Networking

`engine/network`'s transport (Phase 7) and `game::systems::protocol`'s
application-level replication messages plus `engine/replication`'s
prediction/interpolation (Phase 8), as actually implemented and tested.
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
framing lives in `game::systems::protocol` (`replication_protocol.h`/
`.cpp`), shared by `VoxelClient` and `VoxelServer` so they can't
independently drift out of sync. All five messages are a one-byte type
tag followed by fixed big-endian fields (hand-rolled, not a generic
serialization framework - see DECISIONS.md):

- **Welcome** (`type=0`, server->client, sent ReliableOrdered):
  `[world_seed: u32][tick_rate: u8]`. Sent once, the first time the
  server sees a new peer's address.
- **Heartbeat** (`type=1`, server->client, sent UnreliableSequenced):
  `[tick: u32][entity_count: u16]`. Sent to every known connection once
  per server tick.
- **EntityState** (`type=2`, server->client, sent UnreliableSequenced):
  `[entity_count: u8][(entity_index: u32, position: 3×f32) ...]`. The
  AI entities' current positions, filtered per-client by interest
  management (see below) - sent once per server tick per client.
- **PlayerInput** (`type=3`, client->server, sent UnreliableSequenced):
  `[sequence: u32][horizontal_delta: 3×f32][dt: f32]`. The sending
  client's local movement input for that frame - only the latest matters
  if one goes missing, hence unreliable.
- **PlayerCorrection** (`type=4`, server->client, sent
  UnreliableSequenced): `[acknowledged_sequence: u32][position: 3×f32]`.
  The server's authoritative position for that client's player as of the
  last `PlayerInput` it actually applied - sent every
  `kCorrectionIntervalTicks` (4) ticks, not every tick.

## Client-side prediction + reconciliation (Phase 8)

`VoxelClient`, when connected (`LCU_CONNECT_PORT` set - see "Client
connection model" below), predicts its own player's movement locally
and immediately via `lcu::replication::PredictionBuffer<PlayerPhysicsState,
Vec3>` (`predict_and_record`) instead of waiting for a server round-trip,
so movement feels instant. Each predicted input is recorded with an
incrementing `sequence` and sent to the server as `PlayerInput`.
`VoxelServer` independently applies the same physics
(`apply_gravity`/`integrate_player`, against its own copy of the world)
to each `PlayerInput` it receives and periodically reports the result as
`PlayerCorrection`. When a `PlayerCorrection` arrives, the client calls
`PredictionBuffer::reconcile()`: discard every recorded input at or
before the acknowledged sequence, then replay everything still pending
on top of the server's corrected state - reconstructing an up-to-date
prediction that already accounts for whatever the server actually
observed, not just what the client assumed.

## Remote entity interpolation (Phase 8)

`VoxelClient`, when connected, does not run the wandering-AI simulation
locally at all (the server already does - see server/main.cpp) - it
maintains one `lcu::replication::PositionInterpolator` per entity
(keyed by `EntityState`'s `entity_index`), fed by
`add_sample(local_clock, position)` on every `EntityState` message.
Querying `interpolated_position(local_clock)` renders a smoothed
position between the two most recent real samples (a small render delay
behind the latest arrival, rather than snapping to each new sample the
instant it's received - see the type's own doc comment).

## Interest management (Phase 8)

`VoxelServer` computes each client's `EntityState` message independently,
including only AI entities within `kInterestRadius` (24 blocks) of that
client's own server-known player position - genuine distance filtering,
not a placeholder, even though every entity in this vertical slice's
small loaded area currently falls within it for any client near spawn
(nothing has moved far enough to exercise the filter actually excluding
something - see DECISIONS.md).

## Server connection model

`VoxelServer` keeps `std::unordered_map<Address, ClientState>`
(`ClientState` = one `Connection` plus one server-owned
`PlayerPhysicsState` plus the last acknowledged input sequence). Any
address a datagram arrives from that isn't already a key becomes a new
entry (logged, spawned at the same point the single-player client
spawns at, and immediately sent a Welcome). There is **no authentication
or handshake validation** beyond "a UDP packet arrived from this
address" - anyone who can send a UDP packet to the server's port is
treated as connected. This is acceptable for this phase's vertical slice
(proving real client-server messages and physics flow over the
transport) but not for any real deployment - see PROJECT_STATE.md
"Known Limitations" and DECISIONS.md.

## Client connection model

Set the `LCU_CONNECT_PORT` env var to make `VoxelClient` connect to a
`VoxelServer` on `127.0.0.1:<port>` at startup instead of running fully
single-player/local (unset: identical single-player behavior to every
earlier phase, unaffected). Only loopback IPv4 is supported - there is
no hostname/IP-string parser anywhere yet, and no in-game "connect to a
server" UI (both later work - see DECISIONS.md/PROJECT_STATE.md).

## What's verified

See `BUILD_STATUS.md` for the full table. Summary: the protocol logic
(ordering, dedup, retransmission timing) is unit-tested with simulated
packets; every `game::systems::protocol` message round-trips through its
own encode/decode unit tests; `PositionInterpolator`/`PredictionBuffer`
are unit-tested standalone (the latter against both a hand-verifiable
plain-float instantiation and a real `PlayerPhysicsState`/
`integrate_player` instantiation); `UdpSocket` is tested over real
loopback traffic; two full integration tests run real `Connection` pairs
over real sockets on `127.0.0.1`, including one that deliberately drops
the first real datagram sent and confirms retransmission recovers it;
and a real two-process run connects an actual `VoxelClient` to an actual
`VoxelServer` over real loopback UDP and confirms the full loop - Welcome
received, `EntityState` positions rendered through real interpolation,
`PlayerInput` sent and a `PlayerCorrection` received and reconciled
against - not simulated, not mocked (see BUILD_STATUS.md for the exact
reproduce steps).

**Not verified**: behavior over a real (non-loopback) network with real
latency/jitter/loss patterns, NAT traversal, IPv6, more than one
simultaneous client, or any load beyond a handful of connections and
messages.

## What's deferred

- **Chunk network streaming + compression.** `engine/serialization::
  chunk_serializer` already produces zstd-compressed chunk bytes (Phase
  3), but a compressed chunk (a few KB) doesn't fit in one
  `kMaxDatagramSize` (1200-byte) UDP datagram - sending it over
  `engine/network` as-is would need message fragmentation (splitting one
  logical message across multiple datagrams and reassembling them
  in order), which doesn't exist in `Connection` yet. Both clients
  currently generate their own local copy of the world from the same
  hardcoded seed instead of receiving it from the server - see the next
  entry, and DECISIONS.md "chunk streaming deferred: fragmentation
  prerequisite".
- **The client doesn't actually use the server's Welcome `world_seed`**
  to generate its world - it logs the received value (confirming the
  message round-trips correctly) but still calls its own compile-time
  `kWorldSeed` for `World` construction, which happens before any network
  round-trip could complete. Both are hardcoded to 1337 today so this
  isn't currently observable as a mismatch - using the server's seed for
  real needs world generation deferred until after Welcome arrives, a
  bigger structural change than this phase's scope. See DECISIONS.md.
- **Block edits aren't replicated at all.** Break/place still only
  mutates the connected client's own local `World` - not sent to the
  server, not seen by other clients. Needs the same reliable-message
  machinery `PlayerInput`/`PlayerCorrection` already prove out, just not
  wired for block edits yet.
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
- **Authentication/encryption** - no player identity system exists yet
  to authenticate against (Phase 9+ territory).
- **Multiple simultaneous clients** are structurally supported (the
  server already keys everything by `Address` in a map) but never
  actually exercised together in any run so far - every verification run
  to date has used exactly one client.
