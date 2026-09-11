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
**Phase 17 (surface/subsurface terrain content)**, **Phase 18 (item
mappings for grass/dirt)**, **Phase 19 (server-side inventory
extended past game:stone)**, **Phase 20 (interest-scoped chunk
unloading, real chunk persistence, disconnect detection)**, **Phase 21
(hotbar item selection for placing grass/dirt)**, **Phase 22
(data-driven block-id-to-item-id mapping)**, **Phase 23
(quick-craft: RecipeRegistry's first real caller)**, **Phase 24
(item_crafted: EventBus's second real event)**, **Phase 25
(macOS build audit)**, **Phase 26 (visible terrain: per-block/
per-face colors + procedural shader noise)**, **Phase 27 (skybox +
sun/moon)**, **Phase 28 (renderer consumes real per-voxel light)**,
**Phase 29 (WorldLight data structure)**, **Phase 30 (sky-light
cross-chunk propagation)**, **Phase 31 (block-light cross-chunk
propagation)**, **Phase 32 (boundary buffer, skipped - see below)**,
**Phase 33 (VoxelClient integration + smooth lighting)**,
**Phase 34 (torch block + lighting benchmarks)**, **Phase 35
(chunk unload marks neighbors dirty)**, **Phase 36 (entity boxes +
extended debug overlay)**, **Phase 37 (sea level at y=0 + water
block/rendering)**, **Phase 38 (continental/mountain terrain)**,
**Phase 39 (biomes)**, **Phase 40 (caves + ores)**, **Phase 41
(vegetation)**, **Phase 42 (documentation update)**, **Phase 43
(input overhaul + mouse look + Minecraft-parity defaults)**,
**Phase 44 (2D UI framework)**, **Phase 45 (persistent options)**,
**Phase 46 (menu framework: pause/options/controls)**, **Phase 47
(HUD overhaul: hotbar + health/hunger bars + F-toggles)**, **Phase
48 (block highlight + hold-to-break + hand)**, **Phase 49
(inventory screen + drag/drop + crafting grid)**, **Phase 50
(item entities + crafting table)**, **Phase 51 (health, hunger,
fall damage, respawn)**, **Phase 52 (documentation)**, **Phase 53
(texture-atlas pipeline - infrastructure only, no real textures yet)**,
**Phase 54 (17 real procedurally-generated MC-style textures,
not yet wired to any block/item at that point)**,
**Phase 55 (blocks now reference real Phase-54 textures per-face)**,
**Phase 56 (items - inventory, hotbar, hand, dropped items - now
reference the same atlas, reusing a block's own texture where an item
represents a block)**, and **Phase 57 (a real, own-design procedurally-
generated bitmap-font atlas + `engine::ui::TextRenderer`, now the
default way HUD/menu/inventory/workbench labels draw, with bgfx's old
debug-text buffer kept as a real `LCU_LEGACY_DEBUG_TEXT=1` fallback)**
are done - this closed out the fourth user-directed program (Phases
53-57: texture atlas, procedural MC-style textures, blocks/items on the
atlas, a bitmap font + real text renderer) in full. A fifth
user-directed program (Phases 58-66: player model, visible NPCs, crack
textures, transparent water, a real skin system, and full farming) is
now in progress - **Phase 58 (a real Minecraft-proportioned 6-box
Steve-like character model, a real procedurally-generated 64x64 skin in
the actual MC UV layout, a first-person item-textured arm box replacing
the old flat hand icon, and a real 3-way F5 perspective cycle)** and
**Phase 59 (the 3 real `AIWander` entities now render as real visible
Steve-like NPCs via `submit_character_model` - Phase 58's own body
rendering, extracted and reused - with real wander-direction facing and
walk/idle animation; debug wireframe boxes became a real toggle,
default off)** are done - see TASK_QUEUE.md for per-phase detail as
each of the remaining
7 phases lands. See
"Reality Audit" and
"Last Completed Task" below for what they
cover and what's next. Phases 26-42 (visible terrain colors, skybox,
cross-chunk global lighting with real performance constraints,
procedural terrain with sea level at y=0, water, biomes, caves/ores,
vegetation, and a documentation pass) closed out that first
user-directed program; a second one (Phases 43-46: rebindable input,
a 2D UI framework, persistent options, and a menu/options/controls
screen) is now in progress - see TASK_QUEUE.md for per-phase detail as
each lands.

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

**Phase 19 (server-side inventory extended past `game:stone`)**:
closed Phase 15's remaining honest gap - the server's authoritative
per-client `Inventory` only ever tracked `game:stone`, so Phase 18's
new grass/dirt pickup was entirely client-optimistic with nothing to
correct it. `VoxelServer` now registers `game:grass`/`game:dirt` items
(capturing their `ItemId`s, previously discarded) and generalizes the
break/place bookkeeping through a new `item_for_block` lookup (the same
direct 1:1 mapping `VoxelClient`'s own `grant_item_for_broken_block`
already used) instead of a single hardcoded `stone_id` check - covers
all three tracked items identically now. `send_inventory_update` became
`send_inventory_updates`: after every `BlockAction`, the server sends
one `InventoryUpdate` per tracked item (`tracked_items = {stone, grass,
dirt}`), not just whichever the request happened to touch, so a stale
guess for an *unrelated* tracked item also eventually corrects. The
place-validity check generalized the same way: any item-backed
`block_id`, not just `stone_id` specifically, is gated on the requester
actually holding one.

`VoxelClient` needed **no changes** - its `InventoryUpdate` handler was
already generic (keyed by whatever `item_id` arrives), so it started
correctly reconciling grass/dirt the moment the server started sending
those updates.

Verified via a real two-process run (`LCU_VERIFY_BREAK_PLACE`): the
player spawns standing on a grass block (Phase 17's layering), and the
full round trip converges cleanly - server logs `Applied BlockAction
...: (0,28,-1) 2 -> 0`, client logs `Requesting break`, `Picked up 1
game:grass (inventory: 1)`, `Applied server BlockChange ...
block_id=0`, zero warnings/errors - confirming `item_for_block`'s
grass mapping, the server's `add_item` call, and the new 3-item
`send_inventory_updates` broadcast all execute correctly end to end.
The disagree-then-correct path itself (an explicit `Reconciled
inventory item ...` log line) was already proven for this identical,
now-generalized mechanism in Phase 15's stone-specific test - not
re-demonstrated here since nothing about *how* reconciliation works
changed, only *which* items it covers.

No new unit tests - pure generalization of already-tested
`ItemRegistry`/`Inventory` orchestration, verified via the real run
above. `ctest` unchanged at 343/343 (bgfx) / 340/340 (non-bgfx).

Honestly scoped: still only stone/grass/dirt are inventory-backed (no
general, data-driven block-id-to-item-id mapping - three explicit `if`
checks in `item_for_block`, not configuration); no persistence across a
disconnect/reconnect; placing still only ever places `game:stone` (no
hotbar/item-selection UI exists to place anything else).

**Phase 20 (interest-scoped chunk unloading, real chunk persistence,
disconnect detection)**: closed Phase 16's remaining honest gap - the
server's shared `World` could stream new chunks in per-movement but
could never unload anything, and UDP's connectionless nature meant a
departed client's `ClientState` lived forever, permanently inflating
the union of "chunks someone might need". Three tightly-coupled pieces,
closed together because the first genuinely required the second and
both benefited from the third being real too:

1. **Real disconnect detection**: every `ClientState` now tracks
   `last_packet_time`, updated on every received packet; a per-tick
   sweep erases (and logs) any client idle past a new
   `kClientTimeoutSeconds = 5.0f` constant.
2. **Interest-scoped unloading**: a new `compute_interest_set` lambda
   gives each client a real `interest_set` (every chunk coord within
   load radius of its last streamed center); after any tick where a
   client moved, connected, or was pruned, the server unions every
   remaining client's interest set and unloads whatever currently-
   loaded chunk nobody needs.
3. **Real chunk persistence wired to its first real trigger**: before
   unloading, the chunk is saved via the already-existing, already-
   tested `lcu::serialization::save_chunk_to_file` (Phase 3's
   primitive, never previously called from a real code path); if a
   client's interest later returns to that coord, `load_chunk_from_file`
   is tried before falling back to regenerating it - regenerating an
   edited-then-evicted chunk would silently revert the edit, a genuine
   correctness bug, not just a missed optimization.

A design-time bug was caught before ever building: pre-setting a
freshly-connected client's `last_streamed_center` to its own spawn
center made the movement-triggered streaming loop treat "just
connected" as "no change since last tick, skip" - which would silently
skip the real load-or-reload path for a client's own spawn-adjacent
chunks whenever a previous client's departure had evicted them. Fixed
by making the field `std::optional<ChunkCoord>` (default unset,
compares unequal to any real coord) instead of pre-populating it.

Verified via real multi-process runs, not mocks: a disconnect-timeout
run (`"Client 127.0.0.1:42566 timed out after 5.0s of silence,
disconnecting"` at essentially exactly 5s); an isolated unload run
(`"Saved chunk (x,y,1) to disk before unloading"` x12, then `"Unloaded
12 chunk(s) no connected client still needs (total 36 loaded)"`); and a
chained run where a second, freshly-connecting client receives
`ChunkData` for the exact same 12 coordinates the first run evicted -
direct proof those chunks were reloaded from disk, not silently
regenerated or lost. One real, if narrow, networking behavior surfaced
while stress-testing (a suspected `UnreliableSequenced` `u16` sequence-
wraparound under extreme sustained packet volume) and is documented,
not fixed, in NETWORKING.md - root cause unconfirmed, out of this
phase's scope.

No new unit tests - this phase composes already-tested primitives
(`World`, `lcu::serialization::{save,load}_chunk_to_file`) under new
server-side orchestration, exercised by the real runs above. `ctest`
unchanged at 343/343 (bgfx) / 340/340 (non-bgfx).

Honestly scoped: persistence is session-scoped (chunks save under
`<world>/chunks/`, so a fresh server process on the same world
directory would pick them up, but full cross-restart persistence as a
verified *product feature* hasn't been separately demonstrated);
`kClientTimeoutSeconds` is a placeholder value, not tuned against real
latency/jitter data; the client's own `World` still never unloads
(only the server's shared one does - the client only ever tracks one
player's interest, so this hasn't been a correctness problem); the
sequence-wraparound finding above remains unconfirmed and unfixed. See
NETWORKING.md "Interest-scoped chunk unloading, real chunk persistence,
and disconnect detection" for the full writeup.

**Phase 21 (hotbar item selection for placing grass/dirt)**: closed
Phase 18/19's remaining honest gap - `game:grass`/`game:dirt` had real
item mappings on both break and (server-side, Phase 19) place
validation, but `PlaceBlock` itself still only ever requested
`game:stone`, since there was no way for a player to choose otherwise.

A new `Action::CycleHotbar` (`engine/platform::Action`), bound to `R`
on keyboard and a new "ITEM" touch button, follows the exact pattern
every other action already uses. `VoxelClient` gained a
`placeable_items` list (stone/grass/dirt, same order as every other
block/item list in the file) and a plain `selected_placeable_index`,
cycled on an edge-detected `CycleHotbar` press. `PlaceBlock`'s handling
(both single-player and networked branches) now reads
`placeable_items[selected_placeable_index]` instead of the hardcoded
`stone_id`/`stone_item_id` - no protocol change needed, since
`BlockAction::block_id` was already a plain field and the server's
`item_for_block`/place-validity gate already generalized to any
item-backed block back in Phase 19.

Deliberately minimal, not a graphical hotbar: no on-screen slot
rendering or selection highlight exists yet (needs `engine/ui`'s
texture-atlas work, same gap already noted for the debug overlay) -
the current selection is only observable via a log line (`"Selected
placeable item: game:grass"`), same text-first-pass pattern as
Phase 6's lighting/day-night systems before them.

Verified via a real single-player run (the existing
`LCU_VERIFY_BREAK_PLACE` hook extended with a `kVerifyCycleHotbarFrame`
between break and place): `"Selected placeable item: game:grass"` then
`"Placing game:grass at world (0, 28, -1) (inventory: 0)"` - the exact
position the grass block was broken from. Verified via a real
two-process networked run: server logs `"Applied BlockAction from
<addr>: (0,29,-1) 0 -> 2"` (block id 2 = `game:grass`, not the old
hardcoded stone id 1), client logs `"Requesting place game:grass..."`
then `"Applied server BlockChange at world (0, 29, -1): block_id=2"` -
both sides converge on grass, not stone, confirming the server-
authoritative path works for a client-selected block for the first
time in a real run.

No new unit tests - `touch_input_test.cpp`'s `Action::Count`-driven
loop and every other `Action`-keyed test already generalize to the new
enumerator automatically. `ctest` unchanged at 343/343 (bgfx) / 340/340
(non-bgfx).

**Phase 22 (data-driven block-id-to-item-id mapping)**: closed Phase
19's remaining honest gap - `item_for_block` (server) and
`grant_item_for_broken_block` (client) were both still three explicit
`if (block_id == X)` checks, one per block, hand-duplicated between
the two files and needing a matching edit in both for every new
item-backed block.

New `game::items::BlockItemMapping` (`game/items/` - a new
gameplay-layer module alongside `game/components`/`game/systems`,
matching the GAME -> ENGINE layering `ARCHITECTURE.md` already
documents, not an engine-level primitive since it's gameplay content
wiring a block registry to an item registry): a small
`register_pair(block_id, item_id)`/`item_for_block(block_id)` table,
`kNoItemId` for anything unregistered. No new engine-level dependency
needed - `Lcu::EngineCore` already transitively provides both
`lcu::voxel::BlockId` and `lcu::items::ItemId` to `game/`.

`VoxelClient`'s `grant_item_for_broken_block` and `VoxelServer`'s
`item_for_block` both now populate the same table shape (three
`register_pair` calls right after each block/item pair is registered)
and do a single lookup instead of their own hardcoded chain. Client
and server still each maintain their own separate table populated from
their own separate content registration - no cross-process sync of the
mapping itself, the same "must agree by construction" simplification
every other piece of shared content in this project already carries
(see NETWORKING.md) - but the *logic shape* is now identical, and
adding a fourth block/item pair from here on is one `register_pair`
call per side, not a new `if` branch in each.

4 new unit tests (`BlockItemMapping.*`): unmapped block returns
`kNoItemId`, a registered pair round-trips, re-registering a block id
overwrites its previous mapping, and multiple blocks can map to the
same item (proving the table isn't accidentally 1:1-only, even though
today's actual content happens to be). `ctest` now 344/344 (non-bgfx,
up from 340) / 347/347 (bgfx, up from 343).

Verified via a real single-player run and a real two-process networked
run - both reproduce the exact same log lines Phase 21's own
verification produced ("Picked up 1 game:grass...", "Applied server
BlockChange at world (0, 29, -1): block_id=2"), confirming this was a
true refactor (changed *how* the lookup works) with zero behavior
change (*what* it returns is identical).

Honestly scoped: not loaded from an external data file - "data-driven"
here means a real runtime table populated by code, matching how
`BlockRegistry`/`ItemRegistry` themselves are "datadriven" (in-code
registration, not external config), not a JSON/config-file content
pipeline (a separate, larger piece of future work if modding ever
needs one).

**Phase 23 (quick-craft: RecipeRegistry's first real caller)**: closed
a gap honestly flagged since Phase 5 - `RecipeRegistry` was
implemented and unit tested but had zero real callers anywhere ("no
crafting-grid caller exists yet," restated unchanged through every
later phase's Known Limitations).

First crafted-only content: `game:compost`, a new item with no
corresponding block, obtainable only by crafting. One real shapeless
recipe: `1x game:grass + 1x game:dirt -> 1x game:compost`, on a new
`lcu::items::RecipeRegistry` instance in `VoxelClient`. New
`Action::Craft` (bound to `C`/a new "CRAFT" touch button, same pattern
every other action uses): on an edge-detected press, builds a query
grid from one of each *distinct* item type currently held (dedup by
inventory-slot scan), calls `RecipeRegistry::find_match` for real, and
on a match consumes exactly the grid's contents (which equals the
matched recipe's ingredients exactly, since shapeless matching
requires an exact multiset match) and grants the result. Logs "No
recipe matches your held items" on no match - a real rejection path.
Purely client-side, single-player and networked alike - crafting never
touches the `World` or needs server validation (same
client-authoritative precedent as item pickup, see DECISIONS.md), so
it needed zero protocol/server changes.

A real bug was caught and fixed by real networked verification, not by
reasoning alone: the verification hook's first version gated its two
block breaks by frame count (mirroring `LCU_VERIFY_BREAK_PLACE`'s
style) - this worked single-player but broke networked mode, since the
unthrottled client loop ran hundreds of frames (confirmed: even 200
wasn't enough) before the first break's `BlockChange` round-tripped
back, so the second break's raycast still saw the old, unbroken grass
block and re-requested breaking the *same* position - optimistically
double-granting the item client-side before the server's rejection and
inventory correction arrived. Fixed by switching the hook to a
wall-clock-gated state machine, the same pattern `LCU_VERIFY_MOVE_SECONDS`
(Phase 16) already established for this exact class of problem.
Confirmed fixed via a second real networked run: server applies both
breaks at their correct distinct positions, zero warnings, craft
succeeds, reject path still fires.

No new unit tests - pure orchestration of already-tested
`RecipeRegistry`/`Inventory`/`ItemRegistry` primitives, exercised by
the real single-player and two-process networked runs above. `ctest`
unchanged at 347/347 (bgfx) / 344/344 (non-bgfx).

Honestly scoped: quick-craft's auto-built grid only correctly
represents a recipe needing exactly one of each distinct ingredient
type - not a stand-in for a real grid that could hold more than one of
the same item in different cells; no graphical crafting-grid UI exists
(a Craft press is the entire interaction, feedback is a log line);
shaped-recipe matching still has zero real caller (only shapeless is
exercised by this design).

**Phase 24 (item_crafted: EventBus's second real event)**: closed a
gap flagged since Phase 9 - `EventBus` only ever had one real event
(`block_broken`), with its own doc comment stating "add another
`emit_<event>()` the same way once a second real event exists to
validate the shape against." Phase 23's quick-craft gave the project
its first genuinely new gameplay moment since Phase 13 worth exposing
to mods.

New `EventBus::emit_item_crafted(item_id, count)`, same
error-isolated-per-subscriber pattern `emit_block_broken` already
established. `VoxelClient`'s quick-craft handler calls it right after
a successful `find_match` + item grant. Purely client-side, like
crafting itself - `VoxelServer` never calls it, but still exposes
`lcu.subscribe("item_crafted", ...)` since a mod script is shared
between both hosts and must load identically on either (the same
reason `EventBus` was already constructed server-side even before any
server-fired event existed).

`example_mod/init.lua` now subscribes to both events, proving the real
register -> load -> subscribe -> emit loop generalizes beyond
`block_broken` alone, not just that a second typed emit method
compiles - verified via a real run (`LCU_VERIFY_CRAFT`) showing
`[example_mod] item_crafted #1: 1 x item id 4` fire at the exact
moment compost is crafted, and a real server run confirming the same
mod file still loads cleanly there (the new subscription just never
fires on that host).

Fixed a stale comment along the way: `server/main.cpp` claimed "the
server never calls emit_block_broken() itself," which Phase 13 made
false (block edits are server-authoritative, so the server's own break
handling is where `emit_block_broken` actually fires) - noticed while
touching the same code for `item_crafted`'s opposite case.

3 new unit tests (`EventBus.EmitItemCrafted*`,
`EventBus.BlockBrokenAndItemCraftedSubscribersAreTrackedIndependently`).
`ctest` 350/350 (bgfx, up from 347) / 347/347 (non-bgfx, up from 344).

Honestly scoped: still no manifest/dependency/version format for
`ModLoader`; mod-registered ids still aren't synced over the network;
only two real events now - both client-triggered content moments,
nothing server-side fires one yet; a third event still needs a genuine
third engine-side moment to justify it, not speculative expansion.

**Phase 26 (visible terrain: per-block/per-face colors + procedural
shader noise)**: the chunk shader was still Phase 21's placeholder -
one flat gray-blue material, ignoring block identity entirely.
`BlockDefinition` gained `color`/`side_color`/`bottom_color`;
`mesh_chunk_greedy` selects the right one per face at mesh time (it
already knows the axis/facing direction, so this is real data
selection, not a shader-side special case for e.g. "grass"); grass
gets the classic green-top/brown-sides treatment this way.
`client/shaders/{vs_chunk,fs_chunk}.sc` were rewritten to carry and
use that color, adding a subtle deterministic-per-voxel hash-noise
multiply and a small top-face light lift. A real bug surfaced and was
fixed along the way: `engine/voxel` used `math::Vec3` without
`LcuVoxel` ever declaring a link to `Lcu::Math` - previously silent
because every real consumer got it transitively some other way; adding
a `Vec3` field to `BlockDefinition` broke `block_registry.cpp`'s own
compilation, exposing it.

Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` build (the only way to
actually compile `.sc` files through bgfx's `shaderc`, not just the
C++ side): `"Chunk shader program valid=true"` - the new `Color0`
attribute, varying wiring, and fragment shader logic all link
correctly through the real bgfx pipeline. A real headless run under
that build confirms `LCU_VERIFY_BREAK_PLACE` still works with the real
program loaded, zero regressions. 2 new unit tests for the real,
headlessly-testable part (per-face color selection); the shader's own
noise pattern can only be confirmed by actually looking at it - see
Known Limitations. `ctest` 352/352 (bgfx, up from 350) / 349/349
(non-bgfx, up from 347).

Honestly scoped: **what a real GPU/display shows is still NOT VERIFIED
— ENVIRONMENT LIMITATION** - "valid=true" proves the shader compiles
and links, not that it looks right; no texture atlas exists yet (Phase
12); `game:water`/`game:sand` colors are deferred to Phase 37/39 since
those blocks don't exist yet.

**Phase 27 (skybox + sun/moon)**: until now the sky was a single flat
clear color, and there was no sun/moon at all. `Renderer::begin_frame`
now takes an explicit `Vec3` clear color (was a pre-packed `u32`) and
interpolates it every frame from the existing `DayNightCycle::sky_
light_scale()` - night (0.02, 0.03, 0.08) to day (0.45, 0.65, 0.95) -
reusing the one real time signal rather than adding a second animation
clock. A new `Renderer::submit_billboard()` draws a camera-facing sun/
moon quad (built from the camera's own `right()`/`cross(right,
forward)` basis) using bgfx transient buffers, into a dedicated bgfx
view (`kSkyViewId`) ordered via `bgfx::setViewOrder` to execute
*before* the terrain view - terrain's own real depth test against the
sky view's cleared depth buffer then naturally occludes the sky quad
wherever a block is actually in front of it, satisfying "own view,
depth test off" for the sky quad itself while still getting correct
occlusion. Direction math was extracted into a new pure `game::
systems::sun_direction(time_of_day)` next to `DayNightCycle` (moon is
always exactly opposite) instead of staying inlined in `client/
main.cpp`, specifically so it's headlessly unit-testable. Dedicated
minimal `vs_sky.sc`/`fs_sky.sc` (position + flat color, no lighting/
noise - the sun/moon IS a light source, not something lit by one).

Two real bugs found and fixed: (1) `bgfx::allocTransientVertexBuffer`/
`allocTransientIndexBuffer` return `void` in this bgfx version, not
`bool` - fixed via `getAvailTransientVertexBuffer`/`getAvailTransient
IndexBuffer` pre-checks; (2) proactively avoided a repeat of Phase 26's
`Lcu::Math` link bug by adding `Lcu::Math` to `LcuGame`'s link
libraries up front, since `sun_direction` now returns a real `Vec3`
from that library.

6 new unit tests (`SunDirection.*`): all four phase points (dawn/noon/
dusk/midnight), unit-length-in-the-xy-plane, and moon-always-opposite-
sun. Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` build: `"Sky
shader program valid=true"` alongside the existing chunk shader, plus
a real headless `LCU_VERIFY_BREAK_PLACE` run under that build showing
zero regressions. `ctest` 358/358 (bgfx, up from 352) / 355/355
(non-bgfx, up from 349).

Scope: stars at night were explicitly listed as optional in the brief
("Sterne bei Nacht optional") and are deliberately deferred - a
scoped-out feature, not a missing one.

Honestly scoped: **what a real GPU/display actually shows (sky color,
sun/moon visibility, and the occlusion behavior described above) is
still NOT VERIFIED — ENVIRONMENT LIMITATION**.

**Phase 28 (renderer consumes real per-voxel light)**: until now the
chunk shader lit every face with a fixed fake directional light,
completely unrelated to the real per-chunk sky/block light
`engine/lighting` had already been computing since Phase 6 - the actual
light data existed but nothing rendered it. `MeshVertex` gained a
packed `u8 light` field (low nibble sky, high nibble block - the exact
`LightStorage` packing, one byte total per the brief); `mesh_chunk_
greedy` now reads real light from the air cell each face is exposed to
(not the solid block's own cell, which propagation never touches) and
packs it per vertex once, at mesh-build time - never recomputed per
frame. `mesh_chunk_greedy` is templated on a duck-typed `LightStorageT`
rather than including a concrete `lcu::lighting` header, since
`engine/lighting` already depends on `engine/voxel` (the reverse
`#include` would be a circular target dependency) - a light-less
two-argument overload (an always-full-bright stand-in) keeps every
existing call site (tests, `tools/benchmark`) unchanged; only
`client/main.cpp`'s real remesh path passes its actual per-chunk light.
Merging now also requires equal light, not just equal block id/facing,
so a real lighting gradient across a surface no longer gets flattened
into one arbitrary quad-wide brightness by the same optimization that
reduces triangle count. `client/shaders/{vs_chunk,fs_chunk}.sc` were
rewritten: the old fake directional light is gone (it would have
double-counted daylight against real per-voxel light and never actually
darkened at night), replaced by `final = color * (sky * u_skyLightScale
+ block) / 15.0` using a new `u_skyLightScale` uniform set once per
draw call from `DayNightCycle::sky_light_scale()` - the same real time
signal Phase 27's skybox already reuses.

A real, previously-nonexistent bug risk was found and fixed while
wiring the vertex layout: `MeshVertex` had never before ended in a
byte-sized field, so the compiler now pads its total size up to a
4-byte multiple - bytes `bgfx::VertexLayout`'s tightly-packed `.add()`
sum doesn't know about. Left alone, every vertex after the first would
have read from the wrong GPU-buffer offset (silent corruption, not a
crash). Fixed via `layout.skip(sizeof(MeshVertex) - layout.getStride())`
plus an `LCU_ASSERT` verifying the two stay in sync - which did execute
against real 36-chunk production data in this phase's verification run
without firing.

4 new unit tests. Verified via a real `LCU_BUILD_SHADER_TOOLS=ON`
build: `"Chunk shader program valid=true"`, plus a real headless
`LCU_VERIFY_BREAK_PLACE` run under that build with real per-voxel light
flowing through the real 36-chunk world, zero regressions. `ctest`
362/362 (bgfx, up from 358) / 359/359 (non-bgfx, up from 355).

Honestly scoped: **what real per-voxel lighting actually looks like on
a real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**;
cross-chunk light doesn't exist yet (a chunk-boundary face
unconditionally defaults to full-bright, honestly, not guessed) - see
Phase 29-31; lighting isn't smoothed per-vertex yet - see Phase 33.

**Phase 29 (WorldLight data structure)**: Phase 30/31's cross-chunk
light propagation needs to read and write light in a *neighboring*
chunk's `LightStorage`, not just its own - the prerequisite this phase
adds. New `lcu::lighting::WorldLight<EdgeLength>` owns a `ChunkCoord ->
LightStorage` map (replacing `client/main.cpp`'s Phase 6-era ad hoc
`std::unordered_map<ChunkCoord, Light>`) and exposes `sky_light_at`/
`block_light_at` queries that resolve a local coordinate outside
`[0, EdgeLength)` into its real owning chunk, reusing `voxel::
world_to_chunk_and_local`'s existing floor-division logic (the same
helper `engine/world` already uses for block edits) rather than a
second hand-written version of that arithmetic living inside lighting
code. Both return `std::optional<u8>`: `std::nullopt` means "that
chunk's light isn't computed" honestly, never a guessed brightness -
the same discipline Phase 28's `mesh_chunk_greedy` boundary-face
fallback already established.

This phase is deliberately just the data structure and query surface -
it does not itself propagate light across a chunk boundary. A torch
near a chunk edge still stops exactly at that edge, identical to
before this phase; Phase 30/31's BFS is what will actually walk across
the boundary and write into the neighbor. 9 new unit tests (in-bounds
lookups, positive- and negative-direction cross-chunk resolution,
not-loaded-neighbor returns `std::nullopt`). Verified via real
`LCU_VERIFY_BREAK_PLACE` runs (both bgfx and non-bgfx builds) producing
byte-identical log output to Phase 28 - confirming this is a real,
behavior-preserving refactor. `ctest` 371/371 (bgfx, up from 362) /
368/368 (non-bgfx, up from 359).

**Phase 30 (sky-light cross-chunk propagation)**: sky light only ever
travels straight down in this engine (Phase 6's known simplification),
so "cross-chunk propagation" here is a seeded column scan, not a
BFS - `compute_sky_light_column` gained a `sky_open_above` parameter
(default `true`, so every existing caller keeps its old behavior); the
new `compute_sky_light_column_cross_chunk`/`compute_sky_light_cross_
chunk` supply a real value by querying the chunk directly above via
`WorldLight::sky_light_at` (checking just its bottom cell is enough,
since a blocked column is always uniformly 0 top-to-bottom). A solid
roof in the chunk above a chunk now actually darkens it - the real bug
this phase fixes; before, that chunk's own top layer always showed
full brightness regardless of what was above it.

This only works if every column's chunks are lit top-down (highest
`chunk_y` first) - `client/main.cpp`'s load loops were restructured
into three explicit passes per column (block light any order, sky
light strictly top-down, then meshing) instead of one interleaved
ascending pass. The one honestly-scoped gap: a chunk arriving over the
network (`ChunkData`) can't guarantee that ordering relative to its own
vertical neighbors by itself - a chunk streamed in *below* an
already-lit neighbor self-corrects, but the reverse order doesn't yet
retroactively relight what was already computed; closing that fully is
Phase 35's job (neighbor-dirtying).

4 new unit tests. Verified via a real two-process networked run (36
chunks streamed via `ChunkData`, break/place round-trips cleanly, zero
warnings) and real `LCU_VERIFY_BREAK_PLACE` runs (bgfx + non-bgfx)
producing byte-identical output to Phase 29. `ctest` 375/375 (bgfx, up
from 371) / 372/372 (non-bgfx, up from 368).

Honestly scoped: **whether a cross-chunk shadow actually looks correct
on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
LIMITATION**; block light still doesn't cross a chunk boundary at all
(Phase 31, a genuine BFS unlike this phase's column scan); lateral sky
light bleed under overhangs remains an unchanged, documented
simplification.

**Phase 31 (block-light cross-chunk propagation)**: unlike sky light,
block light genuinely floods in all 6 directions, so a real cross-chunk
BFS was needed - not a seeded column scan. New
`flood_block_light_cross_chunk`/`propagate_added_block_light_cross_
chunk`/`unpropagate_block_light_cross_chunk` extend the existing
single-chunk BFS to continue into a neighboring chunk's own
`LightStorage` (via `WorldLight`) whenever a step would leave the
current chunk, checking that neighbor's own block opacity along the
way. Templated on a duck-typed `ChunkProviderT` (matches
`lcu::world::World::chunk_at` exactly) for the same reason Phase 28's
`LightStorageT` was - `engine/world` doesn't depend on `engine/lighting`
so a concrete dependency would actually be cycle-safe here, but the
template keeps propagation unit-testable without needing a full `World`
instance. An unloaded neighbor is never crossed into - honestly nothing
to propagate into, not a guess.

The termination bound ("max 15 voxels around the trigger") falls out
of the existing algorithm for free: light values are capped at 15 and
the BFS already stops once a cell's level would decrement to 0, so no
separate radius cap was needed. `client/main.cpp`'s
`update_lighting_for_edit` now calls the cross-chunk versions (passing
`world` itself as `ChunkProviderT`), and its "brightest neighbor"
refill logic now queries `WorldLight::block_light_at` instead of only
checking same-chunk neighbors - a real correctness fix this phase's own
wiring pass surfaced, not a separate change.

4 new unit tests, including the trickiest case: removing one of two
cross-chunk sources correctly refills the overlap from the remaining
one without a dark gap or wrongly darkening the survivor. Verified via
a real two-process networked run (a real break/place round-trip through
the new cross-chunk edit path, zero warnings) and real
`LCU_VERIFY_BREAK_PLACE` runs (bgfx + non-bgfx) with byte-identical
output to Phase 30. `ctest` 379/379 (bgfx, up from 375) / 376/376
(non-bgfx, up from 372).

Honestly scoped: **whether real cross-chunk torchlight actually looks
correct on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
LIMITATION**; a chunk that loads after a nearby source's BFS already
finished doesn't yet retroactively receive that light (Phase 32/35's
job).

**Phase 32 (boundary buffer) skipped**: explicitly optional in the
brief, and its purpose (deferring a cross-chunk light write into a
buffer so concurrent threads don't contend for the same neighbor
chunk) has no real problem to solve yet - every lighting call in this
codebase runs synchronously on the main thread against one shared
`WorldLight`, nothing is dispatched across multiple threads. See
DECISIONS.md for the full reasoning and what would make this worth
revisiting.

**Phase 33 (VoxelClient integration + smooth lighting)**: two real
fixes, not just wiring. First, a genuine remesh gap: `neighbors_
sharing_boundary` only names a neighbor when an edit lands exactly at
a chunk's own boundary local coordinate, but Phase 31's cross-chunk
block-light BFS can reach a neighbor from edits well inside a chunk
too (any edit within light-emission range of a boundary) - that
neighbor's newly-changed light could go un-remeshed. The cross-chunk
propagate/unpropagate functions gained an optional `touched_chunks`
output set (every chunk, besides the edited one, that actually got a
light write); `update_lighting_for_edit` now returns it, and a new
`remesh_edit_neighbors` helper remeshes the union of that real set
with the existing geometric neighbor set.

Second, real smooth lighting: `mesh_chunk_greedy`'s merged quads are
now lit per-vertex - each of a quad's 4 corners independently averages
the packed light of its up-to-4 diagonally-adjacent mask cells
(`detail::smooth_corner_light`), the classic vertex-light-averaging
technique (without ambient occlusion, deliberately out of scope - see
DECISIONS.md). `MaskCell::merges_with` no longer requires equal light
to merge (Phase 28's flat-shading-only restriction is superseded now
that corners are individually sampled), so merging is purely
geometric/material again - the same or more merging than before, with
smoother output instead of a trade-off between the two. A genuinely
satisfying payoff: **no shader changes were needed at all** - the
existing `v_color1` varying (Phase 28) was already a plain, non-`flat`
float that bgfx/GLSL linearly interpolates across a triangle by
default, so once vertices started carrying different values, the
fragment shader's existing nibble-unpacking arithmetic started
receiving genuinely smooth, GPU-interpolated fractional values per
pixel automatically.

6 new unit tests. Verified via a real two-process networked run and
real `LCU_BUILD_SHADER_TOOLS=ON` + `LCU_VERIFY_BREAK_PLACE` runs (bgfx
and non-bgfx), zero regressions, byte-identical output to Phase 31.
`ctest` 385/385 (bgfx, up from 379) / 382/382 (non-bgfx, up from 376).

Honestly scoped: **what smooth lighting actually looks like on a real
GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; a chunk
loading after a nearby source's BFS already finished still isn't
retroactively relit or remeshed (Phase 35's job).

**Phase 34 (torch block + lighting benchmarks)**: new light-emitting
content (`game:torch`, `light_emission=14`) plus actually meeting the
brief's explicit lighting-performance budgets, not just claiming them.
`is_transparent` is deliberately `false` - a transparent torch would
have been correctly lit and collidable but completely invisible
(`mesh_chunk_greedy` never meshes the transparent layer - it doesn't
exist), a real fake-feature trap caught and fixed before it became a
bug, not after. A new `LCU_VERIFY_TORCH` headless hook breaks the
spawn block, grants a torch, hotbar-cycles to it, places it, and logs
the real light value read back post-place: `"Placed game:torch at
world (0, 28, -1): block_light=14"`.

3 new `tools/benchmark` cases target the brief's own explicit budgets
(chunk-with-neighbors < 2ms, torch place/unplace at a chunk edge each
< 0.5ms). First finding: this project's only `CMAKE_BUILD_TYPE`
("Development") isn't a CMake built-in type, so no target anywhere has
ever built with real optimization flags - confirmed by Google
Benchmark's own "built as DEBUG" warning. A dedicated `build/
bench-release` (`-O3 -DNDEBUG`, confirmed via cache) was created to get
trustworthy numbers - see DECISIONS.md; the underlying project-wide gap
is left open, documented, for a dedicated future pass.

Second finding, a real one: even under genuine `-O3`, torch place/
unplace still missed the 500us budget (639us/767us). Root cause: the
cross-chunk BFS paid up to ~13 redundant `unordered_map` lookups across
two separate maps per popped cell, plus unconditional floor-division,
for the common case where a step never leaves the current chunk. Fixed
with an in-bounds fast path (plain integer range check, reuses
already-held pointers, zero hash lookups for in-chunk steps), proven
behavior-preserving by the unchanged full `ctest` suite (385/385 bgfx /
382/382 non-bgfx) before and after. Re-measured: 115,649 ns / 99,158
ns - both now comfortably under budget; the compute-chunk-with-
neighbors case (16,616 ns) stayed comfortably under its own 2ms budget
throughout.

No new unit tests this phase (the BFS change is proven by the existing
suite staying green; the new content is proven by the real
`LCU_VERIFY_TORCH` run and the new benchmarks). Verified via a real
two-process networked run (600 server ticks, 36 chunks streamed, zero
warnings) and real `LCU_VERIFY_TORCH`/`LCU_VERIFY_BREAK_PLACE`/
`LCU_VERIFY_CRAFT` runs, zero regressions.

Honestly scoped: **what a placed, lit torch actually looks like on a
real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; the
`CMAKE_BUILD_TYPE="Development"` no-optimization gap remains project-
wide outside `build/bench-release`; a chunk loading after a nearby
source's BFS already finished still isn't retroactively relit or
remeshed (Phase 35's job, next).

**Phase 35 (chunk unload marks neighbors dirty)**: closes the two
Phase 30/31 "arrived too late" lighting gaps, plus the literal "chunk
unload" this phase is named for. `reseed_light_for_newly_loaded_chunk`
reuses the existing `flood_block_light_cross_chunk` unchanged, just
seeded differently - every already-loaded neighbor's shared boundary
face is walked once, collecting every currently-lit cell on both sides
into one queue and re-flooding (safe since that function only ever
raises a value, never lowers one); sky light cascades a recompute down
through every already-loaded chunk stacked below a newly-loaded roof.
Wired into `VoxelClient`'s initial load, per-movement streaming, and
the networked `ChunkDataFragment` path (closing that handler's own
long-standing code-comment gap). Real client-side chunk unloading was
also added (the client's own counterpart to `VoxelServer`'s Phase 20
interest-scoped unloading, saving to `client_world/chunks/` first so a
single-player edit survives a wander-away-and-back, then destroying
the GPU mesh and calling `WorldLight::remove_chunk_light` - that
function's own doc comment named this phase as its real caller since
Phase 29).

A real, pre-existing (not introduced by this phase) finding made while
verifying it: `LCU_VERIFY_MOVE_SECONDS` moves in a straight line and
never jumps, so it stalled at the same world position regardless of
whether it ran for 6s or 20s - confirmed via a `git stash`-isolated
pre-Phase-35 build reproducing byte-identically that this is a real
terrain obstacle taller than the auto-step height, not a Phase 35 bug
(holding Jump for the same duration cleared it immediately). Not fixed
(that hook's existing documented straight-line behavior stays as-is,
see DECISIONS.md) - this phase's own real verification temporarily
held Jump to clear the obstacle.

4 new unit tests. Verified via a real longer-distance single-player
run (repeated `"Streaming center moved to ..."`/`"Unloaded 12
chunk(s) ..."` pairs, loaded count steady at 48, 60 real `.chunk`
files written) plus byte-identical `LCU_VERIFY_TORCH`/`LCU_VERIFY_
BREAK_PLACE`/`LCU_VERIFY_CRAFT` runs and a real two-process networked
run (600 ticks, 36 chunks streamed, zero warnings). `ctest` 389/389
(bgfx, up from 385) / 386/386 (non-bgfx, up from 382).

Honestly scoped: **what any of this looks like on a real GPU/display
is still NOT VERIFIED — ENVIRONMENT LIMITATION**; the reload-from-disk
half of client persistence wasn't separately re-exercised end-to-end
in this run (the verify hook only moves one direction) - save-on-
unload is proven by the 60 real files written, and load-from-disk
reuses the exact same `chunk_serializer` API `VoxelServer` already
round-trip-tests; lateral sky light bleed under overhangs remains
unmodeled (documented since Phase 6).

**Phase 36 (entity boxes + extended debug overlay)**: new
`Renderer::submit_wireframe_box` draws a 12-edge line-list box,
reusing `submit_billboard`'s exact position+color vertex format/shader
(Phase 27's sky program) rather than a third shader pair - real depth
testing against terrain, no depth write. Wired into `VoxelClient`: one
box per local AI entity or remote interpolated entity, reusing
`make_player_aabb`. A new `DebugOverlayStats` struct carries chunks
loaded/entity count/real draw-call count/unfinished job count into
`draw_debug_overlay`'s new second on-screen text line; new
`JobSystem::unfinished_job_count()` accessor. CPU/GPU/RAM/ping/
bandwidth deliberately NOT added - no real per-platform CPU/RAM reader
or per-connection RTT/byte-counter exists yet, and a fake placeholder
would violate this project's own verification discipline (see
DECISIONS.md). Draw-call counting mirrors each `submit_*` call's own
no-op-on-invalid-program condition, so it never over-reports.

3 new unit tests (`JobSystem.UnfinishedJobCount*`). Verified via a
real `LCU_BUILD_SHADER_TOOLS=ON` run (`"Chunk shader program
valid=true"`/`"Sky shader program valid=true"` - the new wireframe-box
draw call executing against real compiled shaders, not just `Noop`), a
real `LCU_VERIFY_BREAK_PLACE` run, and a real two-process networked
run (100 frames, 3 remote AI entities boxed every frame, zero
warnings). `ctest` 392/392 (bgfx, up from 389) / 389/389 (non-bgfx, up
from 386).

Honestly scoped: **what the wireframe boxes or overlay text actually
look like on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
LIMITATION**; CPU/GPU/RAM/ping/bandwidth remain deliberately absent
from the overlay until this codebase has a real source for them.

**Phase 37 (sea level at y=0 + water block/rendering)**: `worldgen::
kSeaLevel` (world Y=0, new exported constant) - `terrain_height()`
recentered from an always-positive [8,56] range to [-20,20], centered
on sea level, so roughly half of all columns now land above it (dry
land/hills) and half below (real lake/ocean basins). `generate_
terrain_chunk` gained a `water_block` param filling any below-sea-
level column's gap up to Y=0. `game:water` is deliberately `is_
transparent=false` (the exact torch precedent - no transparent-layer
meshing exists yet, `true` would make it invisible) and `has_
collision=false` (real - the same `has_collision`-driven `is_solid`
predicate every block's collision already uses, so a player genuinely
walks/swims through it; no buoyancy/drag beyond that). All four
`QualityProfile` tiers' vertical range shifted to straddle sea level
while keeping every tier's total chunk count unchanged (1/18/27/36),
so every prior "Loaded N chunks" claim stays numerically true.

A fixed (0,0) spawn column could now legitimately land underwater by
pure chance (no swim mechanics exist) - `VoxelClient`/`VoxelServer`
each gained an identical, deterministic `find_dry_spawn_column` (a
square-ring search outward for the nearest column at or above sea
level, same seed on both sides so they agree without sending a
coordinate over the wire).

6 worldgen unit tests updated/added, 1 quality-profile test updated.
Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn column
(-19,18) found for seed 1337, dry land), real `LCU_VERIFY_BREAK_
PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical
behavior at the new location), and a real two-process networked run
where client and server independently compute the identical spawn
column and the server-reconciled player position lands on dry land,
zero warnings/errors. `ctest` 393/393 (bgfx, up from 392) / 390/390
(non-bgfx, up from 389).

Honestly scoped: **what water actually looks like on a real
GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; water
renders as a solid-looking blue block, no transparency; no waves/
current/buoyancy/swimming physics; no beach/sand shoreline transition;
sky light still stops entirely at water's surface (treated opaque like
any other solid block, an honest consequence of the existing binary
light model).

**Phase 38 (continental/mountain terrain)**: two genuinely separate
worldgen noise stages, matching brief section 21's own "kontinental ->
terrain" pipeline naming for real. A new low-frequency continental
layer (`kContinentalNoiseScale`, ~666-block wavelength) decides both a
column's base elevation (`kDeepOceanBase`..`kHighlandBase`) and how
much amplitude the existing 4-octave detail noise gets to work with
(`kMinMountainAmplitude`..`kMaxMountainAmplitude`) - coastal/oceanic
columns stay flat regardless of what the detail layer samples,
highland columns get real mountain-sized relief. A separate
`kContinentalSeedOffset` keeps the two noise fields statistically
independent.

Found and fixed in the same phase: `find_dry_spawn_column`'s search
radius (64, sized for the old single-frequency noise) could now
legitimately never leave one giant ocean basin - confirmed for real
(seed 1337 needed radius 84 to find any dry land at all). Fixed by
raising `kMaxRadius` to 1024 on both `VoxelClient`/`VoxelServer` and
rewriting the ring search from O(ring-area) (re-scanning the full
square, skipping most cells) to O(ring-perimeter) (only the new
ring's boundary), keeping even the worst case a fast one-time startup
cost (confirmed via a real run completing in 0.23s wall-clock).

1 worldgen test's height-range bounds widened (measured empirically
via a real 20-seed sweep: [-15,20], set to [-20,30] for headroom); 1
new test (`LocalRoughnessVariesAcrossRegions`) confirming local
terrain roughness now genuinely varies by region, unlike the old
uniform-amplitude model. Verified via a real `LCU_BUILD_SHADER_
TOOLS=ON` run (spawn column (-84,-84) found for seed 1337, dry land,
full sky light), real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
`LCU_VERIFY_CRAFT` runs, and a real two-process networked run with
matching independently-computed spawn columns, zero warnings/errors.
`ctest` 394/394 (bgfx, up from 393) / 391/391 (non-bgfx, up from 390).

Honestly scoped: **what the new mountain/continental terrain shape
actually looks like on a real GPU/display is still NOT VERIFIED —
ENVIRONMENT LIMITATION**; no ridged-multifractal/erosion mountain
shaping (a deliberately scoped, honest simplification, not a shortcut
hiding a gap - see DECISIONS.md); still no climate/biome/caves/ores/
structures/vegetation stages (Phases 39-41).

**Phase 39 (biomes)**: real climate/biome pipeline stage - a new
`Biome` enum (`Snowy`/`Plains`/`Desert`) and `biome_at(seed, x, z)`, a
genuinely independent low-frequency noise field (own
`kClimateSeedOffset`, own threshold split, Plains deliberately the
widest band at 50% since it was every column's only behavior before
this phase). `generate_terrain_chunk`'s signature changed to take a
`BiomeBlocks` struct (each biome's own surface/subsurface block ids)
instead of flat parameters; stone/water stay biome-independent on
purpose. Two new real blocks: `game:sand` (Desert surface+subsurface)
and `game:snow` (Snowy surface, dirt subsurface), registered
identically on `VoxelClient`/`VoxelServer` in the same sequence
position (BlockId alignment for replication). New spawn-log biome name
- real, observable confirmation the climate stage ran.

2 new worldgen tests (incl. a real sweep confirming all three biome
categories genuinely occur), 5 existing tests rewritten to compute
expected blocks from the actual biome at each test coordinate via
`biome_at` rather than assuming Plains. Verified via a real
`LCU_BUILD_SHADER_TOOLS=ON` run (spawn column (-84,-84), `biome=
Plains`, confirmed by breaking the spawn block and picking up
`game:grass`), real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
`LCU_VERIFY_CRAFT` runs, and a real two-process networked run with
matching independently-computed spawn columns, zero warnings/errors.
`ctest` 396/396 (bgfx, up from 394) / 393/393 (non-bgfx, up from 391).

Honestly scoped: **what sand/snow/biome transitions actually look
like on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
LIMITATION**; a temperature-only climate model, no humidity axis, no
Whittaker-diagram-style biome table (see DECISIONS.md for why this
scope, not more); no elevation-climate coupling; no item mapping for
sand/snow yet; no caves/ores/structures/vegetation stages yet (Phases
40-41).

**Phase 40 (caves + ores)**: real cave-carving and ore pipeline stages.
New 3D noise primitives (`hash3d`/`lattice_value3d`/trilinear
`smooth_noise3d`/4-octave `fractal_noise3d` - every earlier worldgen
stage only needed 2D column noise). `is_cave(seed, x, y, z,
surface_height)` samples two independent 3D noise fields and carves
open air wherever their values land within a small threshold of each
other - a "noise crevice" that produces real winding tunnels, not
single-threshold "cheese cave" blobs (see DECISIONS.md); a real minimum
depth below that column's own `terrain_height()` keeps tunnels from
ever punching a hole at ground level. `OreType` (`None`/`Coal`/`Iron`)
and `ore_at(seed, x, y, z)` - each ore its own noise field, absolute Y
depth band, and rarity threshold, with `None` overwhelmingly common by
design and Iron genuinely rarer than Coal. New `OreBlocks` struct (same
pattern as `BiomeBlocks`); two new real blocks `game:coal_ore`/
`game:iron_ore`, registered identically on `VoxelClient`/`VoxelServer`
right after `game:water`. `generate_terrain_chunk`'s stone-band branch
now checks `is_cave` first, then `ore_at`, falling back to plain stone.

Ore thresholds were tuned from a real measured `fractal_noise3d` output
distribution, not guessed: the first round-number thresholds (0.90/
0.95) proved nearly unreachable for Iron and too sparse for Coal once a
real test tried to find them, caught the same way Phase 38's spawn-
radius bug and Phase 39's biome-threshold bug were - by measuring the
actual system instead of assuming a shape for it. Fixed to 0.70/0.80
(Coal ~3.7% of eligible cells, Iron ~0.1%, both still small next to
`None`'s overwhelming share). 6 new worldgen tests, 2 existing tests
rewritten (one renamed `ChunkFarBelowTerrainIsStoneCaveOrOre`) since
their old "always plain stone below the subsurface layer" assumption
stopped holding once caves/ores could carve or substitute those cells.
Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run, real
`LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs
(byte-identical to Phase 39), and a real two-process networked run with
matching independently-computed spawn columns, zero warnings/errors/
rejects. `ctest` 401/401 (bgfx, up from 396) / 398/398 (non-bgfx, up
from 393).

Honestly scoped: **what carved caves/ore veins actually look like on a
real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; no
cave-specific lighting treatment; no ore item drops/mapping yet; no
structures/vegetation stages yet (Phase 41); caves/ores have no
artificial depth ceiling (an honest consequence of the noise fields
having no cutoff of their own, not a hidden gap - see DECISIONS.md).

**Phase 41 (vegetation)**: real vegetation pipeline stage. `VegetationType`
(`None`/`Tree`/`Cactus`) and `vegetation_at(seed, x, z, biome)` - its
own independent noise field per type, Tree only for `Biome::Plains`,
Cactus only for `Biome::Desert`, `Biome::Snowy` stays vegetation-free
on purpose (not every biome needs unique content, the same reasoning
water stayed biome-independent in Phase 39). New `VegetationBlocks`
struct (`wood`/`leaves`/`cactus`, same pattern as `BiomeBlocks`/
`OreBlocks`); three new real blocks registered identically on
`VoxelClient`/`VoxelServer` right after `game:iron_ore`.

Deliberately single-column shapes: a tree is a 4-block trunk capped by
a 3-block leaf pillar directly above it, a cactus a 3-block stack - no
canopy spreading into neighboring columns. A real, deliberate scope
choice, not an accidental limitation: a real spreading canopy is
achievable without needing neighbor chunks to exist yet (`terrain_
height`/`biome_at`/`vegetation_at` are pure functions of world
coordinates, callable for any column), but was deferred as real further
work outside this phase's honest scope (see DECISIONS.md). A useful
side effect: this also makes vegetation placement correct across
vertically-stacked chunk boundaries with zero special-casing, the same
way Phase 37's sea-level water fill already is.

Thresholds were measured from `fractal_noise`'s own real output
distribution from the start this time, not guessed - having just been
caught by Phase 40's ore-threshold mistake, the same standalone-probe
technique was applied here before picking numbers, and both thresholds
worked on the first real test run (trees ~4.5% of Plains columns,
cacti ~2.5% of Desert columns). 7 new worldgen tests (incl. a real
sweep confirming both types occur, and a real end-to-end check that
`generate_terrain_chunk` places the actual trunk/canopy shape), 1
existing test rewritten since its "always air above terrain except
water" assumption stopped holding once vegetation could place blocks
there. Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run, real
`LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs
(byte-identical to Phase 40), and a real two-process networked run
with matching independently-computed spawn columns, zero
warnings/errors/rejects. `ctest` 406/406 (bgfx, up from 401) / 403/403
(non-bgfx, up from 398).

Honestly scoped: **what a real tree/cactus actually looks like on a
real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; no
wide/spreading tree canopies (a real, documented scope choice, not a
hidden gap); no varied tree/cactus silhouettes; no wood/leaves/cactus
item drops/mapping yet; no structures pipeline stage (out of scope for
this 42-phase plan entirely).

**Phase 42 (documentation update)**: closed out the Phases 25-42
program with a real documentation pass, not just a phase-count
bookkeeping entry. `README.md` written for the first time (was an
empty file) - a real project-level entry point: what the project is,
an honest current-status pointer, a feature summary reflecting the
actual Phase 41 state, a quick-start build/run block, and a
documentation map. A stale `PROJECT_STATE.md` "Known Limitations"
entry left over from Phase 17 (still claiming "no climate/biome/caves/
ores/structures/vegetation/decoration") was corrected in place to
reflect that Phases 39-41 made biome/caves/ores/vegetation all real,
leaving only structures genuinely unimplemented.
`BUILDING.md`/`CHANGELOG.md`/`DECISIONS.md` were reviewed against the
current state and found already current (kept up to date
phase-by-phase throughout Phases 35-41), so no further edits were
needed there.

**Phase 43 (input overhaul + mouse look + Minecraft-parity defaults)**:
first phase of a second user-directed program (Phases 43-46:
controls, a 2D UI framework, persistent options, and a menu/options/
controls screen). New `engine/platform::KeyBindings` - each `Action`
maps to up to 2 physical keys (a unified space covering both keyboard
scancodes and 3 mouse buttons), starting from real Minecraft-parity
defaults and rebindable in place (`bind()`/`reset_to_defaults()`) -
the real data structure a Phase 46 controls menu will read/write, not
a stub. `KeyboardInputBackend` renamed `DesktopInputBackend` and
rewritten to poll through `KeyBindings` (mouse buttons included)
instead of a fixed table, so `Interact`/`PlaceBlock` are driven by
real mouse clicks with zero changes needed to the existing break/place
logic. New `Action::PickBlock` (middle-click), `CycleHotbarPrev`
(wheel down), 9 `SelectHotbar1..9` (number row), and `Escape` (ESC/Tab,
still routed through the normal Action/KeyBindings path rather than a
hardcoded SDL check - see DECISIONS.md).

Real relative mouse-look (`InputState::mouse_delta_x/y` via
`SDL_GetRelativeMouseState`) applied to the camera additively alongside
the existing arrow-key look fallback, not replacing it. Real mouse
capture management: `Window::set_relative_mouse_mode`/
`consume_wheel_delta_y`/`consume_focus_lost` wrap SDL's own capture/
wheel/focus mechanisms - ESC/Tab releases capture, a click while free
re-captures it without that same click also breaking/placing a block
(`suppress_click_for_recapture`), losing window focus releases it
automatically. This project's own pre-Phase-43 Sprint/Crouch defaults
were backwards (Sprint=Shift, Crouch=Ctrl) - corrected to Minecraft's
real Sprint=Ctrl, Crouch=Shift.

Hygiene fixes from a real first macOS run's own bug reports: the
chunk/sky shader load path now resolves against
`Window::executable_base_path()` (`SDL_GetBasePath`) instead of the
current working directory - verified via real runs of the client from
the repo root and from `/tmp`, both correctly loading real shaders
regardless of launch directory. Rendering-only constants gated behind
`LCU_ENABLE_BGFX` (genuinely dead declarations without it - the real
cause of a reported unused-variable warning, not reproduced by this
sandbox's own compiler). A real, verified redundant `Lcu::Math` link
entry removed from `game/CMakeLists.txt` (already provided
transitively via `Lcu::EngineCore`) - a real contributor to a reported
"ignoring duplicate libraries" linker warning (an Apple-`ld`-specific
message, also not reproduced here).

20 new unit tests. Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run
(from multiple working directories), real `LCU_VERIFY_BREAK_PLACE`/
`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical to Phase
41), and a real two-process networked run with matching
independently-computed spawn columns, zero warnings/errors/rejects.
`ctest` 420/420 (bgfx, up from 406) / 417/417 (non-bgfx, up from 403).

Honestly scoped: **real mouse-look/click/wheel/capture behavior
against an actual mouse device and display is still NOT VERIFIED —
ENVIRONMENT LIMITATION** (this sandbox has no real mouse -
`SDL_SetWindowRelativeMouseMode` under the dummy video driver here
happens to report success with nothing to actually capture); the
exact unused-variable/duplicate-library linker warnings these fixes
target were never reproduced in this sandbox either (fixed on
code-reading grounds, not by watching a warning disappear); no 2D
UI/menu yet to rebind a key through (Phase 44/46); `Action::
Inventory`/`SwapOffhand`/`Escape`'s pause-menu half and most
`SelectHotbar5-9` slots still have no consumer (the real hotbar only
has 4 items).

**Phase 44 (2D UI framework)**: real 2D UI quad batch -
`engine/rendering::Renderer::submit_ui_quad`/`flush_ui_quads` queue
screen-space rectangles (pixel position/size, RGBA color, UV 0..1)
across a frame and upload/draw them in exactly one real
`bgfx::submit()` via transient buffers, the same idiom
`submit_billboard`/`submit_wireframe_box` already use. New
`Mat4::orthographic` (real unit-tested corner/center mapping). New
`kUi2dViewId` bgfx view, own `vs_ui2d.sc`/`fs_ui2d.sc` shader pair
(position + UV + color, no lighting).

Submitted *last* (after terrain), a real, deliberate correction of
this phase's own literal "after sky, before terrain" view-order
wording - that ordering would make the UI invisible behind any solid
geometry, since bgfx composites views in submission order (see
DECISIONS.md for the full reasoning: this is exactly the class of
problem this project's "no fake features" discipline exists to
catch). Real first consumer: a permanent, screen-centered crosshair,
doubling as this phase's own visual verification element rather than
a separate throwaway test.

`ItemDefinition::icon_color`/item-icon pattern rendering deferred
(PARTIAL, a real, documented limit): no inventory/hotbar widget
exists yet to consume it, and this project's own `ItemDefinition` doc
comment already argues against adding fields speculatively - the
vertex format already carries real UV data ready for a future
consumer. 7 new unit tests. Verified via a real
`LCU_BUILD_SHADER_TOOLS=ON` run (`UI2D shader program valid=true`),
real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT`
runs (byte-identical to Phase 43), and a real two-process networked
run with matching independently-computed spawn columns, zero
warnings/errors/rejects. `ctest` 427/427 (bgfx, up from 420) / 419/419
(non-bgfx, up from 417).

Honestly scoped: **what the crosshair/any UI quad actually looks like
on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
LIMITATION** (headless Noop backend proves the pipeline runs
end-to-end, not that it looks right); no item-icon rendering yet
(deferred, see above); no slot backgrounds/health/hunger/menu
backgrounds yet (real future consumers of this same batch API, Phase
46+).

**Phase 45 (persistent options)**: new `engine/platform::Options` -
`mouse_sensitivity`/`fov`/`hud_enabled`/`debug_overlay_enabled` plus a
full `KeyBindings` instance, all loaded from and saved to a real
`key=value` text file (`# comments`, blank lines skipped) at
`SDL_GetPrefPath("LiveCraftUltimate", "LiveCraftUltimate")` - a real
per-OS user config directory, not a hand-picked path, verified on this
Linux sandbox at `~/.local/share/LiveCraftUltimate/LiveCraftUltimate/
options.txt`. Every one of `KeyBindings`' 29 `Action`s round-trips
through a new bidirectional `action_name`/`parse_action_name` table
(`key.<action>=<key>`, plus `key.<action>.alt=<key>` only when a real
second binding exists) rather than a raw enum index, so a future
`Action` insertion can't silently corrupt an existing player's save
file (see DECISIONS.md).

Real tolerance, not just a happy path: a missing file leaves every
default untouched and `load()` returns `false` (first run always looks
like this, not an error); a corrupt or unrecognized line (bad number,
unknown action name, unknown key name) is skipped and every other real
line still loads - verified against a real file with deliberately
interleaved garbage lines between real ones.

`VoxelClient` now genuinely consumes this instead of hardcoded
constants: the former `kMouseSensitivity` constant is gone, replaced
by `options.mouse_sensitivity`; the Phase 44 crosshair is now gated
behind `options.hud_enabled`; the debug overlay is now gated behind
`options.debug_overlay_enabled`, a real behavior change since that
option defaults to `false` (Minecraft's own F3-gated convention), so
the overlay no longer renders unconditionally as it did through Phase
44. Options load at startup and save on exit; no menu UI writes to it
yet (Phase 46). 9 new unit tests (7 `Options`, 2 `ActionName`).
Verified via real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
`LCU_VERIFY_CRAFT` runs (byte-identical to Phase 44, plus the new
load/save log lines), a real `LCU_BUILD_SHADER_TOOLS=ON` run (`Chunk`/
`Sky`/`UI2D` shader programs all still `valid=true`), a real
two-process networked run (zero warnings/errors/rejects, matching
spawn columns), and direct inspection of the real written
`options.txt` confirming every field, including all 29 keybindings,
round-trips as human-readable text at the real OS path. `ctest`
436/436 (bgfx, up from 427) / 428/428 (non-bgfx, up from 419).

Honestly scoped: no options menu UI exists yet to change these values
in-game (Phase 46 - this phase is the storage layer only, per its own
spec); `options.fov` is persisted but **not yet applied to the
camera's projection - NOT VERIFIED, deferred** (nothing currently
reads it for rendering; wiring it in with no menu to change it would
be speculative and unverifiable, so it stays honestly unused until
Phase 46 gives it a real consumer).

**Phase 46 (menu framework: pause/options/controls)**: real
`engine/ui::MenuStack` - stacked `MenuScreen`s (title + `MenuItem` rows,
each with a real `on_activate`/`on_adjust` callback), pure logic with
zero SDL/bgfx dependency (`engine/ui` now builds under `LCU_BUILD_CLIENT`
unconditionally, not only `LCU_ENABLE_BGFX` - see DECISIONS.md), so it's
tested in both the bgfx and non-bgfx configs. `menu_item_layout`/
`menu_item_at_point` compute each row's real pixel rect from screen size
alone - the one shared source of truth both drawing and real mouse
hit-testing read from.

ESC in-game now opens a real Pause screen (Zurueck zum Spiel/Optionen/
Steuerung/Beenden) instead of only releasing mouse capture, and a
non-empty `menu_stack` genuinely pauses movement/physics/AI/day-night -
verified via a real headless run holding `MoveForward` down across the
pause (position provably unchanged) then again after closing it
(position provably changed). Network *receive* deliberately keeps
running while paused, a real, documented deviation from the phase's own
literal "network pauses too" wording (see DECISIONS.md: halting it
risked the connection reading as dead by the time the player unpauses) -
only this client's own outgoing input actually pauses.

Real Options screen (mouse sensitivity/FOV via the existing `LookLeft`/
`LookRight` actions as +/-, HUD/debug-overlay toggles, save-on-leave).
**FOV is now genuinely applied to the camera's projection matrix** -
closes the exact gap Phase 45 deliberately left open. Real Controls
screen: every rebindable `Action` listed, Enter/click enters a real
"waiting for input" capture (new `lcu::platform::poll_any_pressed_key`/
`is_escape_key`, reading real SDL keyboard/mouse state directly - the
whole point is binding a key nothing uses yet), a release-then-press
debounce (`rebind_ready`) stops the activating key from immediately
binding itself, Reset restores every default. New `Action::MenuConfirm`
(Enter) - both it and `Action::Escape` are deliberately excluded from
the rebind list.

**A real use-after-free was found and fixed this phase, not just
theorized about.** A `MenuItem`'s own callback lives inside the
`MenuScreen` currently on top of the stack; the first implementation
popped/pushed `menu_stack` directly from inside such a callback,
destroying (or, for push, potentially reallocating) that very screen -
including the closure still executing - while it was still running.
Headless testing with the new `LCU_VERIFY_MENU` hook reproduced this as
a real segfault. Fixed by deferring every such mutation through a
`pending_menu_action`, processed once per frame after
`activate_selected()`/`adjust_selected()` have fully returned - see
DECISIONS.md for the complete story, including why this needed a
real crash to surface rather than being caught by inspection alone.

`LCU_VERIFY_MENU` (new): pause/resume with provable movement gating,
real navigation via edge-detected `LookDown`/`MenuConfirm`, a real
two-step sensitivity adjustment confirmed by inspecting the saved
`options.txt` (`0.0022` -> `0.0026`, exactly two real `+0.0002` steps).
Deliberately does not exercise the controls screen's rebind capture -
`poll_any_pressed_key` reads real SDL hardware state this sandbox's
dummy input driver never produces, the same category of gap Phase 43's
own mouse-look verification already has. 21 new unit tests. Verified
via the `LCU_VERIFY_MENU` run above, real `LCU_VERIFY_BREAK_PLACE`/
`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical to Phase 45),
a real `LCU_BUILD_SHADER_TOOLS=ON` run (`Chunk`/`Sky`/`UI2D` shader
programs all still `valid=true`), and a real two-process networked run
(zero warnings/errors/rejects, matching spawn columns). `ctest`
455/455 (bgfx, up from 436) / 447/447 (non-bgfx, up from 428).

Honestly scoped: the menu's real on-screen appearance is still **NOT
VERIFIED — ENVIRONMENT LIMITATION** (headless Noop backend proves the
pipeline runs, not that it looks right); the controls screen's rebind
capture is real, reviewed code but **NOT VERIFIED against a real
keyboard/mouse** (see above); no live Renderdistanz control (deferred,
PARTIAL - `load_settings.radius_xz` is `const`, live re-streaming is a
real, separate structural change this phase's own directive explicitly
allows deferring - see DECISIONS.md); no chat, no multiplayer UI, no
advancements (out of scope per this phase's own directive).

**Phase 47 (HUD overhaul: hotbar + health/hunger bars + F-toggles)**:
new `engine/ui::hud.{h,cpp}` - pure layout math (`hotbar_slot_layout`/
`stat_bar_layout`), zero SDL/bgfx dependency, tested in both configs.
Real Minecraft-position hotbar: 9 bottom-center slots, bordered/filled
quads, flat colored icon quads for the 4 real `placeable_items`, real
held-count labels. **New `ItemDefinition::icon_color`** closes the
exact gap Phase 44 deferred ("no inventory/hotbar widget exists yet to
consume it") - set per item to match its own block's tint where one
exists. Real 10-icon health/hunger bars above the hotbar, hardcoded
full this phase (real half-icon fill math already in place for Phase
51's real values).

5 new `Action`s - `ToggleHud`/`ToggleDebugOverlay`/`Screenshot`/
`TogglePerspective`/`Fullscreen`, bound to F1/F3/F2/F5/F11, the exact
bindings Phase 43's own "verbindlich" table reserved, only now given
real consumers. `ToggleHud`/`ToggleDebugOverlay` flip the same real
persisted flags the options menu already reads/writes. New `Renderer::
request_screenshot`/`Window::set_fullscreen` wrap
`bgfx::requestScreenShot`/`SDL_SetWindowFullscreen` for real.
`TogglePerspective` is real camera-eye-offset rendering only - raycast/
movement/`camera.position` are untouched, only the render eye shifts
back along the real look direction. **Third-person-front is a real,
documented PARTIAL**: no player model exists anywhere in this codebase
to render in front of the camera, so that mode is honestly not
implemented rather than shipped as an empty no-op.

**A real shared-resource bug was found and fixed this phase**: up to
three systems (debug overlay, HUD labels, menu labels) now draw into
bgfx's one debug-text buffer the same frame; each used to call
`clear_debug_text()` internally, which would have silently wiped
whichever ran first. Fixed by moving the one real clear up to `client/
main.cpp`, called once, before all three, in a real deliberate order
(overlay -> HUD -> menu).

New `LCU_VERIFY_HUD` hook: each F-key pressed on its own frame, real
log output confirms each real resulting state. 21 new unit tests.
Verified via that run, real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
`LCU_VERIFY_CRAFT`/`LCU_VERIFY_MENU` runs (byte-identical to Phase 46),
a real `LCU_BUILD_SHADER_TOOLS=ON` run (`Chunk`/`Sky`/`UI2D` shader
programs all still `valid=true`), and a real two-process networked run
(zero warnings/errors/rejects, matching spawn columns). `ctest`
466/466 (bgfx, up from 455) / 458/458 (non-bgfx, up from 447).

Honestly scoped: the HUD's real on-screen appearance is still **NOT
VERIFIED — ENVIRONMENT LIMITATION** (headless Noop backend proves the
pipeline runs, not that it looks right); `bgfx::requestScreenShot`'s
real output can't be inspected under the headless `Noop` backend (no
real framebuffer content to capture); third-person-front deferred (see
above); hotbar slots 5-9 still show nothing (only 4 real placeable
items exist, unchanged since Phase 43).

**Phase 48 (block highlight + hold-to-break + hand)**: a real black
wireframe highlight on the raycast-targeted block (a new consumer of
`submit_wireframe_box`, already real since Phase 36). **Real hold-to-
break**: breaking a block is no longer instant - `BlockDefinition::
hardness` (real per-block seconds: stone 2.0, wood 1.5, dirt 0.5,
leaves 0.2, grass 0.6, sand 0.5, snow 0.1, cactus 0.4, ores 3.0, torch
0.0 = instant) now gates how long Interact must be held against the
*same* targeted block. New pure `lcu::voxel::break_progress_fraction`/
`is_break_ready` (`break_progress.{h,cpp}`) - the one real source both
the break trigger and the darkening overlay read, so they can never
disagree. A real one-shot latch stops a held click from re-sending a
networked break request every frame while waiting for the server's own
`BlockChange`.

Real break-progress overlay: a solid box (new `Renderer::
submit_solid_box`, reusing the existing sky shader - no new shader
files needed) darkens toward black as progress advances - a real,
honestly-scoped substitute for a per-fragment crack-noise shader effect
on the block's own face, which would need a new world-position uniform
threaded through `fs_chunk.sc`, outside this phase's scope (see
DECISIONS.md). Opaque, not alpha-blended - a real, documented rough
edge (appears at whatever darkness the first held frame computes,
doesn't fade in from invisible). Real hand icon (Phase 47's
`icon_color`) with a real elapsed-time sine-ease swing on every
break/place. Water needed zero special-case unbreakable logic -
`has_collision=false` (Phase 37) already keeps the raycast from ever
targeting it.

**A real networked timing bug was found and fixed during this phase's
own verification, not merely anticipated.** `LCU_VERIFY_BREAK_PLACE`/
`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` were rewritten from single-frame
instant-break pulses to real elapsed-time hold windows sized to each
target's own hardness (a fixed frame count can't express this
reliably in this sandbox's unthrottled loop) - a real, necessary
consequence of the mechanic change, not a regression. `LCU_VERIFY_
CRAFT`'s networked run specifically failed with its first chosen gap
(0.2s) between its two held-break windows - the second break's raycast
could still see the stale, not-yet-removed grass block if the server's
`BlockChange` hadn't landed yet - confirmed by an actual failed run
(only one break landed, both craft attempts rejected) during
verification, then fixed by widening the real gap to 0.8s and
re-confirming a clean run.

9 new unit tests. Verified via all four rewritten/regression hooks
(full pipelines confirmed real end-to-end), a real two-process
networked `LCU_VERIFY_CRAFT` run (zero warnings/errors/rejects with the
widened gap), real `LCU_VERIFY_MENU`/`LCU_VERIFY_HUD` regression runs
(unaffected), and a real `LCU_BUILD_SHADER_TOOLS=ON` run (`Chunk`/
`Sky`/`UI2D` shader programs all still `valid=true`). `ctest` 475/475
(bgfx, up from 466) / 467/467 (non-bgfx, up from 458).

Honestly scoped: what the highlight/overlay/hand icon actually look
like on a real GPU/display is still **NOT VERIFIED — ENVIRONMENT
LIMITATION**; no real crack-noise-density shader effect on the block's
own face (deferred, see above).

**Phase 49 (inventory screen + drag/drop + crafting grid)**: the
biggest structural change of this batch. Phase 21's `placeable_items`/
`selected_placeable_index` - a virtual "known item types" selector,
completely decoupled from where an item actually lived in the inventory
- is gone entirely. The hotbar is now real: `selected_hotbar_slot` is a
plain index (0-8) into 9 of `player_inventory`'s own 36 slots;
`CycleHotbar`/`CycleHotbarPrev`/`SelectHotbar1-9` move it, placing
reads whatever `ItemStack` actually sits there via a new
`BlockItemMapping::block_for_item` reverse lookup. Any block/item pair
registered via `register_pair` is now automatically placeable the
moment the player holds it - `game:wood` (Phase 41's block, never
before given an item - a real, now-closed gap) needed nothing beyond
its own `register_pair` call.

Pressing `E` opens a real Minecraft-shaped inventory screen: 2x2
crafting grid + result slot on top, 3x9 main storage, the same hotbar
again at the bottom. New `engine/ui::inventory_screen.{h,cpp}` (pure
layout/hit-testing, mirrors `hud.h`'s own pure-logic/renderer split)
and `inventory_screen_renderer.{h,cpp}` (drawing). Opening it does
**not** pause the simulation - day/night and AI wander keep running,
matching the phase's own directive ("no pause in inventory"); only the
player's own movement/camera/mining/placing/crafting lock, via a new
`inventory_open` bool independent of the existing `menu_stack`-driven
`paused`. `ESC` closes the inventory instead of opening the pause menu
when it's open; the two states are kept mutually exclusive.

Real drag/drop: new `engine/items::inventory_ops.{h,cpp}` -
`inventory_left_click` (pick up/place/merge/swap a whole stack),
`inventory_right_click` (half stack / place one), `inventory_shift_click`
(transfer to another slot range or a whole other `Inventory`, backed by
a new `Inventory::add_item_to_range`) - pure logic, dispatched from real
mouse clicks in `client/main.cpp` (Interact=left button, PlaceBlock=right
button, `Crouch`=shift modifier, the same physical keys Minecraft itself
uses). The 2x2 crafting grid is a genuine `RecipeRegistry::find_match`
query against a separate 5-slot `Inventory`, recomputed on every input
change; a new `game:planks` item plus the phase's own suggested
`1 wood -> 4 planks` shapeless recipe give it a real, reachable first
recipe. Phase 23's quick-craft convenience path is untouched.

A real, previously-hit-and-fixed bug during this phase's own
verification: the new `LCU_VERIFY_INVENTORY` headless hook's first
draft silently never opened the inventory at all - its `input.set_down`
calls sat alongside `LCU_VERIFY_BREAK_PLACE`/`CRAFT`/`TORCH` later in
the frame, *after* the E-toggle/click-handling code that reads them had
already run that frame. Confirmed via the real log output (no
"Inventory opened" line), then fixed by moving the hook next to
`LCU_VERIFY_MENU`/`LCU_VERIFY_HUD`, which already sit earlier in the
frame for the identical reason - see DECISIONS.md. This also motivated
a new `Window::warp_mouse` (`SDL_WarpMouseInWindow`) - the first real
mouse-*position*-driven headless verification in this project (every
earlier UI hook, including `LCU_VERIFY_MENU`, used keyboard navigation
instead); confirmed empirically to work under `SDL_VIDEODRIVER=dummy`.

44 new unit tests (30 drag/drop, 2 `Inventory::add_item_to_range`, 8
inventory-screen layout/hit-testing, 3 `BlockItemMapping` reverse-lookup
regression). Verified via real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_
TORCH`/`LCU_VERIFY_CRAFT` regression runs (both updated for the new
slot-based hotbar - cycling now needs a real net-zero round trip via
`CycleHotbar`+`CycleHotbarPrev` to land back on the right slot before
placing), a real `LCU_VERIFY_INVENTORY` run in both the bgfx and
non-bgfx builds (full pipeline: pick up wood -> drop in craft grid ->
take the crafted 4 planks -> place in main inventory -> shift-click back
to hotbar -> close), real `LCU_VERIFY_MENU`/`LCU_VERIFY_HUD` regression
runs, a real two-process networked `LCU_VERIFY_BREAK_PLACE` run, and a
real `LCU_BUILD_SHADER_TOOLS=ON` build. `ctest` 506/506 (bgfx, up from
475) / 498/498 (non-bgfx, up from 467).

Honestly scoped: what the inventory screen actually looks like on a
real GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**;
shift-clicking a craft-grid slot lands anywhere in the whole 36-slot
inventory rather than hotbar-first-then-main (a real, minor
simplification vs. Minecraft's own precise ordering); a recipe needing
more than one of the same ingredient in a single 2x2 cell wouldn't be
correctly consumed by the result-click's "decrement each ingredient by
1" logic (documented at the call site, no registered recipe needs it
yet); still no icon/texture atlas (flat-color quads, unchanged since
Phase 44).

**Phase 50 (item entities + crafting table)**: closes Phase 17's own
long-standing "item pickup goes straight to inventory, no physical
dropped-item world entity" gap - real, physically-simulated item
entities. Breaking a block now spawns a real `game::components::
ItemEntity` + `Position` on the shared `entity_registry` (the same ECS
world AI entities already live in) with a real small upward toss;
`game::systems::update_item_entities` applies real gravity and ground
collision every frame by reusing `lcu::physics::apply_gravity`/
`move_and_collide` - the exact primitives the player's own controller
already uses, not a second physics implementation - plus a real
per-frame Y-axis spin (visual only) and a real 5-minute despawn.
`pickup_item_entities` adds the item to the inventory once the player's
own (inflated) AABB overlaps it and its 0.5s pickup delay has counted
down; a partial fit keeps the entity alive with its count reduced to
the real leftover. New `Renderer::submit_world_billboard` renders each
one as a small, real depth-tested camera-facing quad - unlike
`submit_billboard`'s own sky view (no depth test, correct for a sun
"at infinity" but wrong for something that needs to be genuinely
occluded by/occlude nearby terrain), reusing the same sky shader with
zero new shader files.

A real crafting table (`game:crafting_table`) opens a real workbench
screen on right-click: a 3x3 grid + result, plus the same main storage
and hotbar rows the regular inventory screen shows. The phase's own
directive described this as "like the inventory UI, but only
grid+result" - read literally that could mean a grid-only screen, but
a grid with no way to actually move items into it would be unusable
(the cursor stack can't reach a screen showing no storage at all). The
chosen, real reading is "reuse the inventory screen's shape; only the
grid itself differs (3x3, not 2x2)" - matches real Minecraft's own
crafting-table GUI and is what a working `LCU_VERIFY_WORKBENCH` run
actually needs (see DECISIONS.md for the full reasoning). New pure
`engine/ui::crafting_table_screen.{h,cpp}` + its own renderer, built
from `inventory_screen.h`'s shared building blocks rather than
touching its already-tested 2x2 grid; the real click dispatch reuses
the exact same `inventory_left_click`/`right_click`/`shift_click`
functions Phase 49 already built - no new drag/drop logic needed.
Breaking a crafting table drops itself, and `VoxelServer` now registers
`game:wood`/`game:crafting_table` too (Phase 49 only added wood
client-side - a real, now-closed parity gap), with `tracked_items`
grown from 4 to 6 entries.

A real bug was found and fixed *twice* during this phase's own headless
verification, not merely anticipated: an exact player-AABB-vs-item-AABB
pickup overlap almost never triggers in real play, since the block a
player breaks is typically one block in front of them, and a dropped
item with no horizontal velocity of its own can settle one or even two
blocks *below* the player's own standing height (mining straight down).
A real, measured 0.75 vertical inflate missed a real single-block-deep
item by 0.005 (an actual `LCU_VERIFY_BREAK_PLACE` failure - the item
never reached the inventory in time to place); 1.0 still missed a real
two-blocks-deep item (an actual `LCU_VERIFY_CRAFT` failure - the second
break's item was never picked up in an extended run). Fixed at a real
2.0 with margin, both fixes confirmed via a real re-run, not assumed.

17 new unit tests. Verified via a real `LCU_VERIFY_WORKBENCH` run
(both bgfx and non-bgfx - open -> pick up -> craft -> take result ->
close, proving the same shapeless "1 wood -> 4 planks" recipe works in
the bigger 3x3 grid too), real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_
TORCH`/`LCU_VERIFY_CRAFT`/`LCU_VERIFY_INVENTORY`/`LCU_VERIFY_MENU`/
`LCU_VERIFY_HUD` regression runs, a real two-process networked
`LCU_VERIFY_CRAFT` run, and a real `LCU_BUILD_SHADER_TOOLS=ON` build.
`ctest` 523/523 (bgfx, up from 506) / 515/515 (non-bgfx, up from 498).

Honestly scoped: what a dropped item or the workbench screen actually
look like on a real GPU/display is still **NOT VERIFIED — ENVIRONMENT
LIMITATION**; item entities have no horizontal scatter on spawn
(vertical toss only, a real documented simplification); the
workbench's own shift-click lands anywhere in the whole inventory
rather than hotbar-first (the same real, minor simplification the 2x2
grid already has); a recipe needing more than one of the same
ingredient in a single grid cell still isn't correctly consumed by
either result-click (documented, no registered recipe needs it yet).

**Phase 51 (health, hunger, fall damage, respawn)**: closes Phase 47's
own "hardcoded full this phase" gap - the HUD's real half-icon fill
math finally gets real values. New `game::components::PlayerHealth`/
`PlayerHunger` (plain structs, not ECS components - player state here
has never been an `entity_registry` entity, matching
`PlayerPhysicsState`'s own placement since Phase 4, see DECISIONS.md)
and `game::systems::player_vitals_system.{h,cpp}`: real Minecraft-
shaped fall damage (`fallDistance` accumulates only while airborne and
actually descending; damage is applied/reset only on the exact landing-
frame transition, so a normal jump deals zero, not just "usually
zero"), natural regen (+1 HP every real 4s at hunger>=18), starvation
(-1 HP every real 4s at hunger==0), hunger drain (-1 every real 30s
normally, 2x while sprinting-and-moving), a real per-jump hunger cost,
and eating.

Wired into `client/main.cpp`: `previous_player_y` captured before each
frame's own gravity/collision resolve, fed to `update_fall_tracking`
after - works identically for the single-player and networked physics
paths since both end up mutating the same `player.aabb`/
`player.grounded`. Hunger drain/regen/starvation tick every frame gated
on `paused` alone (menu_stack empty), same as day/night - vitals keep
ticking while the inventory/workbench screen is open, matching real
Minecraft (only movement/mining/placing/eating lock for those). Real
HUD wiring: `hud_state.health`/`hunger` now read straight from
`player_health`/`player_hunger` - Phase 47's own icon math needed zero
changes.

New `game:apple` (+4 hunger)/`game:bread` (+5 hunger) items with real
Minecraft restore values. Right-click dispatch is now a real three-way
branch: crafting-table intercept (Phase 50.3, checked first) -> eating
(new - deliberately doesn't require a raycast hit, matching Minecraft
letting you eat while looking at open air) -> normal slot-driven
placement (Phase 49, unchanged). Neither food item has a survival
obtain path yet (no farming, no mob drops - both explicitly out of this
phase's scope) - see Known Limitations below.

Death (from fall damage or starvation) drops the *entire* 36-slot
inventory as real item entities at the player's position - reusing
Phase 50's `ItemEntity`/`update_item_entities`/`pickup_item_entities`
pipeline wholesale (same spawn shape as a broken block's own drop, just
centered on the player), closes any open inventory/workbench screen,
and pushes a new "Du bist gestorben" `MenuScreen` (reusing Phase 46's
`MenuStack` framework entirely - zero new UI plumbing) with a single
Respawn row that resets position to the original spawn point plus every
vitals field/accumulator to its starting value.

24 new unit tests. New `LCU_VERIFY_HEALTH` hook (the project's
seventh): teleports the player 10 blocks above spawn with
`grounded=false` (the same direct-state-seed honesty
`LCU_VERIFY_WORKBENCH`'s own block seed already uses - a real jump
can't reach that height deterministically, but the fall from there on
is real, unmodified gravity/collision), seeds hunger below max, grants
an apple, then simulates a real right-click once the fall has had time
to land. A real run's log output proves both halves of this phase's own
directive: `Fall damage: 6.9 (health: 13.1/20.0)` then `Ate game:apple
(hunger: 14.0/20.0)`. Death+respawn were separately confirmed via a
real run with a temporarily-lethal fall height (`Player died (health
reached 0)` / `Player died - inventory dropped as item entities`) -
the respawn button itself reuses the exact same `MenuStack`/
`pending_menu_action` machinery `LCU_VERIFY_MENU` already proves works,
so it wasn't re-verified via a second simulated click sequence.

**A real, confirmed (not merely suspected) single-player-only gap**: in
networked mode, the synthetic teleport is invisible to `VoxelServer`'s
own authoritative simulation - the server only ever learns the
player's position from real `PlayerInput` packets, so the very next
`PlayerCorrection` snaps the client back down before a real fall
distance can accumulate. Confirmed via an actual two-process networked
run: eating still verifies correctly (item state is real
client-authoritative state, unaffected by reconciliation), but no
`Fall damage:` line appears. Documented in the hook's own doc comment
and in DECISIONS.md, not silently worked around.

Verified via real `LCU_VERIFY_HEALTH` runs (bgfx + non-bgfx), a real
two-process networked run, real regression runs of every existing hook
(`LCU_VERIFY_BREAK_PLACE`/`CRAFT`/`TORCH`/`MENU`/`HUD`/`INVENTORY`/
`WORKBENCH` - all still pass), and a real `LCU_BUILD_SHADER_TOOLS=ON`
build. `ctest` 547/547 (bgfx, up from 523) / 539/539 (non-bgfx, up from
515).

Honestly scoped: no armor/enchantments reduce fall damage (out of this
program's scope entirely); `Action::Sprint` now drives hunger drain's
real 2x multiplier but still doesn't itself move the player any faster
(a real, pre-existing gap from Phase 43 - Sprint had no consumer at
all before this phase); apple/bread have no survival obtain path (no
farming/mob drops, both explicitly out of scope - see Known
Limitations); real fall damage isn't verifiable in networked mode (see
above).

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 547/547 passing (this build dir is
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 539/539 passing
(`ChunkMeshUpload.*` only exists in the bgfx build, since it needs a
real bgfx context). Covers Log, QualityProfile, Vec3, Mat4, FrameStats,
InputState, TouchInputBackend, Chunk, ChunkStorage, ChunkCoord,
BlockRegistry, GreedyMesher, JobSystem,
ChunkMeshUpload, World, Worldgen, ChunkSerializer, Raycast,
PlayerPhysics/AABB, FirstPersonCamera, MovementInput, ItemRegistry,
Inventory, InventoryOps, RecipeRegistry, ecs::Registry, block/sky light
propagation, AIWanderSystem, DayNightCycle, ItemEntitySystem,
PlayerVitalsSystem, BlockItemMapping, InventoryScreen,
CraftingTableScreen, Sequence, PacketHeader, Connection,
UdpSocket, Address, LoopbackIntegration, FragmentPayload,
FragmentReassembler, PositionInterpolator,
PredictionBuffer, ReplicationProtocol (incl. BlockAction/BlockChange/
ChunkData/ChunkDataFragment/InventoryUpdate),
LuaState, EventBus,
RegistryBindings, ModLoader, MenuStack, Hud, GenerateSineWave,
ComputeStereoPan, DistanceAttenuation. JobSystem
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
  chunks outside range, which was the wrong shape for the server's
  single `World` instance shared across every connected client back
  when only one client's range was tracked - Phase 16 instead
  hand-rolled the load-only half of the same radius logic directly in
  `server/main.cpp`/`client/main.cpp`. ~~The server's shared `World`
  never unloads anything~~ **Fixed** (Phase 20): the server now tracks
  every connected client's real interest set and unloads (saving to
  disk first) any chunk none of them still need - see NETWORKING.md
  "Interest-scoped chunk unloading, real chunk persistence, and
  disconnect detection". `World::update_streaming`
  itself is still real and still unit-tested, just still not the
  function either executable's own streaming trigger calls (Phase 20's
  unload logic is hand-rolled in `server/main.cpp` too, mirroring how
  Phase 16's load half was, since it needs the multi-client interest
  union `update_streaming` doesn't know about). The client's own
  `World` still never unloads - it only ever tracks one player's
  interest, so this hasn't surfaced as a correctness problem there.
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
  `game:stone` deeper. ~~Still no climate/biome/caves/ores/structures/
  vegetation/decoration~~ **Biome/caves/ores/vegetation fixed** (Phases
  39-41, see those phases' own CHANGELOG/PROJECT_STATE entries): three
  real climate biomes, real cave carving, two ore types, and
  single-column tree/cactus vegetation all now vary real worldgen
  content by position/depth/biome. Structures (brief section 21's one
  remaining pipeline stage) are still not implemented - out of scope
  for this 42-phase plan entirely, not deferred from any specific
  phase.
- ~~Chunk save/load (`engine/serialization::chunk_serializer`) is
  unit-tested in isolation but still not wired to any actual trigger in
  `VoxelClient` or `VoxelServer`~~ **Fixed** (Phase 20, server-side):
  `VoxelServer` now calls `save_chunk_to_file` for real before unloading
  any chunk no connected client's interest set still covers, and
  `load_chunk_from_file` when a client's interest returns to that coord
  - a real trigger, not just a tested-in-isolation primitive. Scoped to
  the current server process's own session directory (`<world>/chunks/`)
  - not separately verified as surviving a deliberate server restart as
  a product feature, and `VoxelClient` still has no save/load trigger of
  its own (no "save world" command in single-player). See NETWORKING.md.
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
- `CycleHotbar`'s selection (Phase 21) is a plain index over a fixed,
  hardcoded 3-entry list (`placeable_items` in `VoxelClient`), not
  driven by what the player's `Inventory` actually holds - cycling
  lands on `game:stone` even with zero in stock (the subsequent place
  attempt then just silently no-ops, same as any other empty-stack
  place attempt already did before this phase). No on-screen indication
  of the current selection exists either - only a log line.
- ~~`RecipeRegistry` has no crafting-grid caller anywhere - implemented
  and unit-tested standalone~~ **Fixed** (Phase 23): a new
  `Action::Craft` quick-craft trigger calls `find_match` for real
  against a grid auto-built from held items - see PROJECT_STATE.md
  "Phase 23" above. Still no *graphical* crafting-grid UI (no way to
  arrange items into specific cells - one Craft press against an
  auto-built grid is the entire interaction), the auto-built grid only
  correctly represents a recipe needing exactly one of each distinct
  ingredient type, and shaped-recipe matching still has zero real
  caller (only shapeless is exercised).
- ~~Three real block types now exist (`game:stone`/`game:grass`/
  `game:dirt`, Phase 17-18) with real terrain, collision, meshing,
  replication, and pickup-on-break - but placing a block still always
  places `game:stone` specifically; there's no hotbar/item-selection UI
  yet to choose what to place from a multi-item inventory~~ **Fixed**
  (Phase 21): a new `Action::CycleHotbar` lets the player choose which
  of the three `PlaceBlock` places next - see NETWORKING.md/
  PROJECT_STATE.md "Phase 21" above. Still no *graphical* hotbar (no
  on-screen slot rendering/selection highlight - selection is
  log-line-only, needs `engine/ui`'s texture-atlas work first), and
  selection is a plain fixed-list cycle, not an inventory-driven hotbar
  that only shows items actually held.
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
  ~~Still not interest-managed by distance in the sense of ever
  *unloading* anything - the server's shared `World` only ever grows~~
  **Fixed** (Phase 20): the server now unloads (saving to disk first)
  any chunk no connected client's real interest set still covers, and
  reloads from disk rather than regenerating if interest returns - see
  NETWORKING.md "Interest-scoped chunk unloading, real chunk
  persistence, and disconnect detection". Block *edits*
  (breaking/placing) **are** also replicated (Phase 13, see
  NETWORKING.md "Block edit replication") - server-authoritative,
  broadcast to every connected client *and* replayed in full to any
  client that connects later (`block_change_history`, so a late joiner
  still catches up), both verified via real multi-process runs.
  Server-side inventory (Phase 15, extended Phase 19, see NETWORKING.md
  "Server-side inventory") now covers all three real block/item pairs -
  `VoxelServer` keeps an authoritative per-client `Inventory`, gates
  placing an item-backed block on actually holding one, and corrects a
  client's optimistic guess via `InventoryUpdate` messages (one per
  tracked item) after every `BlockAction`, closing the "unrefunded on
  rejection" gap for `game:stone`/`game:grass`/`game:dirt` alike. What
  none of these phases covers: any block/item beyond those three isn't
  inventory-gated - ~~no general, data-driven block-id-to-item-id
  mapping, `item_for_block` is three explicit `if` checks, not
  configuration~~ **Fixed** (Phase 22): both sides now use a real
  `game::items::BlockItemMapping` table instead (see PROJECT_STATE.md
  "Phase 22" above) - still only three pairs actually registered today,
  so "any block/item beyond those three" remains accurate, just via a
  table that scales to a fourth pair with one call instead of a new
  branch. Server-side inventory has no persistence across a disconnect,
  and the
  edit history itself is unbounded for the server process's lifetime
  rather than compacted against persisted state (see NETWORKING.md).
  The shared `World`'s loaded-chunk set does now shrink back down
  (Phase 20 - see above), but the *edit history* used for late-joiner
  catch-up is separate from chunk persistence and still isn't compacted
  when a chunk it references gets unloaded/saved.
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
- Discovered but unconfirmed/unfixed (Phase 20 stress-testing): under
  extreme sustained packet volume (tens of real seconds of an
  unthrottled test client sending `PlayerInput` every frame), server-
  simulated player movement appeared to stall after crossing roughly
  one chunk boundary and never progress further, even given 200+
  additional real seconds of client runtime. Suspected but not
  confirmed root cause: `PlayerInput`'s `UnreliableSequenced` channel's
  `u16` sequence-wraparound comparison (`sequence_greater_than`)
  misjudging "newer vs. older" once the counter has wrapped multiple
  times between processed packets, causing the server to start
  discarding all further updates as stale. Not fixed - would need
  reading/testing `Connection`/`sequence_greater_than` specifically,
  out of Phase 20's scope; verification instead used a reliable
  ~15-second single-crossing movement window instead of one continuous
  long-distance run. See NETWORKING.md.
- `kClientTimeoutSeconds` (Phase 20's disconnect-detection idle
  threshold, `5.0f`) is a placeholder value chosen for fast, reliable
  test iteration, not tuned against any real-world latency/jitter/
  packet-loss data - a real deployment over an actual internet
  connection (rather than loopback) would likely need this higher.
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
- Mobile/Windows builds are untested from this Linux-only sandbox;
  `CMakePresets.json` presets exist for them but have not been
  exercised on their native toolchains. macOS (Phase 25) was
  code-audited - every CMake/FetchContent path and macOS-specific
  branch was read directly, and one real bug was found and fixed
  (`active_shader_profile_dir()` had no Metal case - see
  DECISIONS.md/BUILDING.md) - but actually running `cmake --build`
  against a real Mac toolchain still hasn't happened from this
  sandbox, so it stays **NOT VERIFIED — ENVIRONMENT LIMITATION**, not
  TESTED, until someone with a real Mac runs the commands in
  `BUILDING.md`.
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
- ~~No texture atlas/UV mapping validation — `MeshVertex.u`/`.v` are
  populated (quad-local, in block units) but nothing downstream
  consumes or checks them yet, since there's no atlas (Phase 12).~~
  **Fixed** (Phases 53-56): a real 256x256 procedural-texture atlas
  exists, `MeshVertex` gained `texture_index`, and every real block/item
  in the game now renders through it - see CHANGELOG.md/BUILD_STATUS.md.
- Shader compilation (`LCU_BUILD_SHADER_TOOLS`) is opt-in and OFF by
  default — most builds/CI runs won't have a real draw call unless this
  is explicitly turned on, since it adds real build time (shaderc +
  glslang/SPIRV-Tools/SPIRV-Cross/Dawn-Tint).
- ~~Chunk shaders have no texturing — flat lit color only~~ **Fixed**
  (Phases 26, then 53-55): Phase 26 added per-block/per-face color plus
  procedural noise; Phases 53-55 added real atlas texturing on top - the
  chunk shader now samples `s_atlas` per face using each block's own
  top/side/bottom texture, mixed with the existing light factor.
  `LCU_USE_TEXTURES=0` still keeps the Phase 26 color-only path working
  as a real fallback (not just theoretically - both paths are exercised
  by real headless runs every phase since 53).
- No visual verification of any rendering exists or can exist in this
  sandbox — every claim above about the draw call is about the API
  calls succeeding (valid handles, no crash, bgfx accepts the shader
  binaries), not about correct-looking output on a screen.
- Registries beyond `BlockRegistry`/`ItemRegistry` (`EntityRegistry`,
  `BiomeRegistry`, `StructureRegistry`, `SoundRegistry`,
  `CommandRegistry`) don't exist yet — Lua bindings only cover the two
  registries that were already real before Phase 9; the others are
  added when something actually needs them (brief section 98).
- ~~`EventBus` only has one real event (`block_broken`)~~ **Fixed**
  (Phase 24): `item_crafted` is a second real event, following the
  exact pattern the Phase 9 note below asked for. No entity-spawned/
  player-joined/tick/... events exist yet since nothing in the engine
  fires them; both real events today are client-triggered content
  moments (breaking, crafting) - nothing server-side fires one yet.
  Add another `emit_<event>()` the same way once a third real event
  exists (see DECISIONS.md).
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
  number today), no texture/shadow/particle quality tiers; a real
  texture atlas exists now (Phase 53) but it's a single fixed
  256x256 resolution with no lower-quality tier to switch to, and
  there's still no shadow/particle system of any kind to have a
  quality tier for.
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
- ~~`engine/ui::draw_debug_overlay` (and every other debug-text call
  site - HUD item counts, menu labels, inventory/workbench slot counts)
  still uses bgfx's built-in VGA-style debug-text character buffer, not
  a real font/texture-atlas text renderer~~ **Fixed** (Phase 57): a
  real, own-design procedurally-generated bitmap-font atlas
  (`engine/assets::font_atlas.h`, ASCII 32-126, separate from the
  Phase 53 block atlas) plus `engine::ui::TextRenderer` is now the
  DEFAULT text-drawing path for all five of those functions, at real
  pixel positions (no more character-cell rounding). bgfx's own
  debug-text buffer stays available as a real, working fallback via
  `LCU_LEGACY_DEBUG_TEXT=1` - not removed, just no longer the default.
  Text is still monospace-only (no kerning) and limited to the same
  ASCII 32-126 range the font atlas covers (a character outside that
  falls back to '?').
- The on-screen touch-control legend has no interactive elements of its
  own (no buttons a mouse/gamepad can click) — it draws where
  `TouchInputBackend`'s real touch-button rects are, for a player to
  see, but a desktop/gamepad player can't interact with it as a menu;
  `engine/ui` is presentation-only so far, not an input-routing/focus
  system for non-touch input devices.
- No content pipeline exists for *imported/authored* models/textures/
  sounds — "content" in brief section 96's Phase 12 sense (real files
  authored outside this codebase and loaded at runtime) is still
  entirely absent; every visual/audio element in this project remains
  generated in code, not loaded from an asset file. This is now a
  deliberate, standing scope decision, not just an unaddressed gap: the
  Phase 53-57 directive itself fixed procedurally-generated 16x16
  MC-style textures (Phase 54, `engine/assets::procedural_textures`)
  and a procedurally-generated bitmap font (Phase 57) as the *real*
  content pipeline for this project, specifically to avoid real MC
  texture/font assets and their licensing - see DECISIONS.md. Phase
  53's optional stb_image-based PNG loader (for a debug atlas dump only,
  never a real content-loading path) was itself deliberately not
  implemented either - see that phase's own DECISIONS.md entry.
- The `vs_sky.sc`/`fs_sky.sc` shader family (skybox, wireframe/solid
  boxes, and world billboards - `submit_world_billboard`, used for
  dropped item entities) has no alpha blending (Phase 56 added real
  texture sampling to this family but deliberately did not add
  blending, out of that phase's own scope). A dropped item whose
  texture has transparent pixels (e.g. `game:torch`'s flame/stem
  cutout) renders those pixels solid black on its world billboard
  rather than see-through - a real, accepted visual gap, not a bug, see
  DECISIONS.md. Real leaf transparency has the identical shape of gap
  (`game:leaves`'s alpha-0 holes render solid in-chunk too, unaffected
  by Phase 56 since chunk rendering is a separate shader).
- The Phase 58 character model's body yaw reads directly from
  `camera.yaw` (the body always faces exactly where the camera looks,
  horizontally) rather than a real independent, movement-driven facing
  direction that lags behind the camera the way actual Minecraft's own
  body/head yaw system does - a real, deliberate simplification (no
  gameplay system in this project yet distinguishes "look direction"
  from "movement direction" in any way a player could notice), see
  DECISIONS.md. The player gets no idle/breathing animation (Phase 59
  adds that for NPCs specifically, per the brief's own phasing - the
  player itself still doesn't get one). Third-person camera distance
  (`kThirdPersonDistance`) still has no real wall-collision pull-in - a
  pre-existing gap from Phase 47, unchanged by Phase 58's own real
  third-person-front addition.
- The Phase 59 NPC walk-cycle animation speed is a fixed real constant
  (`kNpcWalkCycleFrequency`), not scaled by each real `AIWander::
  speed` - every NPC's legs swing at the same rate regardless of how
  fast that specific NPC is actually moving, a real, deliberate
  simplification (see DECISIONS.md; adding a real per-NPC rate would
  mean exposing gameplay movement state to rendering purely for a
  cosmetic need). Real networked remote-player avatars still have no
  visible character model (only local `AIWander` NPCs do) - a remote
  player still renders as, at most, a debug wireframe box when
  `options.debug_overlay_enabled` is on, nothing at all otherwise.
- **No mobs** — no hostile/passive/neutral entity content of any kind
  (only the pre-existing wandering AI/item entities exist). **No
  redstone** — no wiring/logic-gate/mechanism content. **No
  enchantments/anvil/potions** — no enchanting table, anvil repair, or
  brewing. **No Nether/End** — a single overworld dimension only. **No
  villagers/trading**. **No structures** (out of scope since Phase
  38-41's own worldgen phases, still true). **No farming** — no crops,
  no way to grow/harvest food; `game:apple`/`game:bread` (Phase 51)
  therefore have no survival obtain path, only a direct debug-style
  grant (`LCU_VERIFY_HEALTH`'s own setup). **No chat/server browser** —
  networked mode is still connect-by-port only, no in-game text
  communication. **No skin customization**. All of these are explicit,
  standing exclusions from the current multi-phase directive, not
  phases that were attempted and fell short.
- Fall damage has no armor/enchantment mitigation — `fall_damage_for_
  distance` (Phase 51) is a flat `distance - 3` with nothing to reduce
  it, matching this project's real current scope (no armor/enchantment
  content exists at all, see above).
- `Action::Sprint` drives hunger drain's real 2x multiplier (Phase 51)
  but still doesn't itself move the player any faster — a real,
  pre-existing gap from Phase 43 (the action was bound with no consumer
  at all before Phase 51 gave it one), not something this phase's own
  "more when sprinting" hunger-drain requirement needed to close to be
  satisfied.
- Real fall damage isn't verifiable in networked mode (Phase 51,
  `LCU_VERIFY_HEALTH`) — the hook's synthetic mid-air teleport is
  invisible to `VoxelServer`'s own authoritative simulation (the server
  only learns position from real `PlayerInput`), so the next
  `PlayerCorrection` snaps the client back down before a real fall
  distance can accumulate; confirmed via an actual two-process run, not
  assumed — see DECISIONS.md. Eating verifies correctly in networked
  mode regardless (item state is real client-authoritative state).

## Next Task

Per the master brief: the goal is a complete playable game, not a
completed checklist - work continues past the original 12-phase queue.
Next up, in priority order (brief section 10 - multiplayer fundamentals
before content/polish):

1. ~~Interest-scoped chunk unloading~~ **Done (Phase 20)**: the
   server's shared `World` now unloads (saving to disk first) any
   chunk no connected client's real interest set still covers, and
   reloads from disk rather than regenerating if interest returns -
   see NETWORKING.md. A disconnect-detection timeout and the
   `std::optional` sentinel fix that made it safe were both closed in
   the same phase.
2. ~~Placing grass/dirt~~ **Done (Phase 21)**: a new `Action::
   CycleHotbar` lets the player choose which of stone/grass/dirt
   `PlaceBlock` places next - see PROJECT_STATE.md "Phase 21" above.
   Still no graphical hotbar UI (log-line-only selection feedback).
3. ~~A general, data-driven block-id-to-item-id mapping~~ **Done
   (Phase 22)**: a new `game::items::BlockItemMapping` table replaces
   the hardcoded `if` chains on both client and server - see
   PROJECT_STATE.md "Phase 22" above. Still only three pairs actually
   registered (stone/grass/dirt) and each process populates its own
   table independently (not synced), but adding a fourth is now one
   `register_pair` call per side, not a new branch in two files.
4. ~~A real crafting-UI caller for the already-implemented
   `RecipeRegistry`~~ **Done (Phase 23)**: a new `Action::Craft`
   quick-craft trigger calls `find_match` for real - see
   PROJECT_STATE.md "Phase 23" above. Still no graphical crafting-grid
   UI, and the auto-built query grid only correctly represents
   one-of-each-distinct-ingredient recipes.
5. ~~Modding depth (a second real event beyond `block_broken`)~~
   **Done (Phase 24)**: `item_crafted` is a second real event,
   `example_mod` subscribes to both - see PROJECT_STATE.md "Phase 24"
   above. Both real events are still client-triggered content moments;
   nothing server-side fires one yet, and `ModLoader` still has no
   manifest/dependency/version format.
6. **Active, user-directed program (Phases 25-42)**: the user wants to
   run `VoxelClient` on a real Mac and see it for the first time -
   visible colored terrain, a skybox, real global cross-chunk lighting
   (with explicit performance constraints - never per-frame, bounded
   BFS, off the JobSystem), procedural terrain with sea level at y=0,
   water, biomes, caves/ores, and vegetation. This supersedes brief
   section 10's generic "more content" item above with a concrete,
   larger-scoped plan. Done so far: ~~Phase 25 (macOS build audit,
   one real Metal-shader-profile bug found and fixed)~~, ~~Phase 26
   (visible terrain: per-block/per-face colors + procedural shader
   noise)~~, ~~Phase 27 (skybox + sun/moon, view-ordered occlusion, 2
   real bugs found and fixed)~~, ~~Phase 28 (renderer consumes real
   per-voxel light, duck-typed LightStorageT template parameter to
   avoid a circular engine/voxel<->engine/lighting dependency, a real
   vertex-stride bug found and fixed)~~, ~~Phase 29 (WorldLight data
   structure - cross-chunk-aware query surface, no propagation yet)~~,
   ~~Phase 30 (sky-light cross-chunk propagation - a seeded column
   scan, not a BFS; client/main.cpp's load loops restructured for
   top-down ordering)~~, ~~Phase 31 (block-light cross-chunk
   propagation - a genuine BFS crossing chunk boundaries via
   WorldLight, duck-typed ChunkProviderT)~~, ~~Phase 32 (boundary
   buffer - skipped, optional, no concurrency to buffer against yet)~~,
   ~~Phase 33 (VoxelClient integration - real touched_chunks remesh
   tracking - + smooth per-vertex lighting, no shader changes
   needed)~~ - see PROJECT_STATE.md "Phase 25" through "Phase 33"
   above and TASK_QUEUE.md for full per-phase detail. Next: Phase 34
   (torch block + lighting benchmarks - explicit numeric performance
   budgets), Phase 35 (unload marks neighbors dirty - also closes
   Phase 30/31's networked-ChunkData/late-loading ordering gaps),
   entity rendering/debug overlay (36), then the worldgen phases
   (37-41: sea level, water, continents, biomes, caves/ores,
   vegetation), then docs (42).
7. Lower priority, opportunistic: confirm/fix the Phase 20 stress-test
   finding (a suspected `UnreliableSequenced` sequence-wraparound issue
   in `Connection`/`sequence_greater_than` under extreme sustained
   packet volume) - deferred since it needs dedicated networking-code
   investigation, not a quick fix, and hasn't affected any real
   verification run at this vertical slice's normal traffic volume.
8. ~~Active, user-directed program (Phases 43-52)~~ **Done**: a third
   program after items 6/above - rebindable input (43), a 2D UI
   framework (44), persistent options (45), the menu/pause framework
   (46), a real HUD (47), block highlight/hold-to-break/hand (48), the
   inventory screen + drag/drop + crafting grid (49), item entities + a
   crafting table (50), health/hunger/fall damage/respawn (51), and a
   documentation pass (52 - README controls table, BUILDING
   options.txt note, a DECISIONS.md entry on the standing exclusion
   list's own rationale) are all done - see PROJECT_STATE.md "Phase 43"
   through "Phase 51" above and TASK_QUEUE.md for full per-phase detail.
   Next: no further phase is currently queued - future work should be
   directed by the user (mobs/redstone/enchantments/Nether/villagers/
   structures/farming/chat/skins were all explicitly out of scope for
   this program, see DECISIONS.md "Closing Phases 43-52").

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
