# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, and
`NETWORKING.md` once networking is relevant, in that order, before
touching code. The repository is the source of truth, not this file's
prose if the two disagree — if in doubt, run the build and tests and
trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

All 12 phases from the original queue are complete/functionally
complete for what this headless sandbox can verify (see the per-phase
history below). The project is now past that original 12-phase queue
and into open-ended continued development (brief: "the goal is a
complete playable game, not a completed checklist") - **Phase 13 (block
edit replication)**, **Phase 14 (chunk network streaming)**, **Phase 15
(server-side inventory)**, **Phase 16 (per-movement chunk streaming)**,
**Phase 17 (surface/subsurface terrain content)**, and **Phase 18 (item
mappings for grass/dirt)** are done; see "Reality Audit" and "Last
Completed Task" below for what they cover and what's next.

## Reality Audit (2026-09-10)

Per-session discipline: never trust this file's own prior claims
without re-verifying against actual code/build/test/runtime behavior.
This audit was performed by rebuilding both configs from a clean
incremental build, running the full `ctest` suite (313/313 non-bgfx,
316/316 bgfx - both green), and reading the actual networking/
replication code rather than trusting NETWORKING.md's prior text. One
real, confirmed gap was found and closed this session (see below); the
audit did not surface any other documentation/reality mismatch beyond
what was already honestly flagged as PARTIAL/MISSING/UNVERIFIED in this
file's own prior "Known Limitations" section.

| Area | Documented (before this audit) | Actual (verified) |
|---|---|---|
| Player movement replication | COMPLETE | COMPLETE - real two-process test, prediction+reconciliation |
| Entity (AI) replication | COMPLETE | COMPLETE - real interpolation, interest-managed |
| **Block edit replication** | **MISSING** (honestly flagged) | **Was MISSING, now COMPLETE** - see Phase 13 below, real 3-process test |
| Chunk streaming/fragmentation | MISSING (honestly flagged) | Was MISSING, now COMPLETE - see Phase 14 below, real two-process tests at 1-chunk and 36-chunk scale |
| Texture/content pipeline | MISSING (honestly flagged) | Confirmed still MISSING - no texture atlas, no asset loader anywhere in the tree |
| Mod loading | PARTIAL (honestly flagged) | Confirmed PARTIAL - real Lua VM + registry bindings + event bus all work (verified via real run), but only one event (`block_broken`) exists and there's no manifest/dependency format |
| Android/iOS build | UNVERIFIED (honestly flagged) | Confirmed UNVERIFIED - `cmake --preset android-arm64` reaches real NDK detection and fails only there (no NDK installed); no Gradle/Xcode project exists |
| Test suite | "305/305 (nobgfx) / 308/308 (bgfx)" | Was accurate at time of writing; now 313/313 / 316/316 after Phase 13's 10 new protocol tests |

Conclusion: this repository's documentation was **not** found to be
overstating completion anywhere audited - every PARTIAL/MISSING/
UNVERIFIED claim already in this file checked out against the real
code. The one real gap worth closing immediately, given the master
brief's explicit emphasis on multiplayer fundamentals, was block edit
replication - closed this session (Phase 13).

## Last Completed Task

Phase 10: `engine/platform::TouchInputBackend` maps a frame's active
finger touches onto the same `Action`/`InputState` abstraction
`KeyboardInputBackend` already drives (brief section 28) - a
twin-virtual-stick layout (movement drag on the screen's left half,
look drag on the right, both dead-zone-thresholded into the existing
discrete Move*/Look* actions) plus fixed button rects for
Jump/Interact/PlaceBlock/Sprint/Crouch/Inventory. `MovementInput`/
`FirstPersonCamera`/the break-place loop need zero changes to work with
it - they only ever read `InputState`. Pure logic with no SDL
dependency (there's no real touchscreen in this sandbox to wire a real
`SDL_EVENT_FINGER_*` pump against, so that plumbing is honestly
deferred - see Known Limitations), fully exercised by 13 unit tests
against synthetic `TouchPoint` lists (single-finger drag, dead zone,
two simultaneous drags, button-vs-drag disambiguation, release
clearing state).

Added `lcu::core::QualityProfile`/`chunk_load_settings_for` (brief
section 60's MOBILE_LOW/MEDIUM/HIGH, plus `Desktop`) - deliberately
placed in `engine/core`, not `engine/platform`, since `VoxelServer`
needs it too and `engine/platform` is gated behind `LCU_BUILD_CLIENT`
and off-limits to the server (see ARCHITECTURE.md "Server has zero
GPU/window dependency"). `Desktop` is numerically identical to this
project's pre-existing hardcoded chunk-load radius/vertical-range
(`kLoadRadiusXZ`/`kMinChunkY`/`kMaxChunkY`, now removed in favor of
this), so nothing changes by default; each Mobile tier trims both,
down to a single loaded chunk for `MobileLow`. Wired into both
`VoxelClient` and `VoxelServer` via a new `LCU_QUALITY_PROFILE` env
var (an unrecognized value falls back to `Desktop`, not a crash). 4
unit tests plus real-run verification: default and an invalid env var
value both still log "Loaded 36 chunks" exactly as before this phase;
`LCU_QUALITY_PROFILE=mobile_low`/`mobile_high` log "Loaded 1 chunks"/
"Loaded 27 chunks" respectively - a real behavioral change, not just a
label, confirmed by an actual run.

Re-verified `CMakePresets.json`'s `android-arm64`/`ios` presets:
`cmake --preset android-arm64` correctly reaches and fails only at
Android's own NDK-detection step, confirming the preset itself parses
and is structurally correct - not broken CMake, just genuinely blocked
on a missing toolchain in this environment. A full Android Gradle
project / iOS Xcode project wrapper around these presets is
deliberately not written this phase - unverifiable native-mobile
project boilerplate this sandbox can't build or run is exactly what
the project's "never claim done beyond what's verified" discipline
argues against (see DECISIONS.md); it's real work for whoever has the
actual NDK/Xcode toolchain to exercise it against.

17 new unit tests (`TouchInputBackend`, `QualityProfile`/
`parse_quality_profile`). `ctest` 291/291 passing (bgfx build) /
288/288 (non-bgfx build).

Phase 11: added `tools/benchmark` (`VoxelBenchmarks`), opt-in via the
existing `LCU_BUILD_TOOLS` option, using Google Benchmark (FetchContent,
pinned, same vendor/pattern as GoogleTest - `third_party/CMakeLists.txt`)
against real engine functions, not synthetic stand-ins:
`ChunkStorage::set_block`/`block_at` (voxel access),
`worldgen::generate_terrain_chunk` (chunk gen), `mesh_chunk_greedy` on
both a fully-solid and a checkerboard chunk (meshing), `compute_block_
light`/`compute_sky_light` (lighting), `raycast`/`move_and_collide`
(physics), `save_chunk_to_file`/`load_chunk_from_file` (serialization -
zstd compression happens inside these, so no separate compression
micro-benchmark was written), a `Connection` `ReliableOrdered` send +
deliver round trip (network), and `update_ai_wander` at 10/100/1000
entities (entity sim - `SetItemsProcessed` reports entities/sec).

Actually run, twice: once in this project's default `Development` build
type (Google Benchmark's own output flagged it "Library was built as
DEBUG" - `Development` sets no optimization flags, a real and useful
observation about the difference between this repo's dev build and an
optimized one), and once in a real `-DCMAKE_BUILD_TYPE=Release` build
(clean run, no such warning) - see `BUILD_STATUS.md` for the exact
numbers from both. One concrete finding from the Release numbers: greedy
meshing a checkerboard-pattern chunk (no two neighbors share a block
type, so no face merging is possible) took ~15x longer than meshing a
fully-solid chunk of the same size (1.45ms vs. 94us) - real, measured
evidence that the algorithm's face-merging is doing substantial,
non-decorative work. No code was changed based on these numbers this
phase - the point was having real measurements to point at, not
guessing at an optimization nothing has shown is actually needed (brief
section 98 applies to premature optimization too, not just premature
abstraction).

15 benchmark cases covering all 8 named areas (not unit tests -
`VoxelBenchmarks` is a separate opt-in executable; `ctest` counts are
unchanged by this phase, still 291/291 / 288/288).

Phase 12 (the last phase in the original queue): `engine/audio` -
`AudioEngine` (RAII wrapper around one `SDL_AudioStream` from
`SDL_OpenAudioDeviceStream`, 44.1kHz stereo float; only `audio_engine.cpp`
includes `<SDL3/SDL_audio.h>`, mirroring `engine/scripting`'s Lua-header
confinement), `generate_sine_wave` (real, own-created procedural PCM tone
content - no WAV pipeline exists and any checked-in asset would need to
be this project's own IP anyway, brief section 12), and
`compute_stereo_pan`/`distance_attenuation` (pure-math positional audio:
pan by lateral angle to the listener, linear distance falloff - a real
first pass, not full HRTF/3D audio). Wired into `VoxelClient`: breaking/
placing a block now plays a real synthesized, positionally-panned tone.
`AudioEngine::init()` failing (no device - most CI, this sandbox without
`SDL_AUDIODRIVER=dummy`) is logged and non-fatal; `play()` becomes a
silent no-op.

`engine/ui::draw_debug_overlay` - a real on-screen HUD using bgfx's
built-in VGA-style debug-text buffer (`Renderer` gained
`draw_debug_text`/`clear_debug_text` wrapping `bgfx::dbgTextPrintf`/
`dbgTextClear`, keeping bgfx access confined to `engine/rendering` per
ARCHITECTURE.md), showing live FPS and a legend for every mobile
touch-control button - drawn at the exact same normalized rects
`TouchInputBackend` hit-tests against. Promoted the touch button layout
out of `touch_input.cpp`'s private `constexpr` array into a shared
`lcu::platform::kTouchButtonLayout` (`touch_control_layout.h`)
specifically so hit-testing and on-screen drawing read from one
definition and can never drift apart - a real, motivated refactor, not
speculative. This closes two `PROJECT_STATE.md` Known Limitations for
real: Phase 10's "a player would currently be dragging/tapping blind"
(the touch overlay now has an actual visual) and the long-standing
"Debug overlay is a log line, not an on-screen overlay."

Verified via real runs, not just unit tests: `AudioEngine initialized:
44100 Hz, stereo float` under `SDL_AUDIODRIVER=dummy`, with the
break/place round trip completing with no crash; a full bgfx (`Noop`
backend) `VoxelClient` run from startup to `LCU_MAX_FRAMES` shutdown
with `draw_debug_overlay` executing every frame, no assert/crash.

15 new unit tests (`GenerateSineWave`, `ComputeStereoPan`,
`DistanceAttenuation` - all pure logic, no real audio device needed).
`ctest` 308/308 passing (bgfx build) / 305/305 (non-bgfx build).

**Phase 13 (block edit replication)**: closed the single most
consequential gap the Reality Audit above confirmed - block edits were
entirely unreplicated (each connected client mutated only its own local
`World`; a second client, or the server itself, never saw the change).
Added `BlockAction` (client->server request) and `BlockChange`
(server->all-clients broadcast) to `game::systems::protocol`, both
`ReliableOrdered`. `VoxelServer` validates every request (target chunk
loaded, break targets non-air, place targets air with a registered
`block_id`, target within `kMaxBlockActionRange` of the requester's own
server-known position - brief section 20's "never trust client data")
before applying it to its own `World` and broadcasting the result to
every connected client, itself included - no client mutates its own
`World` speculatively for a block edit (see DECISIONS.md). Item
pickup/consumption stays client-local and optimistic (fires at request-
send time, not at `BlockChange`-received time, since every client gets
every broadcast and can't tell whose edit it was from the message
alone) - a real, honestly-scoped simplification, not silently swept
under the rug (see NETWORKING.md "What's deferred" for the exact
edges: no refund on server rejection, no world-diff catch-up for a
client that joins after an edit already happened).

Found and fixed two real bugs while verifying this, not just adding new
code: (1) an initial implementation used `continue` inside the
place-block branch to skip re-duplicating logic, which would have
skipped the rest of that frame's loop body entirely (rendering, network
flush, frame counting) - caught by actually running it, not by
inspection, and fixed by restructuring to a proper if/else instead. (2)
Networked-mode breaking initially gave the player no item at all (the
pickup logic only existed in single-player's code path) - meaning a
networked player could break blocks forever but never place one, since
placing requires an item. Both were real, playtested bugs, not
hypothetical - exactly what brief section 29 ("regression rule") and
section 30 ("debugging": reproduce, find root cause, fix, add test/
verify, don't guess) call for.

Verified via a real three-process run (one `VoxelServer`, two
independent `VoxelClient`s - one acting via `LCU_VERIFY_BREAK_PLACE`,
one purely observing, given a large `LCU_MAX_FRAMES` budget since the
client loop is unthrottled and a passive observer otherwise exits
before a full server tick cycle elapses - a real methodology detail
worth recording for whoever reruns this): server logs `Applied
BlockAction from <addr>: (0,28,-1) 1 -> 0` then `(0,29,-1) 0 -> 1`; the
acting client logs the item pickup, the item consumption on place, and
`Applied server BlockChange` for both edits; the independent observer -
which never touched either block itself - logs the identical `Applied
server BlockChange` lines for both, confirming its `World` genuinely
converged with the other two processes rather than merely proving a
message decoded. Single-player mode re-verified byte-for-byte
unchanged via the same `LCU_VERIFY_BREAK_PLACE` hook.

10 new unit tests (`BlockActionBreakRoundTrips`,
`BlockActionPlaceRoundTripsWithBlockId`, and 8 more covering rejection/
truncation/wrong-type cases for both new messages). `ctest` 316/316
passing (bgfx build) / 313/313 (non-bgfx build).

The three-process test above also exposed a second real gap the same
session: a client connecting *after* an edit already happened never
learned about it (a one-shot broadcast only reaches whoever is already
connected). Fixed immediately, same pass: `VoxelServer` now keeps every
applied edit in order (`block_change_history`) and replays it in full
to a newly connecting client right after its `Welcome`. Verified via a
real run: a client breaks then places a block and disconnects: a
second client that connects *only after* both edits happened still
logs `Applied server BlockChange` for both, and the server logs
`Replayed 2 historical block change(s) to <addr>` - proving actual
catch-up, not just that the feature compiles.

**Phase 14 (chunk network streaming)**: closed the Reality Audit's other
confirmed gap - `engine/network::Connection` had no message
fragmentation, so a compressed chunk (a few KB) could never actually be
sent over the wire despite `chunk_serializer` already producing correct
bytes (Phase 3). Added two independent, reusable pieces: (1)
`lcu::network::fragment_payload`/`FragmentReassembler` - a generic,
caller-side split/rejoin layer (6-byte header: message_id/fragment_
index/fragment_count, big-endian) deliberately kept out of `Connection`/
`PacketHeader` itself, so the large majority of this codebase's traffic
that never needs it pays nothing; (2) `lcu::serialization::
serialize_chunk_to_bytes`/`deserialize_chunk_from_bytes`, extracted from
the existing file-based `save_chunk_to_file`/`load_chunk_from_file` (now
thin wrappers around them) so network streaming reuses the exact same,
already-tested compression/versioning/corruption logic instead of a
parallel copy. Both are unit-tested standalone (11 new fragmentation
tests, 4 new in-memory-serialization tests) before either was wired into
anything - `InMemoryBytesMatchFileBytes` pins byte-for-byte equivalence
with the pre-existing file path.

Built on top of those: `ChunkData` (server->client, logical - a chunk
snapshot, too large for one datagram) and `ChunkDataFragment` (the
actual wire message, one fragment of a fragmented `ChunkData`) in
`game::systems::protocol`. `VoxelServer`, right after `Welcome` and the
`block_change_history` replay, now sends a newly-connecting client every
chunk it has loaded (`World::loaded_chunk_coords()`, a new accessor)
fragmented and `ReliableOrdered`. `VoxelClient` reassembles via a
per-connection `FragmentReassembler`, decodes, and fully overwrites its
own (independently, deterministically generated - and until now, merely
*assumed* identical) local chunk with the server's authoritative one,
then fully relights and remeshes that chunk plus its six neighbors. This
makes the client's world actually *received from* the server, not just
coincidentally matching it - the real multiplayer-fundamentals gap brief
section 19 called out by name.

Verified via two real two-process runs, not just unit tests: a
`mobile_low`-profile (1-chunk world) run logs `Sent 1 chunk(s) (1
fragment(s))` server-side and `Applied server ChunkData for chunk (0, 1,
0)` client-side; a `desktop`-profile (36-chunk world) run logs `Sent 36
chunk(s) (36 fragment(s))` and exactly 36 matching `Applied server
ChunkData` lines client-side, zero warnings/errors either run -
confirming the common single-fragment-per-chunk path and that a full
loaded world (not just one chunk) streams and applies correctly end to
end.

15 new unit tests total for this phase (11 fragmentation +
4 in-memory serialization) plus 6 new `ReplicationProtocol` tests for
`ChunkData`/`ChunkDataFragment` encode/decode. `ctest` 337/337 passing
(bgfx build) / 334/334 (non-bgfx build).

Honestly scoped, not silently left half-done: this is a one-shot full
sync sent once on connect, not interest-managed by distance (unlike
`kInterestRadius` for entities) and not re-streamed as either side's
loaded-chunk set changes afterward - see NETWORKING.md "Chunk network
streaming" for the exact edge and DECISIONS.md for the reasoning.

**Phase 15 (server-side inventory)**: closed Phase 13's remaining
honest gap - item pickup/placement-cost was entirely client-local and
optimistic, with no server-side accounting and no refund on a rejected
`BlockAction`. `VoxelServer` now keeps a real, authoritative
`lcu::items::Inventory` per connected client (`ClientState::inventory`),
registers the same `game:stone` item `VoxelClient` does (so their
`ItemId`s coincide by construction), and does two new things inside
`handle_block_action`: placing `game:stone` is now rejected unless the
requester actually holds one server-side (a new validity condition
alongside the existing ones), and a successful break/place of it
adds/removes one from that client's server-side inventory. A new
`InventoryUpdate` message (server->one client, `ReliableOrdered`) is
sent after *every* `BlockAction` - accepted or rejected, at any
rejection point - carrying that client's current authoritative count.

`VoxelClient` keeps its existing optimistic pickup/consumption
(unchanged from Phase 13 - fires at request-send time, still the right
call for responsiveness, see DECISIONS.md), but now reconciles it
against every `InventoryUpdate` the same way `PlayerCorrection` already
reconciles predicted movement: compute the delta between the optimistic
guess and the server's authoritative count, `add_item`/`remove_item` to
close it, log only when they actually disagreed. This is the real fix
for the "no rejection feedback, no refund" gap Phase 13 honestly
flagged - a request the server ends up rejecting no longer leaves the
client's displayed count silently wrong.

Verified via a real two-process run (`LCU_VERIFY_BREAK_PLACE`): the
client's log shows the full round trip in both directions - after the
optimistic break-pickup (`Picked up 1 game:stone (inventory: 1)`) and
place-consume (`Requesting place ... (inventory: 0)`), two `Reconciled
inventory item 1 to authoritative count ...` lines appear (0 corrected
to 1 for the accepted break, then 1 corrected to 0 for the accepted
place), each immediately followed by the matching `Applied server
BlockChange` - proving the server's authoritative count and the
client's optimistic guess actually converged after each round trip, not
just that a message decoded.

6 new unit tests for `InventoryUpdate` encode/decode. `ctest` 342/342
passing (bgfx build) / 339/339 (non-bgfx build).

Honestly scoped: only `game:stone` is inventory-gated - there's no
general block-id-to-item-id mapping, so any other registered block
(mod content) still places without a server-side item check. No
persistence either - a server-side inventory lives only for the
connection's lifetime, lost on disconnect like every other per-client
server state today.

**Phase 16 (per-movement chunk streaming)**: closed Phase 14's
remaining honest gap - the connect-time `ChunkData` sync ran exactly
once, so a player wandering past their initial spawn area streamed
nothing new. `VoxelServer` now re-checks every connected client's
loaded-chunk range every tick (only when that client's current chunk
coordinate has actually changed since last checked, so a stationary
client costs nothing extra), loads any not-yet-loaded chunk in range
using the exact same generate-then-load logic the startup area already
uses, and broadcasts every newly-loaded chunk as `ChunkData` to *every*
connected client - not just whoever's movement triggered it. The
server's shared `World` deliberately only ever grows, never shrinks
(see DECISIONS.md "server-side chunk streaming never unloads") -
unloading based on one client's position could break a different
client still standing in that chunk, since `World` is one instance
shared across every connection; real per-client interest-scoped
unloading is deferred until something actually needs it.

`VoxelClient` runs the mirror-image local half unconditionally: the
same load-then-light-then-mesh sequence the initial spawn-area load
already runs, triggered only when the player's own chunk coordinate
changes. Also fixed a real gap in the Phase 14 `ChunkDataFragment`
handler that this phase's dynamics actually exercise for the first
time: a `ChunkData` for a coordinate the client hasn't locally streamed
to yet (their local trigger hasn't fired this frame, or a *different*
client's movement grew the server's world first) used to be silently
dropped ("isn't loaded locally, ignoring") - now the client creates a
real chunk slot via `world.load_chunk` before overwriting it, so no
legitimately-arriving `ChunkData` is ever lost.

Added a new headless verification hook, `LCU_VERIFY_MOVE_SECONDS`:
holds `MoveForward` for that many real (wall-clock) seconds - frame-
count-indexed hooks like `LCU_VERIFY_BREAK_PLACE` don't work for this,
since the client's main loop is unthrottled (see BUILD_STATUS.md) and
how far a fixed number of frames travels depends on real elapsed time,
not frame count.

Verified via two real multi-process runs. Two-process: a client held
`MoveForward` for 6 real seconds (enough to cross the 16-block chunk
boundary) - the server logs `Streamed 1 newly-loaded chunk(s) into
range (total 2 loaded)`, the client logs `Applied server ChunkData for
chunk (0, 1, -1)`, zero warnings/errors. Three-process: the same moving
client alongside a second, entirely stationary client that never sent
a single nonzero `PlayerInput` - that stationary client's own log shows
the identical `Applied server ChunkData for chunk (0, 1, -1)` line,
proving the broadcast reaches every connected client, not just the one
whose movement triggered it.

No new unit tests needed - this phase is orchestration logic in the two
executables (`server/main.cpp`/`client/main.cpp`) built entirely on
already-unit-tested primitives (`World`, `fragment_payload`,
`ChunkData` encode/decode), verified instead via the real multi-process
runs above, consistent with how the rest of `server/main.cpp`'s
handshake/broadcast logic is verified. `ctest` unchanged at 342/342
(bgfx) / 339/339 (non-bgfx).

Honestly scoped: still no interest-managed unloading (see above); a
client's own local streaming trigger and the server's are independent
and only usually agree, not literally synchronized - occasionally
redundant but never incorrect, since either order converges to the
same overwritten state.

**Phase 17 (surface/subsurface terrain content)**: closed a
long-flagged content gap - worldgen only ever placed one block type
below the terrain height, with no registered "game:grass"/"game:dirt"
anywhere real to place instead. `lcu::world::worldgen::
generate_terrain_chunk`'s signature changed from a single `solid_block`
parameter to `(surface_block, subsurface_block, stone_block)`: the
topmost solid layer is now `surface_block`, the next `kSubsurfaceDepth`
(3) layers are `subsurface_block`, everything deeper is `stone_block` -
a real grass-over-dirt-over-stone column, not a stub. `VoxelClient` and
`VoxelServer` both register `game:grass` and `game:dirt` block
definitions (identical fields, identical registration order right after
`game:stone` on both sides, so their `BlockId`s coincide by
construction - the same simplification already carried for item ids,
see DECISIONS.md) and pass them into `generate_terrain_chunk`.

Both new blocks are fully real content, not placeholders: real
`has_collision`/`is_transparent` definitions (so physics/collision and
greedy-mesh face culling work automatically - both are entirely
data-driven off `BlockRegistry`, never hardcoded by block id), real
network replication (a `ChunkData` snapshot's compressed bytes are
whatever block ids the chunk actually holds - no code path anywhere
assumes "only stone exists"), real break/place mutation through the
existing generic `BlockAction`/`BlockChange`/local-edit paths. The one
explicitly-scoped gap: only `game:stone` has an item mapping (Phase 5),
so breaking grass or dirt currently removes the block without granting
an item - an honest, bounded limitation (see Known Limitations), not a
hidden one.

4 unit tests updated and 2 new ones added (`SurfaceLayerIsExactlyOneBlockThickAtTheHeight`,
plus the renamed `ChunkFarBelowTerrainIsEntirelyStone`) for the new
layering behavior. Verified via a real single-player run (36-chunk
world generates and loads with no crash, `LCU_VERIFY_BREAK_PLACE`
round-trips cleanly) and a real two-process networked run (`Sent 1
chunk(s) (1 fragment(s))` / `Applied server ChunkData for chunk (0, 1,
0)` for a chunk now containing the layered grass/dirt/stone content,
zero warnings/errors) - confirming the new content flows through the
*existing* generation/meshing/collision/replication pipeline
unmodified, not a special case bolted on beside it. `ctest` 343/343
(bgfx) / 340/340 (non-bgfx), up from 342/342 / 339/339.

**Phase 18 (item mappings for grass/dirt)**: closed Phase 17's
immediate follow-up gap - `game:grass`/`game:dirt` were fully real
terrain content but breaking either granted no item, since Phase 5's
break->item logic was a single hardcoded `if (block == stone_id)`
check. `VoxelClient` now registers `game:grass`/`game:dirt` items
(identical 1:1 mapping to their block counterparts, matching
`game:stone`'s own convention - not a shared loot-table drop) and a new
`grant_item_for_broken_block` helper replaces the two previously-
duplicated stone-only blocks (networked and single-player break paths)
with a single lookup covering all three blocks. `VoxelServer` registers
the same two items, in the same order, purely to keep both sides'
`ItemId` spaces aligned - it doesn't track either in a per-client
`Inventory` yet (see Known Limitations/DECISIONS.md).

Deliberately narrow scope: placing grass/dirt isn't wired up (still no
hotbar/item-selection UI to choose what to place - `PlaceBlock` always
places `game:stone`), and server-side authoritative tracking (Phase
15's `InventoryUpdate`/rejection-correction machinery) still only
exists for `game:stone` - grass/dirt pickup is client-authoritative and
optimistic, same as `game:stone` was before Phase 15, honestly
documented rather than silently left half-done.

Verified via two real runs. Single-player (`LCU_VERIFY_BREAK_PLACE`):
the player spawns standing on a grass surface block (Phase 17's new
layering means the raycast straight down now hits grass, not stone),
and the log shows `Breaking block at world (0, 28, -1)` then `Picked up
1 game:grass (inventory: 1)` - a real, unforced exercise of the new
path, not a contrived scenario. Networked (two-process): the server
logs `Applied BlockAction from <addr>: (0,28,-1) 2 -> 0` (block id 2 =
`game:grass`), the client logs `Requesting break`, `Picked up 1
game:grass (inventory: 1)`, then `Applied server BlockChange ...
block_id=0` - confirming the item-mapping extension works identically
under server-authoritative block editing, not just in single-player.

No new unit tests - pure orchestration logic reusing already-tested
`ItemRegistry`/`Inventory` primitives, verified via the real runs
above. `ctest` unchanged at 343/343 (bgfx) / 340/340 (non-bgfx).

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 343/343 passing (this build dir is
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 340/340 passing
(`ChunkMeshUpload.*` only exists in the bgfx build, since it needs a
real bgfx context). Covers Log, QualityProfile, Vec3, Mat4, FrameStats,
InputState, TouchInputBackend, Chunk, ChunkStorage, ChunkCoord,
BlockRegistry, GreedyMesher, JobSystem,
ChunkMeshUpload, World, Worldgen, ChunkSerializer, Raycast,
PlayerPhysics/AABB, FirstPersonCamera, MovementInput, ItemRegistry,
Inventory, RecipeRegistry, ecs::Registry, block/sky light propagation,
AIWanderSystem, DayNightCycle, Sequence, PacketHeader, Connection,
UdpSocket, Address, LoopbackIntegration, FragmentPayload,
FragmentReassembler, PositionInterpolator,
PredictionBuffer, ReplicationProtocol (incl. BlockAction/BlockChange/
ChunkData/ChunkDataFragment/InventoryUpdate),
LuaState, EventBus,
RegistryBindings, ModLoader, GenerateSineWave, ComputeStereoPan,
DistanceAttenuation. JobSystem
additionally verified via 200 repeated `ctest`-suite runs and 50 runs
under ThreadSanitizer, zero failures/races - see `BUILDING.md` "Testing
under ThreadSanitizer" for the exact commands. GreedyMesher's triangle
winding is verified via a geometric cross-product check, not just
vertex counts. Real integration tests exist for networking
(`LoopbackIntegration.*`: two `Connection`s over real loopback UDP
sockets, one deliberately dropping the first real datagram sent) and,
outside the automated suite, a real two-process `VoxelClient`<->
`VoxelServer` movement/entity multiplayer run, a real *three*-process
run (one server, two independent clients) proving block edit replication
actually converges both clients' worlds, real two-process chunk-
streaming runs at both a 1-chunk and a 36-chunk scale, and a real
two-process run proving server-side inventory reconciliation actually
converges an optimistic client guess with the server's authoritative
count (see BUILD_STATUS.md); save/load is still unit-tested only, not
yet exercised
through a full server-save/client-load cycle since there's no
server-side world-save trigger yet, and `VoxelClient`/`VoxelServer`
don't call it either - see Known Limitations.

## Known Bugs

None currently tracked.

## Known Limitations

- ~~`VoxelClient` loads a static, fixed 36-chunk area around spawn once
  at startup ... rather than streaming from the player's actual
  position~~ **Fixed** (Phase 16, both `VoxelClient` and `VoxelServer`):
  both now load newly-in-range chunks as the player's own chunk column
  changes - see NETWORKING.md "Per-movement chunk streaming". Not via
  `World::update_streaming` itself, though: that method also *unloads*
  chunks outside range, which is the wrong shape for the server's
  single `World` instance shared across every connected client (see
  DECISIONS.md "server-side chunk streaming never unloads") - Phase 16
  instead hand-rolls the load-only half of the same radius logic
  directly in `server/main.cpp`/`client/main.cpp`. `World::update_streaming`
  itself, unload behavior included, is still real and still unit-tested,
  just not the function either executable's own streaming trigger calls.
- `World::update_streaming` itself still streams a 3D cube by Chebyshev
  distance, not the horizontal-disc-plus-bounded-vertical shape real
  worlds want - still true, and still nothing calls it (see above), so
  there's still no real caller to validate a disc-shaped version
  against.
- ~~Worldgen ... no surface/subsurface block variation (dirt/grass over
  stone) - single block type fills everything below the height~~
  **Fixed** (Phase 17, see "Last Completed Task" below and
  TASK_QUEUE.md): `generate_terrain_chunk` now places a real
  `game:grass` surface layer, `game:dirt` for the next few layers, and
  `game:stone` deeper. Still no climate/biome/caves/ores/structures/
  vegetation/decoration (brief section 21's later pipeline stages) -
  every column still uses the same three block ids regardless of
  position or depth beyond the fixed layering above.
- Chunk save/load (`engine/serialization::chunk_serializer`) is
  unit-tested in isolation but still not wired to any actual trigger in
  `VoxelClient` or `VoxelServer` (no "save world" command, no server
  persistence loop yet - Phase 7+). Brief section 80's slice 1 is closed
  in the sense that the save/load primitive exists and break/place
  mutates real in-memory chunk data; persisting those edits to disk from
  a live client/server session is still open.
- Look input is arrow keys, not mouse-look - SDL relative-mouse-mode
  plumbing doesn't exist yet (see `engine/platform/include/lcu/platform/
  input.h` and DECISIONS.md). A real, usable interim control scheme, not
  a placeholder that does nothing.
- Block-break's item drop is a direct, hardcoded 1:1 mapping (stone,
  grass, and dirt each to their own like-named item -
  `grant_item_for_broken_block` in `VoxelClient`, Phase 18), not a
  general loot-table/drop-rate system or a data-driven block->item
  table - each block/item pair is still an explicit `if` check, not
  configuration (see DECISIONS.md).
- `Inventory` has no UI - no hotbar rendering, no drag-drop, no way for
  a player to see or rearrange their items (needs `engine/ui`, a later
  phase). `player_inventory` in `VoxelClient` is currently only
  observable via log lines.
- `RecipeRegistry` has no crafting-grid caller anywhere - implemented
  and unit-tested standalone, same as `BlockRegistry`/`ItemRegistry`
  were before `VoxelClient` used them. No crafting table/UI exists yet
  to feed it a real grid.
- Three real block types now exist (`game:stone`/`game:grass`/
  `game:dirt`, Phase 17-18) with real terrain, collision, meshing,
  replication, and pickup-on-break - but placing a block still always
  places `game:stone` specifically; there's no hotbar/item-selection UI
  yet to choose what to place from a multi-item inventory.
- Lighting (`engine/lighting`) is single-chunk scoped - no light bleeds
  across a chunk boundary yet (a bright torch one block from a chunk
  edge won't light the neighboring chunk's cells, and sky light doesn't
  know whether the chunk above it is open sky or a solid roof). Sky
  light also doesn't spread laterally under overhangs (straight
  top-down column fill only). See DECISIONS.md.
- Computed light (`engine/lighting::Light`, held per-chunk in
  `VoxelClient`'s `chunk_light`) isn't consumed by anything visual yet -
  the chunk shader is still flat directional+ambient lit with no
  per-voxel light sampling. Only observed via a log line
  ("Sky light 5 blocks above spawn column: ...").
- `DayNightCycle` ticks and its `sky_light_scale()` is correct and
  tested, but nothing scales the actual rendered scene or
  `engine/lighting` data by it yet - also log-line-only for now.
- AI (`game::systems::update_ai_wander`) is wander-only: no player
  awareness, no pathfinding/obstacle avoidance (a wandering entity can
  walk into a wall and just stops making progress until its next
  target pick), no combat/interaction. The 3 spawned entities in
  `VoxelClient` have no visual representation (no mesh/model system for
  entities yet) - only logged positions.
- `VoxelClient`'s networked mode (`LCU_CONNECT_PORT`) only connects to
  `127.0.0.1` - there is no hostname/IP-string parser anywhere yet, and
  no in-game "connect to a server" UI. A real "join by address" flow is
  later work.
- Chunk *data* **is** now replicated over the network (Phase 14, see
  NETWORKING.md "Chunk network streaming") - `VoxelServer` sends a
  newly-connecting client a full, fragmented `ChunkData` snapshot of
  every chunk it has loaded, verified via real runs at both a 1-chunk
  and a 36-chunk scale, and (Phase 16, see NETWORKING.md "Per-movement
  chunk streaming") keeps streaming new chunks to every connected
  client as any of them wanders into unloaded territory, verified via
  real two- and three-process runs including a stationary client
  independently receiving another client's movement-triggered chunk.
  Still not interest-managed by distance in the sense of ever
  *unloading* anything - the server's shared `World` only ever grows
  (see DECISIONS.md). Block *edits*
  (breaking/placing) **are** also replicated (Phase 13, see
  NETWORKING.md "Block edit replication") - server-authoritative,
  broadcast to every connected client *and* replayed in full to any
  client that connects later (`block_change_history`, so a late joiner
  still catches up), both verified via real multi-process runs.
  Server-side inventory (Phase 15, see NETWORKING.md "Server-side
  inventory") now also exists for `game:stone` specifically - `VoxelServer`
  keeps an authoritative per-client `Inventory`, gates placing
  `game:stone` on actually holding one, and corrects a client's
  optimistic guess via a new `InventoryUpdate` message after every
  `BlockAction`, closing the "unrefunded on rejection" gap for that
  item. What none of these phases covers: any block/item besides
  `game:stone` isn't inventory-gated (no general block-id-to-item-id
  mapping), server-side inventory has no persistence across a
  disconnect, the shared `World`'s loaded-chunk set never shrinks back
  down (Phase 16 - see above), and the edit history itself is unbounded
  for the server process's lifetime rather than compacted against
  persisted state (see NETWORKING.md).
- `VoxelClient`'s networked mode logs the `Welcome` message's
  `world_seed` but doesn't actually use it for world generation - it
  still calls its own compile-time `kWorldSeed`, which by construction
  runs before any network round-trip could complete. Both are hardcoded
  to 1337 today so this isn't currently observable as a mismatch - see
  NETWORKING.md.
- `engine/network::Connection`'s reliable channels have no RTT
  estimation, congestion control, or max-resend cutoff - a fixed
  retransmit interval, forever. Correct (tested, including under real
  simulated loss) but not tuned for real-world network conditions - see
  NETWORKING.md/DECISIONS.md.
- `VoxelServer`'s connection model has no authentication - any UDP
  datagram from a new address is treated as a new client connection, no
  questions asked. A `PlayerInput`'s reported `dt` is trusted (clamped to
  a ceiling, but not otherwise validated) - a real anti-cheat concern for
  any public deployment, fine for this vertical slice. See NETWORKING.md.
- `Connection`'s reorder buffer and duplicate-detection set for the two
  reliable channels use raw `u16` sequence values in
  ordered/hash containers, whose numeric ordering doesn't account for
  wraparound the way `sequence_greater_than` does - only matters past
  65536 messages on a single channel of one connection, far beyond this
  vertical slice's traffic volume. See NETWORKING.md.
- Interest management (`VoxelServer`'s per-client `EntityState`
  distance filter) is real logic but never actually exercised excluding
  an entity in any verification run so far - this vertical slice's
  world and AI wander radius are small enough that everything stays
  within `kInterestRadius` of any client near spawn. See NETWORKING.md.
- Every multiplayer verification run so far has used exactly one
  connected client - multiple simultaneous clients are structurally
  supported (`VoxelServer` already keys everything by `Address` in a
  map) but untested together.
- bgfx's real GPU backend (Vulkan/GL/Metal/D3D) selection is untested —
  only the `Noop` headless fallback has been exercised, since this sandbox
  has no GPU/display.
- Mobile/Windows/macOS builds are untested from this Linux-only sandbox;
  `CMakePresets.json` presets exist for them but have not been exercised
  on their native toolchains.
- Input abstraction covers keyboard only (`KeyboardInputBackend`); no
  real mouse-look, gamepad or touch backend yet — camera look is driven
  by arrow keys (`LookUp/Down/Left/Right`, see `DECISIONS.md`) as an
  interim scheme until SDL relative-mouse-mode is wired up; gamepad/touch
  still have no consumer until mobile work starts (Phase 10).
- Debug overlay is a log line, not an on-screen overlay — needs
  `engine/ui`/text rendering (later phase) to actually draw on screen.
- No block state encoding yet (rotation/orientation/powered/etc., brief
  section 17) — deferred until a block actually needs it (see
  `DECISIONS.md`).
- No palette/run-length compression on chunk storage — flat array only,
  deferred until Phase 3 world streaming gives real memory numbers to
  profile (brief section 76).
- `JobSystem` scheduling is a single mutex + condition variable, not
  lock-free or work-stealing — correctness-first, unoptimized (see
  DECISIONS.md). Fine at today's job volumes (its own tests); revisit
  only if Phase 11 profiling shows contention actually matters once real
  workloads (chunk gen/meshing) exist.
- `JobSystem::cancel()` only prevents not-yet-started jobs from running;
  it cannot preempt a job already `Running`. No use case has needed
  preemption yet.
- Greedy meshing produces only the opaque layer; transparent/water
  layers exist structurally but are always empty (no transparent block
  registered anywhere, and transparent-vs-transparent face rules are
  deliberately unimplemented until one exists — see DECISIONS.md).
- No texture atlas/UV mapping validation — `MeshVertex.u`/`.v` are
  populated (quad-local, in block units) but nothing downstream
  consumes or checks them yet, since there's no atlas (Phase 12).
- Shader compilation (`LCU_BUILD_SHADER_TOOLS`) is opt-in and OFF by
  default — most builds/CI runs won't have a real draw call unless this
  is explicitly turned on, since it adds real build time (shaderc +
  glslang/SPIRV-Tools/SPIRV-Cross/Dawn-Tint).
- Chunk shaders have no texturing — flat lit color only. `MeshVertex.u`/
  `.v` are populated but unused downstream until a texture atlas exists
  (Phase 12).
- No visual verification of any rendering exists or can exist in this
  sandbox — every claim above about the draw call is about the API
  calls succeeding (valid handles, no crash, bgfx accepts the shader
  binaries), not about correct-looking output on a screen.
- Registries beyond `BlockRegistry`/`ItemRegistry` (`EntityRegistry`,
  `BiomeRegistry`, `StructureRegistry`, `SoundRegistry`,
  `CommandRegistry`) don't exist yet — Lua bindings only cover the two
  registries that were already real before Phase 9; the others are
  added when something actually needs them (brief section 98).
- `EventBus` only has one real event (`block_broken`) — no entity-
  spawned/player-joined/tick/... events exist since nothing in the
  engine fires them yet; add another `emit_<event>()` the same way once
  a second real event exists (see DECISIONS.md).
- `ModLoader` has no manifest/dependency/version format — a mod is just
  a directory name plus a fixed `init.lua` entry point. No load-order
  guarantees between mods beyond directory iteration order, no way for
  one mod to depend on another.
- Mod-registered block/item ids aren't synced over the network at all —
  `VoxelClient` and `VoxelServer` each load `mods/` independently and
  must agree by construction (same mods directory, same registration
  order) for ids to match; nothing detects or reports a mismatch. Same
  underlying gap as the "chunk data still isn't replicated" limitation
  above - both are "client/server must agree by construction, nothing
  syncs it" gaps.
- Lua sandboxing is standard-library-only (no `io`/`os`/`package`) — a
  mod script still runs with no CPU/memory/time limits (a mod with an
  infinite loop hangs the host process); resource-limiting a Lua VM is
  deferred until a real need (untrusted third-party mods, not just this
  repo's own `example_mod`) exists.
- `TouchInputBackend` has no real input source wired up — nothing in
  `VoxelClient` calls it yet, and there's no `SDL_EVENT_FINGER_*` pump
  in `engine/platform::Window` to feed it real touches from (this
  sandbox has no touchscreen to test that against anyway). The
  touch-to-`Action` mapping logic itself is real and unit tested; only
  the "read real hardware touches and call `update()`" wiring is
  missing.
- Button/drag-region layout in `TouchInputBackend` is a fixed set of
  normalized-screen-space rectangles, not configurable/skinnable, and
  has no on-screen visual representation (no `engine/ui` yet to draw
  the virtual joystick/buttons a player would actually see) — a player
  would currently be dragging/tapping blind.
- `QualityProfile` only controls chunk-load radius/vertical range so
  far — no render-distance-vs-loaded-distance split (both are the same
  number today), no texture/shadow/particle quality tiers, since none
  of those systems have more than one quality level to choose between
  yet (no texture atlas, no shadows, no particles).
- Android/iOS: only `CMakePresets.json` entries exist and were
  re-verified structurally reachable (`android-arm64` fails only at
  NDK detection, as expected without one installed). No Gradle project,
  no `AndroidManifest.xml`, no Xcode project/Info.plist, no mobile
  entry point wiring `SDL_main`/touch events on either platform — all
  deferred until there's an actual NDK/Xcode toolchain in the
  environment to build and run against (this sandbox is Linux-only, no
  GPU/display either way).
- `VoxelBenchmarks` numbers are from this sandbox's specific CPU (a
  4-core 2.1GHz container) — not representative of a real player's
  desktop, a real Android/iOS device, or this sandbox's own numbers
  under load from something else. They're a baseline to compare future
  changes against, not an absolute performance claim.
- No code was changed based on the Phase 11 benchmark numbers — nothing
  has shown a need to yet (every measured time is well under a 16ms
  frame budget at the tested scales). The benchmarks exist so a future
  optimization has something real to point at, not because anything is
  currently known to be slow.
- Benchmark coverage is one representative scenario per system (e.g.
  one fully-solid and one checkerboard chunk for meshing), not a sweep
  across chunk fill ratios/entity counts/world sizes — broader coverage
  is added if a specific scenario ever needs profiling, not
  speculatively now.
- Positional audio is pan (lateral angle to the listener) + linear
  distance falloff, not full HRTF/3D audio, occlusion, or reverb — a
  real, working first pass; more elaborate audio DSP is deferred until
  actual game content (multiple simultaneous sound sources, indoor/
  outdoor acoustics) gives a reason to tune it, not guessed at now.
- `AudioEngine` plays one-shot tones only — no looping/streaming
  playback, no per-sound volume/priority mixing beyond the stereo gain
  `play()` already takes, no music/ambience layer. Only two sounds
  exist anywhere (`break_sound`/`place_sound` in `VoxelClient`), both
  procedurally generated sine tones — no real sound-effect content
  pipeline (loading/authoring actual game audio) exists yet.
- `engine/ui::draw_debug_overlay` uses bgfx's built-in VGA-style
  debug-text character buffer, not a real font/texture-atlas text
  renderer — no texture atlas exists yet (brief section 12's content
  pipeline is separate, larger work with no player-facing text to
  justify it before this). Text is monospace ASCII only, fixed 8x16 (or
  8x8) character cells, no styling beyond the VGA 16-color palette.
- The on-screen touch-control legend has no interactive elements of its
  own (no buttons a mouse/gamepad can click) — it draws where
  `TouchInputBackend`'s real touch-button rects are, for a player to
  see, but a desktop/gamepad player can't interact with it as a menu;
  `engine/ui` is presentation-only so far, not an input-routing/focus
  system for non-touch input devices.
- No content pipeline exists for models/textures/sounds beyond what's
  procedurally generated in code (worldgen's terrain, the greedy
  mesher's geometry, `generate_sine_wave`'s tones) — "content" in brief
  section 96's Phase 12 sense (imported/authored game assets) is still
  entirely absent; every visual/audio element in this project today is
  generated, not loaded.

## Next Task

Per the master brief: the goal is a complete playable game, not a
completed checklist - work continues past the original 12-phase queue.
Next up, in priority order (brief section 10 - multiplayer fundamentals
before content/polish):

1. **Extend server-side inventory past `game:stone`**, closing Phase
   15's remaining honest gap (and now Phase 18's too - `game:grass`/
   `game:dirt` pickup is client-authoritative/optimistic only, same as
   `game:stone` was before Phase 15): there's still no general
   block-id-to-item-id mapping server-side, so no block besides
   `game:stone` is gated/tracked authoritatively, and inventory has no
   persistence across a disconnect/reconnect.
2. **Interest-scoped chunk unloading**, closing Phase 16's remaining
   honest gap: the server's shared `World` only ever grows (see
   DECISIONS.md "server-side chunk streaming never unloads") - a real
   long-running server needs a way to drop chunks nothing currently
   connected still needs, without breaking a client that's still
   standing in one another client abandoned.
3. **Placing grass/dirt**, closing Phase 18's other remaining gap:
   `PlaceBlock` still only ever places `game:stone` - there's no
   hotbar/item-selection UI yet to choose what to place from a multi-
   item inventory.
4. Continue down brief section 10's list after that: further
   content/gameplay systems (a real crafting-UI caller for the
   already-implemented `RecipeRegistry`, more block/item variety), then
   modding depth (a second real event beyond `block_broken`), then
   platform verification (Android/iOS on an actual toolchain), then
   performance work informed by `VoxelBenchmarks`' real numbers, then
   UI/audio polish.

Update state docs and commit after each, same discipline as every phase
before it - see "Resume Protocol" implicit throughout this file: read
this file, `TASK_QUEUE.md`, `BUILD_STATUS.md` first, then rebuild and
re-run the full test suite before trusting any of it, then continue.

## Current Architecture

See `ARCHITECTURE.md`. Layering is GAME -> VOXEL ENGINE ->
RENDERING ABSTRACTION -> BGFX -> platform backend. Server links only the
headless `Lcu::EngineCore` + `Lcu::Game`, never `Lcu::Engine`/
`Lcu::Platform` (SDL) or bgfx — enforced by the CMake target graph,
verified via `ldd`. `engine/rendering::Renderer` is the only place besides
`engine/platform` allowed to include bgfx/SDL headers.

## Important Decisions

See `DECISIONS.md` for full rationale. Headlines: bgfx for rendering, SDL3
for windowing/input, CMake FetchContent pinned to exact tags for all
dependencies, own minimal math library instead of GLM, server built as a
genuinely separate target with no GPU/window dependency, `BlockRegistry`
living in `engine/voxel` (not `engine/modding`) since chunk storage needs
it well before Phase 9's mod-loading exists, chunk storage as a flat
array with no palette compression or block-state packing until something
concrete needs either.
