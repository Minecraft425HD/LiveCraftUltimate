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
edit replication)** and **Phase 14 (chunk network streaming)** are done;
see "Reality Audit" and "Last Completed Task" below for what they cover
and what's next.

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

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 337/337 passing (this build dir is
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 334/334 passing
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
ChunkData/ChunkDataFragment),
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
actually converges both clients' worlds, and real two-process chunk-
streaming runs at both a 1-chunk and a 36-chunk scale (see
BUILD_STATUS.md); save/load is still unit-tested only, not yet exercised
through a full server-save/client-load cycle since there's no
server-side world-save trigger yet, and `VoxelClient`/`VoxelServer`
don't call it either - see Known Limitations.

## Known Bugs

None currently tracked.

## Known Limitations

- `VoxelClient` loads a static, fixed 36-chunk area around spawn once at
  startup (a 3x3 column of chunks, 4 chunks tall) rather than calling
  `World::update_streaming` every frame from the player's actual
  position - the player can walk outside the loaded area (movement/
  physics/raycast simply stop finding chunks there; `chunk_at`/
  `chunk_at_mutable` return null and break/place silently no-ops, logged
  at debug level). Wiring `update_streaming` into the per-frame loop is
  deferred, not forgotten - see DECISIONS.md.
- `World::update_streaming` itself still streams a 3D cube by Chebyshev
  distance, not the horizontal-disc-plus-bounded-vertical shape real
  worlds want. A camera/player now exists (Phase 4) but `VoxelClient`
  doesn't call `update_streaming` yet (see above), so there's still no
  real caller to validate a disc-shaped version against.
- Worldgen only implements continental+terrain (brief section 21's first
  two pipeline stages) - no climate/biome/caves/ores/structures/
  vegetation/decoration, and no surface/subsurface block variation
  (dirt/grass over stone) - single block type fills everything below
  the height.
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
- Block-break's item drop is a direct, hardcoded 1:1 mapping
  (`stone block -> stone item`) written into `VoxelClient` itself, not a
  general loot-table/drop-rate system - there's only one droppable block
  type to motivate one, so a real table is deferred until more than one
  exists (see DECISIONS.md).
- `Inventory` has no UI - no hotbar rendering, no drag-drop, no way for
  a player to see or rearrange their items (needs `engine/ui`, a later
  phase). `player_inventory` in `VoxelClient` is currently only
  observable via log lines.
- `RecipeRegistry` has no crafting-grid caller anywhere - implemented
  and unit-tested standalone, same as `BlockRegistry`/`ItemRegistry`
  were before `VoxelClient` used them. No crafting table/UI exists yet
  to feed it a real grid.
- Only one block type (`game:stone`) exists anywhere outside unit tests;
  placing a block always places stone.
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
  and a 36-chunk scale. Still a one-shot sync on connect only, not
  interest-managed by distance and not re-streamed as either side's
  loaded-chunk set changes afterward - a client and server that both
  keep streaming new chunks in as a player roams still rely on
  independently generating matching deterministic terrain for anything
  sent *after* that initial connect-time sync. Block *edits*
  (breaking/placing) **are** also replicated (Phase 13, see
  NETWORKING.md "Block edit replication") - server-authoritative,
  broadcast to every connected client *and* replayed in full to any
  client that connects later (`block_change_history`, so a late joiner
  still catches up), both verified via real multi-process runs. What
  neither phase covers: a server-side inventory (item pickup/cost is
  still client-local and optimistic, unrefunded if the server rejects
  the request), and the edit history itself is unbounded for the server
  process's lifetime rather than compacted against persisted state (see
  NETWORKING.md).
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

1. **Server-side inventory**, closing Phase 13's remaining honest gap
   (item pickup/placement-cost is still client-local and optimistic,
   unrefunded on a rejected `BlockAction`) - needed before multiplayer
   item economy (crafting, drops, trading) can be real rather than
   per-client fiction.
2. **Per-movement chunk streaming**, closing Phase 14's remaining honest
   gap: the initial connect-time `ChunkData` sync is real and verified,
   but a client's/server's loaded-chunk set can still change afterward
   (`World::update_streaming` as a player moves) with nothing re-syncing
   it - only the connect-time snapshot is covered today.
3. Continue down brief section 10's list after that: content/gameplay
   systems (more block/item types, a real crafting-UI caller for the
   already-implemented `RecipeRegistry`), then modding depth (a second
   real event beyond `block_broken`), then platform verification
   (Android/iOS on an actual toolchain), then performance work informed
   by `VoxelBenchmarks`' real numbers, then UI/audio polish.

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
