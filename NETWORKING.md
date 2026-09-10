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
independently drift out of sync. All nine messages are a one-byte type
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
- **BlockAction** (`type=5`, client->server, sent ReliableOrdered):
  `[action: u8][x, y, z: 3×i64][block_id: u16]`. A requested break
  (`action=0`) or place (`action=1`) - a request, not a fact; see "Block
  edit replication" below for how the server validates it. `block_id` is
  only meaningful for a place request.
- **BlockChange** (`type=6`, server->client, broadcast to every
  connected client, sent ReliableOrdered): `[x, y, z: 3×i64][block_id:
  u16]`. The server's authoritative result of an *applied* edit -
  `block_id` is `kAirBlockId` (0) for a break, the placed id for a
  place. Never sent for a rejected request.
- **ChunkData** (`type=7`, server->client): `[chunk_x, chunk_y, chunk_z:
  3×i32][compressed_bytes...]` - a full chunk snapshot, `compressed_bytes`
  being exactly what `lcu::serialization::serialize_chunk_to_bytes`
  produces. Logical only: never sent directly (a compressed chunk is
  typically bigger than one UDP datagram) - always fragmented first, see
  the next entry and "Chunk network streaming" below.
- **ChunkDataFragment** (`type=8`, server->client, sent ReliableOrdered):
  `[fragment: message_id: u16, fragment_index: u16, fragment_count: u16,
  data...]` - one piece of a `lcu::network::fragment_payload`-split
  `ChunkData` message. The receiver's `FragmentReassembler` accumulates
  fragments by `message_id` and hands the reassembled bytes to
  `decode_chunk_data` once every piece has arrived.

## Block edit replication (Phase 13)

Server-authoritative, following the same principle as player movement
(brief section 8): a client's break/place is a *request*
(`BlockAction`), not an immediate local mutation. The server validates
it against its own `World` and `BlockRegistry`:

- the target's chunk must be loaded on the server;
- a break must target a non-air block; a place must target an air block
  and name a `block_id` that's actually registered (`< BlockRegistry::
  count()`);
- the target must be within `kMaxBlockActionRange` (10 blocks) of that
  client's own server-known player position (`ClientState::player`) -
  brief section 20's "never blindly trust client data": without this, a
  malicious client could edit any loaded coordinate regardless of where
  its player actually is.

A rejected request is logged (`LCU_LOG_WARN`) and otherwise silently
dropped - no rejection message is sent back, so the requester's own
`World` simply never changes for that request (see "What's deferred"
below for what this doesn't cover). A validated request is applied to
the server's `World` immediately and broadcast as `BlockChange` to
*every* connected client, including the requester itself - unlike
`PlayerInput`/`PlayerCorrection`, a client never mutates its own `World`
for a block edit speculatively; it waits for its own `BlockChange` to
come back over the wire, same as any other client would (see
DECISIONS.md "block edits are not client-predicted").

Item pickup/consumption stays entirely client-local and optimistic - a
`VoxelClient` gives itself a `game:stone` item the moment it *sends* a
break `BlockAction` (not when the `BlockChange` confirming it arrives,
since every client receives every `BlockChange`, including edits other
players made, and has no way to tell "was this my own edit" from the
message alone), and consumes one the moment it sends a place
`BlockAction`. There is no server-side inventory yet, so a request the
server ends up rejecting is not refunded.

Verified via a real three-process run (one `VoxelServer`, two
`VoxelClient`s - one performing a synthetic break-then-place via
`LCU_VERIFY_BREAK_PLACE`, the other purely observing): the server logs
`Applied BlockAction from <addr>: (0,28,-1) 1 -> 0` then `... (0,29,-1)
0 -> 1`; the acting client logs the item pickup/consumption and `Applied
server BlockChange` for both edits; the *observing* client - which
never touched either block itself - independently logs the identical
`Applied server BlockChange` lines, confirming its `World` actually
converged to match the other two processes', not just that a message
arrived (see BUILD_STATUS.md for the exact reproduce steps).

## Chunk network streaming (Phase 14)

Right after `Welcome` and the `block_change_history` replay, `VoxelServer`
sends a newly-connecting client every chunk it currently has loaded
(`World::loaded_chunk_coords()`), as `ChunkData` - not because the client
can't generate matching terrain on its own (it independently regenerates
the same deterministic terrain from the same compile-time `kWorldSeed`,
and usually does end up identical), but because the server is the
*authoritative* source of world state (brief section 19) and the client
should receive that state, not merely happen to agree with it. Each
chunk's already-tested compression path
(`lcu::serialization::serialize_chunk_to_bytes` - the same in-memory
primitive `save_chunk_to_file` now wraps) produces the compressed bytes;
`lcu::network::fragment_payload` splits the encoded `ChunkData` message
into `kMaxFragmentDataSize` (1024-byte) pieces, each wrapped as a
`ChunkDataFragment` and sent `ReliableOrdered`; the client's single
`lcu::network::FragmentReassembler` reassembles them (tolerating
out-of-order/duplicate delivery, though `ReliableOrdered` already
guarantees in-order arrival here) and, once complete, decodes the
`ChunkData` and applies it: `lcu::serialization::deserialize_chunk_from_bytes`
into a scratch `Chunk`, a full overwrite of the client's local chunk at
that coordinate (`*target = server_chunk`), then a full relight
(`compute_block_light`+`compute_sky_light`, the same pass used for a
freshly-generated chunk) and a remesh of that chunk plus all six of its
axis-adjacent neighbors (any boundary block could have changed).

Verified via real two-process runs: a `mobile_low`-profile run (one
1-chunk world) logs `Sent 1 chunk(s) (1 fragment(s))` server-side and
`Applied server ChunkData for chunk (0, 1, 0)` client-side; a
`desktop`-profile run (36 loaded chunks) logs `Sent 36 chunk(s) (36
fragment(s))` and exactly 36 matching `Applied server ChunkData` lines
client-side with zero warnings/errors - confirming both the common
single-fragment-per-chunk case and that the full loaded world, not just
one chunk, streams and applies correctly.

**Known simplifications** (see DECISIONS.md): this is a one-shot full
sync sent once on connect, not interest-managed by distance (unlike
`kInterestRadius` for entities - every currently-loaded server chunk is
sent regardless of where the connecting client's player actually is) and
not re-sent as the client (or server) streams new chunks in after that
point. Both loaded worlds are small enough in this vertical slice
(`radius_xz` ≤ 1) for the gap not to matter yet; a real persistent-world
server would need per-chunk streaming keyed to the client's own
`update_streaming` calls, not a single dump at connect time.

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
against - not simulated, not mocked. A real *three*-process run (Phase
13) additionally confirms block edit replication actually converges two
independent clients' worlds, not just that a message decodes correctly -
see "Block edit replication" above. Real two-process runs (Phase 14)
confirm chunk network streaming end-to-end at both a 1-chunk and a
36-chunk scale - see "Chunk network streaming" above (see BUILD_STATUS.md
for the exact reproduce steps for all of the above).

**Not verified**: behavior over a real (non-loopback) network with real
latency/jitter/loss patterns, NAT traversal, IPv6, or any load beyond a
handful of connections and messages. Two simultaneous clients *are* now
verified (the block-replication three-process run above), a step up
from earlier phases' single-client-only verification.

## What's deferred

- ~~Chunk network streaming + compression~~ **Fixed** (Phase 14):
  `lcu::network::fragment_payload`/`FragmentReassembler` now split a
  compressed chunk (produced by `lcu::serialization::
  serialize_chunk_to_bytes`) across multiple `ChunkDataFragment`
  datagrams and reassemble them - see "Chunk network streaming" above.
  Still a one-shot full sync on connect only, not per-movement streaming
  or interest-managed by distance (see that section's "Known
  simplifications").
- **The client doesn't actually use the server's Welcome `world_seed`**
  to generate its world - it logs the received value (confirming the
  message round-trips correctly) but still calls its own compile-time
  `kWorldSeed` for `World` construction, which happens before any network
  round-trip could complete. Both are hardcoded to 1337 today so this
  isn't currently observable as a mismatch - using the server's seed for
  real needs world generation deferred until after Welcome arrives, a
  bigger structural change than this phase's scope. See DECISIONS.md.
  Now partially moot for block *content* (not generation timing): Phase
  14's `ChunkData` overwrites the client's locally-generated chunk with
  the server's actual one right after connect, so even a genuinely
  mismatched seed would self-correct for every chunk the server sends -
  the structural gap (client briefly generates from the wrong seed
  before that overwrite arrives) still exists, it just no longer causes
  an observable, permanent difference.
- ~~Block edits aren't replicated at all~~ **Fixed**: `BlockAction`
  (client -> server, `ReliableOrdered`) / `BlockChange` (server -> all
  clients, `ReliableOrdered`) now make block edits server-authoritative -
  see "Block edit replication" below. Still deferred: a server-side
  inventory (item pickup/placement-cost is still client-authoritative,
  optimistic, and unrefunded on server rejection) and a world-diff
  catch-up for a client that connects *after* an edit already happened
  (see below).
- ~~No world-diff catch-up for a late-joining client~~ **Fixed**:
  `VoxelServer` now keeps every applied `BlockChange` in order
  (`block_change_history`) and replays the full history to a newly
  connecting client right after its `Welcome`, before any per-tick
  traffic - confirmed by an actual test run (a client that broke then
  placed a block, followed later by a second client connecting only
  after both edits had happened, still logs `Applied server BlockChange`
  for both). Unbounded for the server process's lifetime - a real
  long-running server needs to compact this against persisted chunk
  state once chunk save/load has an actual server-side trigger (still
  missing, see PROJECT_STATE.md), not keep every edit forever; fine for
  this vertical slice's session lengths.
- **No rejection feedback for a `BlockAction` the server refuses.** The
  requester's own world silently stays as it was (correct), but nothing
  tells the client *why* - no error message, no UI feedback. Combined
  with the client-authoritative/optimistic item accounting above, a
  rejected place request currently loses the player's item with no
  visible explanation. Acceptable for this vertical slice (rejections
  are rare - only a genuine race or a malicious client normally triggers
  one); a real rejection channel is future work.
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
