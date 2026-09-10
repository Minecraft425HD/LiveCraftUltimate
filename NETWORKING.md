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
independently drift out of sync. All ten messages are a one-byte type
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
- **InventoryUpdate** (`type=9`, server->one client, sent
  ReliableOrdered): `[item_id: u16][count: u32]`. That client's
  authoritative count for one item, sent after every `BlockAction` that
  could have affected it - see "Server-side inventory" below.

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

Item pickup/consumption still fires client-side, optimistically, the
moment `VoxelClient` *sends* a break/place `BlockAction` (not when the
`BlockChange` confirming it arrives, since every client receives every
`BlockChange`, including edits other players made, and has no way to
tell "was this my own edit" from the message alone) - unchanged from
when this was written. What changed in Phase 15: that optimistic guess
is no longer the only bookkeeping - `VoxelServer` now keeps its own
authoritative count per client and corrects the client's guess via
`InventoryUpdate` whenever they disagree, including on a rejected
request. See "Server-side inventory" below.

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

## Per-movement chunk streaming (Phase 16)

Phase 14's `ChunkData` sync only ever ran once, right after connect -
honestly documented there as a known gap ("not re-streamed as either
side's loaded-chunk set changes afterward"). This phase closes it.
`VoxelServer` now re-checks
every connected client's loaded-chunk range on every tick: converts
that client's current, server-known player position
(`ClientState::player.aabb.center()`) to a chunk coordinate
(`chunk_coord_of_position`), and - only when that differs from
`ClientState::last_streamed_center` (a per-client "last checked at"
cache, so a stationary or vertically-only-moving client costs nothing
extra) - loads any not-yet-loaded chunk in `load_settings.radius_xz`/
`min_chunk_y`/`max_chunk_y` around it, exactly like the startup load
loop. Any chunk that transitions from Unloaded this tick is broadcast
as `ChunkData` (fragmented, same as the connect-time sync) to *every*
connected client, not just the one whose movement triggered it - anyone
already connected is equally missing a chunk that didn't exist a moment
ago.

**The server's shared `World` only ever grows, never shrinks** - a
deliberate simplification (see DECISIONS.md "server-side chunk
streaming never unloads"): `World` is one instance shared across every
connected client, so unloading a chunk because *one* client moved away
from it could break a *different* client that's still standing in it.
Real per-client interest-scoped unloading would need either a
per-client "what have I actually sent this client" set or per-client
`World` instances - both real architecture changes deferred until
something (a long-running server's memory footprint, say) actually
needs them, not built speculatively now.

`VoxelClient` runs the mirror-image local half unconditionally
(single-player and networked alike): the same generate-then-light-then-
mesh sequence the initial spawn-area load already runs, triggered only
when the player's own chunk coordinate changes since it was last
checked - so a locally-generated placeholder chunk exists to fill in
before any server `ChunkData` for that new coordinate could possibly
arrive. The `ChunkDataFragment` handler (Phase 14) also gained a small
but real fix for this phase: a `ChunkData` for a coordinate the client
hasn't locally streamed to yet (a different client's movement grew the
server's world past this client's own bounds, or this client's local
trigger simply hasn't fired yet this frame) now calls `world.load_chunk`
to create a real slot before overwriting it, instead of the old
"isn't loaded locally, ignoring" silent drop - a genuine gap Phase 14's
scope (a fixed, initial-sync-only region) never actually exercised.

Verified via two real multi-process runs. Two-process: a `mobile_low`
client held `MoveForward` for 6 real seconds (`LCU_VERIFY_MOVE_SECONDS`,
a new headless verification hook - frame-count-indexed hooks don't work
here since the main loop is unthrottled and how far a fixed frame count
travels depends on real elapsed time, not frame count) - enough to
cross the 16-block chunk boundary at `kMoveSpeed`; the server logs
`Streamed 1 newly-loaded chunk(s) into range (total 2 loaded)` and the
client logs `Applied server ChunkData for chunk (0, 1, -1)`, zero
warnings/errors. Three-process: the same moving client alongside a
second, entirely stationary client that never sent a single
`PlayerInput` with nonzero movement - that stationary client's own log
shows the identical `Applied server ChunkData for chunk (0, 1, -1)`
line, proving the broadcast-to-every-connected-client path (not just
the triggering client) actually works, not just that a message decoded.

**Known simplification:** still no interest-managed unloading (see
above); a client's own local streaming and the server's are two
independent triggers that usually agree (same radius, same movement)
but aren't literally synchronized - a client could in principle stream
a coordinate locally a frame or two before or after the server's own
broadcast for it arrives, resolved by whichever happens second simply
overwriting (idempotent, not a race that corrupts anything, just
occasionally-redundant work).

## Server-side inventory (Phase 15)

`VoxelServer` now holds a real, authoritative `lcu::items::Inventory`
(9 slots, matching `VoxelClient`'s own) per connected client
(`ClientState::inventory`), populated only by validated `BlockAction`s -
never by anything the client sends directly. Registers the same
`game:stone` item `VoxelClient` does (namespaced id, display name, max
stack size all identical - both sides register it as their only item,
so their `ItemId`s coincide by construction, the same simplification
block/item ids already carry for mod content).

`handle_block_action` now does two things with it:

- **Placing `game:stone` specifically requires the client to actually
  hold one, server-side** - a new validity condition alongside the
  existing chunk-loaded/target-state checks: `action.block_id ==
  stone_id` with `client.inventory.count_item(stone_item_id) == 0` is
  rejected exactly like any other invalid request. Any other registered
  `block_id` (mod content, say) isn't gated - there's no general
  block->item mapping yet, just this one hardcoded case (see
  DECISIONS.md).
- **A successful break of `game:stone` adds one to the requester's
  server-side inventory; a successful place of it removes one** - the
  server's own bookkeeping, driven by what it actually just applied to
  its `World`, not by anything the client claimed.

After *every* `BlockAction` - accepted or rejected, at any of the four
possible rejection points or after a successful apply - `VoxelServer`
sends that one client an `InventoryUpdate` with its current
authoritative `game:stone` count. `VoxelClient` still fires its own
optimistic pickup/consumption at request-send time (unchanged from
Phase 13 - see DECISIONS.md), but now reconciles it against every
`InventoryUpdate` it receives, the same pattern `PlayerCorrection`
already uses for movement: compute the delta between the optimistic
local count and the server's authoritative one, `add_item`/`remove_item`
to close it, and log only when they actually disagreed. This closes the
Phase 13 "no rejection feedback, no refund" gap for `game:stone`
specifically: a request the server rejects no longer silently leaves
the client's displayed count wrong forever - the very next
`InventoryUpdate` corrects it.

Verified via a real two-process run (one `VoxelServer`, one
`VoxelClient` via `LCU_VERIFY_BREAK_PLACE`): the client's log shows the
full disagree-then-reconcile cycle in both directions - after the
optimistic break-pickup (`Picked up 1 game:stone (inventory: 1)`) and
the optimistic place-consume (`Requesting place ... (inventory: 0)`),
two `Reconciled inventory item 1 to authoritative count ...` lines
appear (`0` corrected to `1`, matching the accepted break; then `1`
corrected to `0`, matching the accepted place) immediately followed
by the corresponding `Applied server BlockChange` lines - proving the
server's authoritative count and the client's optimistic guess actually
converged after each round trip, not just that a message decoded.

**Known simplification:** only `game:stone` is inventory-backed for
placement; there's no general block-id-to-item-id mapping, so any other
registered block (mod content) can still be placed without an item
check. A malicious client also can't fabricate items it doesn't hold
(the server never trusts a client-reported count for anything), but
there's still no persistence - a server-side inventory is entirely
in-memory for the connection's lifetime, lost on disconnect, same as
every other per-client server state today.

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
36-chunk scale - see "Chunk network streaming" above. A real two-process
run (Phase 15) confirms server-side inventory reconciliation - the
client's optimistic guess and the server's authoritative count actually
converge after each round trip, not just that a message decoded - see
"Server-side inventory" above. Real two-process and three-process runs
(Phase 16) confirm per-movement chunk streaming: a client whose real,
server-simulated position crosses a chunk boundary triggers a genuinely
new chunk being streamed to it, and a second, entirely stationary
client independently receives the same broadcast - see "Per-movement
chunk streaming" above (see BUILD_STATUS.md for the exact reproduce
steps for all of the above).

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
- ~~Chunk streaming is a one-shot full sync on connect only, not
  per-movement~~ **Fixed** (Phase 16): `VoxelServer` now re-checks every
  connected client's loaded-chunk range every tick and streams/
  broadcasts anything newly in range - see "Per-movement chunk
  streaming" above. Still not interest-managed by distance in the sense
  of ever *unloading* anything - the shared `World` only grows (see that
  section's "Known simplification").
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
  see "Block edit replication" below.
- ~~No server-side inventory (item pickup/placement-cost is client-
  authoritative, optimistic, and unrefunded on server rejection)~~
  **Fixed** (Phase 15) for `game:stone` specifically: `VoxelServer` now
  keeps a real, authoritative per-client `Inventory`, gates placing
  `game:stone` on actually holding one server-side, and corrects the
  client's optimistic guess via `InventoryUpdate` after every
  `BlockAction` - see "Server-side inventory" below. Still deferred:
  any other block/item isn't inventory-gated (no general block-id-to-
  item-id mapping yet), and there's no persistence across a
  disconnect/reconnect.
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
