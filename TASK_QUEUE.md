# Task Queue

`[ ]` TODO   `[~]` IN PROGRESS   `[x]` COMPLETE   `[!]` BLOCKED

A task is never marked `[x]` unless it was actually built and, where it
produces a runnable artifact, actually run and observed to behave
correctly in this environment. See `BUILD_STATUS.md` for exactly what was
verified and how.

## Phase 0 — Repository + build system + state system

- [x] Create engine/game/client/server/tools/third_party/mods/examples/tests directory structure.
- [x] Write PROJECT_STATE.md, ROADMAP.md, TASK_QUEUE.md, BUILD_STATUS.md, ARCHITECTURE.md, DECISIONS.md, CHANGELOG.md.
- [x] Root CMakeLists.txt with LCU_BUILD_* options, CMakePresets.json for linux/windows/macos/android/ios.
- [x] third_party/CMakeLists.txt: FetchContent for fmt, SDL3, GoogleTest, bgfx.cmake (bx/bimg/bgfx); third_party/README.md dependency table.
- [x] engine/core: types.h, log.h/.cpp (fmt-backed), assert.h/.cpp. Unit tested.
- [x] engine/math: Vec3, Vec4, Mat4 (translation/scale/perspective/look_at). Unit tested.
- [x] Verify full non-bgfx build + ctest + run VoxelClient (headless) + run VoxelServer + confirm VoxelServer has zero SDL/bgfx link dependency via ldd.

## Phase 1 — SDL3 + bgfx + window + game loop + input

- [x] engine/platform: Window (SDL3-backed), event pump, resize handling. Unit-of-work tested via VoxelClient headless run.
- [x] VoxelClient: opens window, runs loop, clean shutdown. Verified headless (SDL_VIDEODRIVER=dummy); **not** verified with a real display/GPU (none available in this sandbox) — needs confirmation on a machine with a display.
- [x] bgfx integration: fetch + build validation of the bgfx.cmake wrapper. Required installing libgl1-mesa-dev/libglu1-mesa-dev/mesa-common-dev/libwayland-dev in this sandbox (see DECISIONS.md); documented in BUILDING.md for other environments.
- [x] engine/rendering: bgfx init against the SDL3 native window handle (engine/platform::get_native_window_handle, X11/Wayland/Win32/Cocoa/UIKit/Android branches), first cleared frame. Verified headless: falls back to bgfx::RendererType::Noop when no native handle is available (dummy SDL driver) and completes a 5-frame clear/frame loop cleanly. **Not** verified: real Vulkan/GL backend actually presenting on a real display — no GPU/display in this sandbox.
- [x] Input abstraction (engine/platform): Action enum (MoveForward/Jump/Interact/...) + InputState + KeyboardInputBackend (SDL_GetKeyboardState-based). Gamepad/touch backends deferred to Phase 10 (brief sections 27-28) — not built speculatively now. Unit tested (InputState set/is_down); KeyboardInputBackend itself untested by unit test (needs a live SDL keyboard state) but exercised every VoxelClient run.
- [x] Debug overlay skeleton (engine/debug::FrameStats): FPS/avg frame time text line, reported once per second. Pure accumulator, no SDL/bgfx dependency (reusable by VoxelServer for tick-rate reporting later). Unit tested; also verified via a real 2-second VoxelClient run producing actual fps=60165.4 frame_ms=0.02 output. CPU/GPU/RAM/chunks/entities/ping/bandwidth/draw-calls/jobs lines from brief section 60 are added once the systems producing those numbers exist — not stubbed out now.

## Phase 2 — Voxel storage + chunk + meshing + rendering

- [x] engine/voxel: chunk storage. `ChunkStorage<EdgeLength>` template (default 16x16x16 via `Chunk = ChunkStorage<16>`), flat contiguous `BlockId` (u16) array, no per-block C++ instance. Alternative chunk sizes proven via a unit test with `ChunkStorage<8>`. Block *state* (rotation/orientation/powered/etc., brief section 17) is not encoded yet — `BlockId` alone for now; state packing is added once a block that needs it exists (e.g. a directional block in the example mod, Phase 9), not speculatively.
- [x] engine/voxel: `world_to_chunk_and_local()` — correct floor-division coordinate splitting (`ChunkCoord` + `LocalBlockCoord`), the ARCHITECTURE.md "Coordinate spaces" piece needed before world storage/streaming can be built.
- [x] BlockRegistry (landed in `engine/voxel` — recorded in DECISIONS.md; revisit only if `engine/modding`'s registry needs pull it elsewhere later). Namespaced ids (`game:stone`), air always id 0, datadriven `BlockDefinition` (hardness/transparency/collision/light_emission). Block *tags* and full mod-facing registration API are Phase 9 work.
- [x] Greedy meshing, hidden-face removal, opaque layer. `mesh_chunk_greedy<EdgeLength>()`: axis-sweep algorithm, registry-driven opacity (not hardcoded air checks), merges same-block same-facing coplanar faces. Transparent/water layers exist structurally (`ChunkMesh::transparent`/`::water`) but are always empty — no transparent block exists yet to mesh, and transparent-vs-transparent face rules are deliberately deferred (see DECISIONS.md) rather than guessed. Winding verified via a geometric cross-product check on every emitted triangle (no display available to check visually). 8 unit tests.
- [x] Job system (engine/jobs) — `JobSystem`: worker pool, priority scheduling, dependency graphs with cascading cancellation, cancellation of not-yet-started jobs. Correctness-first (one mutex+condvar), not yet lock-free/work-stealing — revisit only if Phase 11 profiling shows it matters (see DECISIONS.md). 12 unit tests covering ordering/dependencies/cancellation/priority, plus 200 repeated runs and 50 runs under ThreadSanitizer with zero failures/races. Now has a real consumer: `VoxelClient` dispatches `mesh_chunk_greedy` through it.
- [x] Wire meshing through `JobSystem` and feed `ChunkMesh` into `engine/rendering` -> bgfx as actual GPU vertex/index buffers. `upload_chunk_mesh_layer`/`destroy_gpu_chunk_mesh`, 3 unit tests, and a real end-to-end `VoxelClient` run (256-block slab -> job-dispatched mesh -> 6 merged quads -> real bgfx buffer handles, `valid=true index_count=36`).
- [x] Real draw call. `client/shaders/{vs_chunk,fs_chunk}.sc` (minimal directional+ambient lighting, no texturing - no atlas yet), compiled via bgfx's `shaderc` (opt-in `LCU_BUILD_SHADER_TOOLS`, see DECISIONS.md), loaded at runtime (`engine/rendering::load_chunk_program`, profile selected by active bgfx renderer type), submitted every frame via `Renderer::submit_chunk_mesh()`. Verified: `"Chunk shader program valid=true"` and 3 clean frames with the draw call actually executing, under bgfx's `Noop` backend. **What's not verified**: what it looks like on a real GPU/display - none exists in this sandbox.

## Phase 3 — World generation + streaming + save

- [x] Deterministic worldgen pipeline: continental+terrain stages only (`engine/world::worldgen::terrain_height`, seeded 4-octave value noise; same seed+coord always same height, different seeds differ, adjacent columns smooth not random - all three properties unit tested). Climate/biome/caves/ores/structures/vegetation/decoration deferred - no biome/structure types registered anywhere to drive them yet (see DECISIONS.md).
- [x] Chunk lifecycle state machine (`engine/world::World`, `ChunkLifecycleState`) matching ARCHITECTURE.md: Unloaded -> Requested -> Generating -> Generated driven directly; Lighting/Meshing/GpuUpload/Ready/Visible states declared but not yet driven by World itself (Lighting is Phase 6; meshing/GPU upload already exist in `engine/voxel`/`engine/rendering` but aren't wired through World's state machine yet - that's client-side bookkeeping once more than one chunk streams).
- [x] World streaming by distance (`World::update_streaming`, Chebyshev radius with load/unload hysteresis). **Not yet** by view direction/movement direction/player position specifically - no camera/player exists yet to supply those signals (Phase 4); known simplification recorded in DECISIONS.md (streams a 3D cube, not a horizontal disc).
- [x] Save/load: `engine/serialization::chunk_serializer`, versioned format (`kChunkFormatVersion`), corruption detection via zstd's content checksum, version-mismatch detection - all with dedicated tests, not just a happy-path round-trip. No silent overwrite: a failed load leaves the caller's chunk untouched (tested).
- [x] Compression library: zstd (decision + rationale in DECISIONS.md, dependency table in third_party/README.md).

## Phase 4 — Player + physics + interaction

- [x] AABB + voxel collision, gravity, jump/crouch/swim/step (`engine/physics::move_and_collide`/`integrate_player`, 18 unit tests, including a hand-verified auto-step regression and a dedicated-ground-probe grounding fix - see DECISIONS.md).
- [x] Voxel DDA raycaster (`engine/physics::raycast`, Amanatides & Woo, 9 unit tests incl. a hand-computed exact-distance case).
- [x] First-person camera, block break/place (`engine/player::FirstPersonCamera`/`movement_direction_from_input`, 12 unit tests; `VoxelClient` now builds a multi-chunk `World`, spawns a physics-driven player on the terrain surface, and wires raycast hits into `World::chunk_at_mutable()` for edge-detected break/place, remeshing the affected chunk plus any chunk sharing the mutated boundary - verified end-to-end with a real headless run, not a mock).

## Phase 5 — Items + inventory + crafting

- [x] `engine/items::ItemRegistry`/`ItemDefinition` - namespaced, datadriven, mirrors `BlockRegistry` (`kNoItemId` reserved like `kAirBlockId`). 5 unit tests.
- [x] `engine/items::Inventory` - slot-based `ItemStack` storage, `add_item`/`remove_item`/`count_item`, respects each item's `max_stack_size`. 10 unit tests incl. partial-stack top-up before spilling into a new slot, and leftover-on-full behavior.
- [x] `engine/items::RecipeRegistry` - shaped (bounding-box-trimmed, exact orientation) and shapeless (ingredient-multiset) recipe matching. 9 unit tests. No crafting-UI caller yet (tested standalone, same as `BlockRegistry`/`ItemRegistry` were before their first real callers existed).
- [x] `VoxelClient`: breaking a block now hands the player a real `game:stone` item via `Inventory::add_item` (block-break's first item consumer); placing a block now consumes one from the inventory via `Inventory::remove_item`, refunding it if the target chunk turns out not to be loaded. Verified end-to-end via the existing `LCU_VERIFY_BREAK_PLACE` headless hook: break picks up 1 stone (inventory: 1), place consumes it (inventory: 0).

## Phase 6 — Entities + AI + lighting + day/night

- [x] `engine/ecs::Registry` - generation-checked `EntityId` handles, sparse-set `ComponentPool<T>` per component type (dense contiguous storage, swap-and-pop removal). `create`/`destroy_entity`, `add`/`get`/`has`/`remove_component`, `pool_for<T>()` for dense iteration. No query DSL/archetypes/multithreaded dispatch - not needed yet. 13 unit tests.
- [x] Sunlight + block light propagation/removal. `engine/lighting::LightStorage` (packed 4-bit sky + 4-bit block light per voxel). `compute_block_light`/`compute_sky_light` for the initial per-chunk flood; `propagate_added_block_light`/`unpropagate_block_light` for true incremental local updates on a single block add/remove (the standard two-phase BFS removal algorithm, not a full recompute per edit) - see DECISIONS.md for the single-chunk-scope simplification. 12 unit tests, incl. an exact-match check between incremental and full-recompute paths and a two-source removal/refill test.
- [x] Simple AI, day/night cycle. `game::components::{Position, AIWander}` + `game::systems::update_ai_wander` (idle/walk-to-target/pick-new-target loop, explicit `std::mt19937` for determinism); `game::systems::DayNightCycle` (cosine sky-light-scale curve, full at noon, dim floor at midnight). 13 unit tests.
- [x] `VoxelClient`: spawns 3 wandering AI entities and a `DayNightCycle`, both updated every frame; computes real per-chunk block+sky light at load and keeps it correct through every break/place edit via the incremental primitives (not a full recompute), plus a per-column sky light refresh. Also fixed a real off-by-one found while verifying this - the player (and AI) previously spawned embedded one block into the ground due to misreading `terrain_height()`'s solid/air boundary; now spawns resting on top of it, as documented.

## Phase 7 — Networking + dedicated server

- [x] `engine/network` transport - all four channels ARCHITECTURE.md commits to (`UnreliableUnordered`, `UnreliableSequenced`, `ReliableUnordered`, `ReliableOrdered`) over a hand-rolled ack/retransmit protocol on UDP (`UdpSocket`, `Connection`, `PacketHeader`, `sequence_greater_than`). See `NETWORKING.md` for the wire format. 46 unit tests, including two full loopback integration tests over real sockets (one deliberately drops a real datagram and verifies retransmission recovers it).
- [x] Server-authoritative state, `VoxelServer` real simulation loop. Replaced the Phase 0 sleep-only placeholder: `VoxelServer` now generates/loads a real `World`, runs the same wandering-AI simulation (`engine/ecs` + `game::systems::update_ai_wander`) as `VoxelClient`, and listens for real UDP connections - a peer is "connected" on its first datagram, gets a real `ReliableOrdered` Welcome (world seed + tick rate), and receives a per-tick `UnreliableSequenced` Heartbeat. Verified via a real two-process test: a standalone Python UDP client connects to a running `VoxelServer` and receives the real handshake + live heartbeats. `ldd` reconfirmed zero SDL/bgfx dependency.

## Phase 8 — Replication + prediction + interpolation

- [x] Client-side prediction + reconciliation: `engine/replication::PredictionBuffer<State, Input>` (generic; predicts immediately, replays pending inputs on top of a server correction). Wired into `VoxelClient`/`VoxelServer` for real player movement - server runs the same physics per `PlayerInput` it receives and reports back via `PlayerCorrection`.
- [x] Remote entity interpolation: `engine/replication::PositionInterpolator` (buffered timestamped samples, linear interpolation at a small render delay, no extrapolation). Wired into `VoxelClient`: when connected, AI entities are rendered from server `EntityState` samples through this instead of being simulated locally.
- [x] Interest management: `VoxelServer` filters each client's `EntityState` broadcast to entities within `kInterestRadius` of that client's own position - real filtering logic (see NETWORKING.md for why it isn't visibly exercised yet in this small a world).
- [ ] Chunk network streaming + compression - deferred: needs message fragmentation (a compressed chunk doesn't fit in one UDP datagram) which `engine/network::Connection` doesn't implement yet. See DECISIONS.md/NETWORKING.md.
- [x] `game::systems::protocol`: shared client/server wire messages (`Welcome`, `Heartbeat`, `EntityState`, `PlayerInput`, `PlayerCorrection`), 11 unit tests.
- [x] Real two-process multiplayer verification: `VoxelClient` connects to a running `VoxelServer` over real loopback UDP - Welcome received, remote AI positions interpolated, player input sent and a server correction received and reconciled.

## Phase 9 — Modding + registries + Lua + events

- [x] Add Lua dependency: official upstream Lua 5.4.7 via its own `onelua.c` amalgamation (`-DMAKE_LIB`), `FetchContent_Populate` since upstream ships no CMake support. `engine/scripting::LuaState` - RAII VM wrapper, sandboxes to base/table/string/math (no io/os/package), forward-declares `lua_State` so `<lua.h>` stays confined to `engine/scripting`/`engine/modding` `.cpp` files. 7 unit tests.
- [x] Registries: `bind_block_registry`/`bind_item_registry` (`engine/modding::registry_bindings`) expose `register_block`/`register_item` to Lua, writing into the same `BlockRegistry`/`ItemRegistry` the base game uses. (Entity/Biome/Recipe/Structure/Sound/Command registry bindings deferred - no such registries exist yet beyond RecipeRegistry, which has no mod-facing use case yet either; added when something needs them.) 6 unit tests.
- [x] Event system: `engine/modding::EventBus` - `lcu.subscribe(event_name, fn)` from Lua, `emit_block_broken(x, y, z, block_id)` from C++, erroring handlers logged and skipped without blocking others. 7 unit tests.
- [x] Mod loader: `engine/modding::ModLoader` - enumerates `<mods_dir>/<mod_name>/init.lua`, one shared `LuaState` per host process, a mod that errors is skipped not fatal. 6 unit tests.
- [x] `example_mod` (brief section 91): `mods/example_mod/init.lua` - registers `example_mod:magic_stone`/`example_mod:magic_wand`, subscribes to `block_broken`, logs every block broken. Wired into both `VoxelClient` and `VoxelServer` (each loads `mods/` at startup into its own `LuaState`+registries). Verified via real runs: server logs its registration lines and `Loaded 1 mod(s) from 'mods'`; client (under `LCU_VERIFY_BREAK_PLACE`) additionally logs `[example_mod] block_broken #1: block id 1 broken at (0, 28, -1)` at the exact moment a real block is broken.

## Phase 10 — Mobile + touch + Android + iOS

- [x] Touch input mapped through the same input-action abstraction as desktop: `engine/platform::TouchInputBackend` (twin-virtual-stick layout - movement drag on the left half of the screen, look drag on the right, fixed button rects for Jump/Interact/PlaceBlock/Sprint/Crouch/Inventory) writes into the same `InputState` `KeyboardInputBackend` does, so `MovementInput`/`FirstPersonCamera`/the break-place loop are unmodified and unaware of the input source. Pure logic, no SDL dependency (real finger events aren't available to wire up in this sandbox - see Known Limitations) - fully unit tested (13 tests) via synthetic `TouchPoint` lists.
- [x] Quality profiles (MOBILE_LOW/MEDIUM/HIGH, plus Desktop): `lcu::core::QualityProfile`/`chunk_load_settings_for` (in `engine/core`, not `engine/platform`, since `VoxelServer` needs it too and must stay SDL/bgfx-free). `Desktop` matches this project's pre-existing hardcoded chunk-load radius/vertical-range exactly (zero behavior change by default); each Mobile tier trims both, down to a single chunk for `MobileLow`. Wired into both `VoxelClient` and `VoxelServer` via a new `LCU_QUALITY_PROFILE` env var. 4 unit tests, plus verified via real runs: default (`Desktop`, and an unrecognized `LCU_QUALITY_PROFILE` value falling back to it) still logs "Loaded 36 chunks" exactly as before; `mobile_low`/`mobile_high` log "Loaded 1 chunks"/"Loaded 27 chunks" respectively.
- [ ] Real Android Gradle/NDK project structure, real iOS Xcode project generation. `CMakePresets.json`'s `android-arm64`/`ios` presets were re-verified this phase - `cmake --preset android-arm64` correctly reaches and fails only at Android's own NDK-detection step ("Neither the NDK or a standalone toolchain was found"), confirming the preset itself is structurally sound, not broken CMake. Writing a full Gradle/Xcode project wrapper around it is deferred: this sandbox has no NDK/Xcode to build or run it against, and unverifiable native-mobile-project boilerplate is exactly the kind of code the project's own discipline (brief section 96, DECISIONS.md precedent - e.g. bgfx's real GPU backend, SDL relative-mouse-mode) says not to write speculatively. Needs an actual toolchain/CI runner.

## Phase 11 — Optimization + profiling

- [x] Benchmarks (`tools/benchmark`, `VoxelBenchmarks`, opt-in via `LCU_BUILD_TOOLS=ON`) using Google Benchmark (FetchContent-pinned, same vendor/pattern as GoogleTest) against the real engine functions - not synthetic stand-ins: `ChunkStorage::set_block`/`block_at` (voxel access), `worldgen::generate_terrain_chunk` (chunk gen), `mesh_chunk_greedy` on both a fully-solid and a checkerboard chunk (meshing - worst vs. best case for face-merging), `compute_block_light`/`compute_sky_light` (lighting), `raycast`/`move_and_collide` (physics), `save_chunk_to_file`/`load_chunk_from_file` (serialization, zstd compression happens inside these - no separate zstd micro-benchmark), `Connection` send+deliver on `ReliableOrdered` (network), and `update_ai_wander` at 10/100/1000 entities (entity sim). Real measured numbers from a `-DCMAKE_BUILD_TYPE=Release` build in this sandbox (4-core, 2.1GHz) are in `BUILD_STATUS.md`. One real finding from actually running these: the checkerboard-pattern chunk mesh takes ~15x longer than the fully-solid chunk (1.45ms vs. 94us) - greedy meshing's face-merging is doing real, measurable work, not just extra code with no effect.

## Phase 12 — UI + audio + content + polish

- [x] SDL3 audio backend: `engine/audio::AudioEngine` (RAII wrapper around one `SDL_AudioStream` opened via `SDL_OpenAudioDeviceStream`, 44.1kHz stereo float) - only `audio_engine.cpp` includes `<SDL3/SDL_audio.h>`, mirroring `engine/scripting`'s Lua-header confinement. `engine/audio::generate_sine_wave` synthesizes real, own-created PCM tone content (no WAV asset pipeline, and any checked-in asset would need to be this project's own IP anyway - brief section 12) - no placeholder silence.
- [x] Positional audio: `engine/audio::{compute_stereo_pan, distance_attenuation}` - pure math (no SDL dependency, fully unit tested), a real first-pass stereo pan + linear distance falloff, not full HRTF/3D audio (see DECISIONS.md for why that's the right amount of complexity right now). 15 new unit tests across `waveform`/`positional`.
- [x] Wired into `VoxelClient`: breaking/placing a block now plays a real synthesized tone, panned/attenuated by the block's position relative to the camera - verified via a real run (`AudioEngine initialized: 44100 Hz, stereo float` under `SDL_AUDIODRIVER=dummy`, no crash through the break/place round trip). `AudioEngine::init()` failing (no device - most CI/this sandbox without the dummy driver) is non-fatal, logged, and silently skips playback.
- [x] UI system usable from desktop/gamepad/touch: `engine/ui::draw_debug_overlay` - a real on-screen HUD via bgfx's built-in debug-text character buffer (`Renderer::draw_debug_text`/`clear_debug_text`, new methods keeping bgfx access confined to `engine/rendering` per ARCHITECTURE.md), showing live FPS and a legend for every mobile touch-control button. The button labels are drawn at the exact same normalized rects `TouchInputBackend` hit-tests against (`lcu::platform::kTouchButtonLayout`, promoted out of `touch_input.cpp` into a shared header specifically so hit-testing and drawing can never drift apart) - this closes the Phase 10 "a player would currently be dragging/tapping blind" limitation for the touch overlay, and the pre-existing "debug overlay is a log line, not on-screen" limitation, both for real. Verified via a real bgfx (`Noop` backend) run: full startup-to-shutdown with no crash/assert through `draw_debug_overlay` every frame.

## Phase 13 — Block edit replication (post-original-queue; found via Reality Audit)

The original 12-phase queue from the brief is complete (see Phase 0-12
above). Per the master brief's explicit rule ("do not stop because the
roadmap is complete"), a Reality Audit was performed against the actual
code/build/test/runtime state rather than trusting this file's own
prior claims - it confirmed every existing PARTIAL/MISSING/UNVERIFIED
note was accurate, and identified block edit replication (brief section
19, called out as "besonders wichtig") as the highest-value remaining
gap: break/place only ever mutated a connected client's own local
`World`, never reaching the server or any other client. See
PROJECT_STATE.md "Reality Audit" for the full table.

- [x] `BlockAction` (client->server request) / `BlockChange`
  (server->all-clients broadcast) added to `game::systems::protocol`,
  both `ReliableOrdered`. 10 new unit tests.
- [x] `VoxelServer::handle_block_action`: validates a request (chunk
  loaded, break targets non-air/place targets air with a registered
  block_id, target within `kMaxBlockActionRange` of the requester's
  server-known position - brief section 20), applies it to the
  server's own `World`, broadcasts the result to every connected
  client including the requester. Rejections are logged, not replied
  to - the requester's world simply doesn't change.
- [x] `VoxelClient`: a block edit is a server-authoritative request,
  not a local mutation - the client waits for its own `BlockChange`
  broadcast to come back before touching its `World` (see DECISIONS.md
  "block edits are not client-predicted"). Item pickup/consumption
  stays client-local and optimistic, fired at request-send time.
- [x] Late-joiner catch-up: `VoxelServer` keeps every applied edit
  (`block_change_history`) and replays it to a newly connecting client
  right after its `Welcome`, found and fixed in the same pass after the
  first verification run exposed that a late-connecting client never
  learned about earlier edits.
- [x] Verified via a real three-process run (1 server + 2 independent
  clients, one acting, one purely observing): both clients converge to
  the identical world state after a break and a place, confirmed by
  matching `Applied server BlockChange` log lines on both - not just
  that a message decoded. A separate run confirms the late-joiner
  catch-up specifically. Single-player mode re-verified unchanged.
  `ctest` 316/316 (bgfx) / 313/313 (non-bgfx), up from 308/308 / 305/305.
- [x] Two real bugs found and fixed while verifying (not hypothesized -
  actually hit while running the feature): a `continue` that would have
  skipped a frame's render/network-flush/frame-count; networked-mode
  breaking giving no item (making placing impossible in multiplayer).

## Phase 14 — Chunk network streaming (post-original-queue; the other half of the gap Phase 13 didn't touch)

Reality Audit's second confirmed gap (see PROJECT_STATE.md "Reality
Audit"): `engine/network::Connection` had no message fragmentation, so a
compressed chunk (a few KB) could never fit in one 1200-byte UDP
datagram - both `VoxelClient` and `VoxelServer` independently generated
matching terrain from the same hardcoded seed instead of the client
actually *receiving* the server's authoritative world.

- [x] `lcu::network::fragment_payload`/`FragmentReassembler`
  (`engine/network/fragmentation.h`/`.cpp`) - a generic, caller-side
  split/rejoin layer, deliberately kept out of `Connection`/
  `PacketHeader` so existing small messages pay nothing for it. 11 new
  unit tests (in-order, out-of-order, duplicate, interleaved-concurrent,
  malformed-too-short delivery).
- [x] `lcu::serialization::serialize_chunk_to_bytes`/
  `deserialize_chunk_from_bytes` extracted from the existing file-based
  `save_chunk_to_file`/`load_chunk_from_file` (now thin wrappers), so
  network streaming reuses the exact same, already-tested compression/
  versioning/corruption logic. 4 new unit tests, including
  `InMemoryBytesMatchFileBytes` pinning byte-for-byte equivalence with
  the pre-existing file path.
- [x] `ChunkData` (server->client, logical, too large for one datagram)
  / `ChunkDataFragment` (the actual wire message) added to
  `game::systems::protocol`. 6 new unit tests.
- [x] `World::loaded_chunk_coords()` (new accessor) + `VoxelServer`:
  right after `Welcome` and the `block_change_history` replay, sends a
  newly-connecting client every currently-loaded chunk, fragmented and
  `ReliableOrdered`.
- [x] `VoxelClient`: a per-connection `FragmentReassembler` reassembles
  `ChunkDataFragment`s; once a `ChunkData` is complete, its chunk fully
  overwrites the client's own (independently, deterministically
  generated) local chunk, then fully relights and remeshes it plus its
  six neighbors.
- [x] Verified via two real two-process runs: a `mobile_low` (1-chunk
  world) run logs `Sent 1 chunk(s) (1 fragment(s))` / `Applied server
  ChunkData for chunk (0, 1, 0)`; a `desktop` (36-chunk world) run logs
  `Sent 36 chunk(s) (36 fragment(s))` and 36 matching `Applied server
  ChunkData` lines, zero warnings/errors. `ctest` 337/337 (bgfx) /
  334/334 (non-bgfx), up from 316/316 / 313/313.

Honestly scoped: a one-shot full sync on connect only, not
interest-managed by distance and not re-streamed as either side's
loaded-chunk set changes afterward (see NETWORKING.md "Chunk network
streaming").

## Phase 15 — Server-side inventory (post-original-queue; closes Phase 13's remaining honest gap)

Item pickup/placement-cost was entirely client-local and optimistic
with no server-side accounting at all - a `BlockAction` the server
rejected was never refunded, honestly flagged as a known gap in Phase
13's own writeup.

- [x] `VoxelServer` registers the same `game:stone` item `VoxelClient`
  does (identical namespaced id/display name/max stack size, so their
  `ItemId`s coincide by construction) and gives each `ClientState` a
  real 9-slot `lcu::items::Inventory`.
- [x] `handle_block_action`: placing `game:stone` is now rejected
  unless the requester actually holds one server-side (a new validity
  condition); a successful break/place of it adds/removes one from that
  client's server-side inventory.
- [x] `InventoryUpdate` (server->one client, `ReliableOrdered`) added
  to `game::systems::protocol` - sent after every `BlockAction`,
  accepted or rejected, carrying that client's current authoritative
  `game:stone` count. 6 new unit tests.
- [x] `VoxelClient` keeps its existing optimistic pickup/consumption
  (unchanged - still fires at request-send time) but now reconciles it
  against every `InventoryUpdate`, the same pattern `PlayerCorrection`
  already uses for movement.
- [x] Verified via a real two-process run (`LCU_VERIFY_BREAK_PLACE`):
  the client's log shows the optimistic guess and the server's
  authoritative count actually disagree then converge in both
  directions (`Reconciled inventory item 1 to authoritative count 1
  (was 0)` right after the break, `... count 0 (was 1)` right after the
  place), each immediately followed by the matching `Applied server
  BlockChange` - not just that a message decoded. `ctest` 342/342
  (bgfx) / 339/339 (non-bgfx), up from 337/337 / 334/334.

Honestly scoped: only `game:stone` is inventory-gated - no general
block-id-to-item-id mapping exists, so any other registered block (mod
content) still places without a server-side item check. No persistence
across a disconnect either.

## Phase 16 — Per-movement chunk streaming (post-original-queue; closes Phase 14's remaining honest gap)

The connect-time `ChunkData` sync (Phase 14) ran exactly once - a
player wandering past their initial spawn area streamed nothing new,
honestly flagged as a known gap in Phase 14's own writeup.

- [x] `VoxelServer` re-checks every connected client's loaded-chunk
  range every tick (only when that client's current chunk coordinate
  has changed since last checked, so a stationary client costs nothing
  extra), loads any not-yet-loaded chunk in range with the same logic
  the startup area already uses, and broadcasts every newly-loaded
  chunk as `ChunkData` to every connected client - not just whoever's
  movement triggered it.
- [x] Deliberately append-only: the server's shared `World` never
  unloads a chunk (see DECISIONS.md "server-side chunk streaming never
  unloads") - unloading based on one client's position could break a
  different client still standing in that chunk, since `World` is one
  instance shared across every connection.
- [x] `VoxelClient` runs the mirror-image local half unconditionally
  (single-player and networked alike): the same load-then-light-then-
  mesh sequence the initial spawn-area load already runs, triggered
  only when the player's own chunk coordinate changes.
- [x] Fixed a real gap in Phase 14's `ChunkDataFragment` handler that
  this phase's dynamics exercise for the first time: a `ChunkData` for
  a coordinate the client hasn't locally streamed to yet used to be
  silently dropped - now the client creates a real chunk slot via
  `world.load_chunk` before overwriting it.
- [x] New headless verification hook, `LCU_VERIFY_MOVE_SECONDS` - holds
  `MoveForward` for that many real (wall-clock) seconds, since a
  frame-count-indexed hook doesn't work against the unthrottled main
  loop.
- [x] Verified via two real multi-process runs: a two-process run where
  a client held `MoveForward` for 6 real seconds (crossing the 16-block
  chunk boundary) shows the server logging `Streamed 1 newly-loaded
  chunk(s) into range (total 2 loaded)` and the client logging `Applied
  server ChunkData for chunk (0, 1, -1)`; a three-process run adds a
  second, entirely stationary client that independently logs the
  identical line, proving the broadcast reaches every connected client,
  not just the one that triggered it.

No new unit tests - orchestration logic in the two executables built on
already-unit-tested primitives, verified via the real runs above.
`ctest` unchanged at 342/342 (bgfx) / 339/339 (non-bgfx).

Honestly scoped: still no interest-managed unloading; a client's own
local streaming trigger and the server's are independent and only
usually agree, not literally synchronized (never incorrect, just
occasionally redundant).

Next per brief section 10's priority order (multiplayer fundamentals
before content/polish) - see PROJECT_STATE.md "Next Task" for the full
reasoning: extending server-side inventory past `game:stone`, then
interest-scoped chunk unloading to close Phase 16's own remaining gap.

## Phase 17 — Surface/subsurface terrain content (post-original-queue; closes a long-flagged content gap)

Worldgen only ever placed one block type below the terrain height - no
`game:grass`/`game:dirt` registered anywhere real, so there was nothing
else to place. Honestly flagged in Known Limitations since Phase 3.

- [x] `lcu::world::worldgen::generate_terrain_chunk`'s signature changed
  from a single `solid_block` to `(surface_block, subsurface_block,
  stone_block)`: the topmost solid layer is `surface_block`, the next
  `kSubsurfaceDepth` (3) layers are `subsurface_block`, everything
  deeper is `stone_block`.
- [x] `VoxelClient`/`VoxelServer` both register `game:grass`/`game:dirt`
  block definitions - identical fields, identical registration order
  right after `game:stone` on both sides, so their `BlockId`s coincide
  by construction (the same simplification already carried for item
  ids, see DECISIONS.md) - and pass them into `generate_terrain_chunk`.
- [x] Both new blocks are fully real content: real collision/meshing
  (entirely data-driven off `BlockRegistry`, never hardcoded by block
  id - no changes needed anywhere in physics/meshing/lighting), real
  network replication (a `ChunkData` snapshot just contains whatever
  block ids the chunk actually holds), real break/place through the
  existing generic edit paths.
- [x] Updated 4 existing unit tests and added 2 new ones
  (`SurfaceLayerIsExactlyOneBlockThickAtTheHeight`, and renamed
  `ChunkFarBelowTerrainIsEntirelySolid` to `...IsEntirelyStone`) for the
  new layering behavior.
- [x] Verified via a real single-player run (36-chunk world generates,
  no crash) and a real two-process networked run (`Sent 1 chunk(s) (1
  fragment(s))` / `Applied server ChunkData` for a chunk now containing
  the layered content, zero warnings/errors) - confirming the new
  content flows through the *existing* pipeline unmodified. `ctest`
  343/343 (bgfx) / 340/340 (non-bgfx), up from 342/342 / 339/339.

Honestly scoped: only `game:stone` has an item mapping (Phase 5) - the
break->item drop for grass/dirt doesn't exist yet, so breaking either
currently grants no item. No climate/biome/caves/ores/structures/
vegetation (brief section 21's later pipeline stages) - every column
still uses the same three block ids regardless of position.

Next per brief section 10's priority order - see PROJECT_STATE.md "Next
Task" for the full reasoning: item mappings for grass/dirt, then
extending server-side inventory past `game:stone`, then interest-scoped
chunk unloading.

## Phase 18 — Item mappings for grass/dirt (post-original-queue; closes Phase 17's immediate follow-up gap)

`game:grass`/`game:dirt` were fully real terrain content (Phase 17) but
breaking either granted no item - Phase 5's break->item logic was one
hardcoded `if (block == stone_id)` check.

- [x] `VoxelClient` registers `game:grass`/`game:dirt` items (1:1
  mapping to their block counterparts, matching `game:stone`'s own
  convention). A new `grant_item_for_broken_block` helper replaces the
  two previously-duplicated stone-only blocks (networked and
  single-player break paths) with one lookup covering all three blocks.
- [x] `VoxelServer` registers the same two items, same order, purely to
  keep both sides' `ItemId` spaces aligned - doesn't track either in a
  per-client `Inventory` yet.
- [x] Verified via a real single-player run (`LCU_VERIFY_BREAK_PLACE`):
  the player spawns standing on a grass surface block (Phase 17's
  layering means the straight-down raycast now hits grass, not stone) -
  log shows `Breaking block at world (0, 28, -1)` then `Picked up 1
  game:grass (inventory: 1)`, an unforced real exercise of the new path.
- [x] Verified via a real two-process networked run: server logs
  `Applied BlockAction from <addr>: (0,28,-1) 2 -> 0` (block id 2 =
  `game:grass`), client logs `Requesting break`, `Picked up 1
  game:grass (inventory: 1)`, then `Applied server BlockChange ...
  block_id=0` - confirming the mapping works under server-authoritative
  editing too, not just single-player.

No new unit tests - pure orchestration logic reusing already-tested
`ItemRegistry`/`Inventory` primitives. `ctest` unchanged at 343/343
(bgfx) / 340/340 (non-bgfx).

Honestly scoped: placing grass/dirt isn't wired up (no hotbar/item-
selection UI - `PlaceBlock` always places `game:stone`), and
server-side authoritative tracking (Phase 15's `InventoryUpdate`) still
only covers `game:stone` - grass/dirt pickup is client-authoritative
and optimistic, same as `game:stone` was before Phase 15.

Next per brief section 10's priority order - see PROJECT_STATE.md "Next
Task" for the full reasoning: extending server-side inventory past
`game:stone`, then interest-scoped chunk unloading, then placing
grass/dirt.

## Phase 19 — Server-side inventory extended past game:stone (post-original-queue; closes Phase 15's remaining honest gap)

Phase 15's server-side inventory only ever tracked `game:stone` -
Phase 18's new grass/dirt pickup was entirely client-optimistic with
nothing server-side to correct it.

- [x] `VoxelServer` registers `game:grass`/`game:dirt` items (capturing
  their `ItemId`s, previously discarded in Phase 18).
- [x] New `item_for_block` lookup (the same direct 1:1 mapping
  `VoxelClient`'s `grant_item_for_broken_block` already used) replaces
  the single hardcoded `stone_id` check in `handle_block_action`'s
  break/place bookkeeping and place-validity gate - covers all three
  tracked items identically now, not a stone-only special case.
- [x] `send_inventory_update` became `send_inventory_updates`: sends
  one `InventoryUpdate` per tracked item after every `BlockAction`, not
  just whichever the request happened to touch.
- [x] `VoxelClient` needed no changes - its `InventoryUpdate` handler
  was already generic (keyed by whatever `item_id` arrives).
- [x] Verified via a real two-process run (`LCU_VERIFY_BREAK_PLACE`):
  the player spawns on a grass block (Phase 17), and the round trip
  converges cleanly - server logs `Applied BlockAction ...: (0,28,-1) 2
  -> 0`, client logs `Requesting break`, `Picked up 1 game:grass
  (inventory: 1)`, `Applied server BlockChange ... block_id=0`, zero
  warnings/errors.

No new unit tests - pure generalization of already-tested
`ItemRegistry`/`Inventory` orchestration. `ctest` unchanged at 343/343
(bgfx) / 340/340 (non-bgfx).

Honestly scoped: still only stone/grass/dirt are inventory-backed (no
general, data-driven block-id-to-item-id mapping); no persistence
across a disconnect/reconnect; placing still only ever places
`game:stone` (no hotbar/item-selection UI).

Next per brief section 10's priority order - see PROJECT_STATE.md "Next
Task" for the full reasoning: interest-scoped chunk unloading, then
placing grass/dirt, then a general data-driven block-item mapping.

---

## Phase 20 — Interest-scoped chunk unloading, real chunk persistence, disconnect detection (post-original-queue; closes Phase 16's remaining honest gap)

Phase 16 made the server stream newly-in-range chunks per movement, but
`World` could still only ever grow - nothing ever unloaded, and UDP's
lack of a connection concept meant a departed client's `ClientState`
(and its share of the load) lived forever too.

- [x] Real disconnect detection: every `ClientState` tracks
  `last_packet_time` (updated on every received packet, not just on
  connect); a per-tick sweep erases any client idle past
  `kClientTimeoutSeconds` (5.0f, deliberately untuned/aggressive - a
  real connected client's next `PlayerInput` is always fresh since the
  client loop is unthrottled).
- [x] Interest-scoped unloading: each `ClientState` now tracks a real
  `interest_set` (every chunk coord within load radius of its last
  streamed center, via a new `compute_interest_set` lambda); after any
  tick where a client moved, connected, or was pruned, the server
  unions every remaining client's interest set and unloads any
  currently-loaded chunk nobody needs.
- [x] Real chunk persistence wired to the real trigger it never had:
  before unloading, the chunk is saved via the already-existing,
  already-tested `lcu::serialization::save_chunk_to_file`; if a
  client's interest later returns to that coord, `load_chunk_from_file`
  is tried before falling back to regeneration - closes a pre-existing
  Known Limitation as a necessary side effect, not scope creep.
- [x] Fixed a bug caught before ever building: pre-setting a freshly-
  connected client's `last_streamed_center` to its own spawn center
  made the movement loop treat "just connected" as "no change, skip",
  which would silently skip the real load-or-reload path for a client's
  own spawn-adjacent chunks if a previous client's departure had
  evicted them. Fixed by making the field `std::optional<ChunkCoord>`
  (default unset) instead of pre-populating it.
- [x] Verified via real multi-process runs: a disconnect-timeout run
  (`"Client 127.0.0.1:42566 timed out after 5.0s of silence,
  disconnecting"` at ~5s, not a mock); an isolated unload run (`"Saved
  chunk (x,y,1) to disk before unloading"` x12, `"Unloaded 12 chunk(s)
  no connected client still needs (total 36 loaded)"`); a chained run
  where a second, freshly-connecting client receives `ChunkData` for
  the exact same 12 coordinates the first run evicted, proving reload-
  from-disk (not silent data loss) - see NETWORKING.md for the full
  writeup including one honestly-documented, unconfirmed finding
  (a suspected `UnreliableSequenced` sequence-wraparound under extreme
  sustained packet volume, discovered while stress-testing but out of
  this phase's scope to fix).

No new unit tests - this phase composes already-tested primitives
(`World`, `lcu::serialization::{save,load}_chunk_to_file`) under new
server-side orchestration exercised by the real runs above. `ctest`
unchanged at 343/343 (bgfx) / 340/340 (non-bgfx).

Honestly scoped: persistence is session-scoped only (saved chunks live
in `<world>/chunks/`, so a fresh server process on the same world
directory would actually pick them up, but full cross-restart
persistence as a *product feature* - e.g. surviving a deliberate
restart in a real deployment - hasn't been separately verified);
`kClientTimeoutSeconds` is a placeholder value, not tuned against any
real-world latency/jitter data; the suspected sequence-wraparound
networking issue above remains unconfirmed and unfixed.

Next per brief section 10's priority order - see PROJECT_STATE.md "Next
Task": placing grass/dirt (no hotbar/item-selection UI exists, so
`PlaceBlock` still always places `game:stone`), then a general
data-driven block-id-to-item-id mapping.

---

## Phase 21 — Hotbar item selection for placing grass/dirt (post-original-queue; closes Phase 18/19's remaining honest gap)

Phase 18/19 gave `game:grass`/`game:dirt` real item mappings on both
break and (server-side) place validation, but `PlaceBlock` itself still
only ever requested `game:stone` - there was no way for a player to
choose otherwise, since no hotbar/item-selection UI existed yet.

- [x] New `Action::CycleHotbar` (`engine/platform::Action`), bound to
  `R` on keyboard and a new "ITEM" touch button, following the exact
  pattern every other action already uses (`engine/platform/src/
  input.cpp`, `touch_control_layout.h`).
- [x] `VoxelClient` gained a `placeable_items` list (stone/grass/dirt,
  same order as every other block/item list in the file) and a plain
  `selected_placeable_index`, cycled on an edge-detected `CycleHotbar`
  press - a real, minimal selection mechanism, not a graphical hotbar
  (no on-screen slot rendering exists yet - needs `engine/ui`'s
  texture-atlas work first, same gap noted for the debug overlay).
- [x] `PlaceBlock`'s handling (both single-player and networked
  branches) now reads `placeable_items[selected_placeable_index]`
  instead of the hardcoded `stone_id`/`stone_item_id` - no protocol
  change needed, since `BlockAction::block_id` was already a plain
  field and the server's `item_for_block`/place-validity gate already
  generalized to any item-backed block back in Phase 19.
- [x] Extended the existing `LCU_VERIFY_BREAK_PLACE` headless hook with
  a `kVerifyCycleHotbarFrame` (between break and place) so the same
  hook now exercises break -> cycle -> place end to end, proving
  placing isn't hardcoded anymore.
- [x] Verified via a real single-player run: `"Selected placeable item:
  game:grass"` then `"Placing game:grass at world (0, 28, -1)
  (inventory: 0)"` - the exact position the grass block was broken
  from. Verified via a real two-process networked run: server logs
  `"Applied BlockAction from <addr>: (0,29,-1) 0 -> 2"` (block id 2 =
  `game:grass`, not the old hardcoded stone id 1), client logs
  `"Requesting place game:grass..."` then `"Applied server BlockChange
  at world (0, 29, -1): block_id=2"` - both sides converge on grass,
  not stone, confirming the server-authoritative path (Phase 19's
  generalized validity gate) works for a client-selected block for the
  first time in a real run.

No new unit tests - `touch_input_test.cpp`'s `Action::Count`-driven
loop and every other `Action`-keyed test already generalize to the new
enumerator automatically. `ctest` unchanged at 343/343 (bgfx) / 340/340
(non-bgfx).

Honestly scoped: still no graphical hotbar (no on-screen slot
rendering/selection highlight - text log only, matching the debug
overlay's own current text-only state); selection is a plain index
cycle through a fixed 3-item list, not a real inventory-driven hotbar
that only shows items the player actually holds; server-side inventory
persistence across reconnect still doesn't exist (Phase 15/19's
existing gap, unrelated to this phase).

Next per brief section 10's priority order - see PROJECT_STATE.md "Next
Task": a general, data-driven block-id-to-item-id mapping (`item_for_block`/
`grant_item_for_broken_block` are still three explicit `if` checks on
both client and server), then further content/gameplay systems.

---

## Phase 22 — Data-driven block-id-to-item-id mapping (post-original-queue; closes Phase 19's remaining honest gap)

Phase 19 generalized the server's inventory bookkeeping to cover
grass/dirt alongside stone, but the lookup underneath it -
`item_for_block` on the server, `grant_item_for_broken_block` on the
client - was still three explicit `if (block_id == X)` checks, one per
block, hand-duplicated between the two files and needing a matching
edit in both places for every new item-backed block added.

- [x] New `game::items::BlockItemMapping` (`game/items/`, a new
  gameplay-layer module alongside `game/components`/`game/systems`,
  matching the GAME -> ENGINE layering `ARCHITECTURE.md` already
  documents) - a small `register_pair(block_id, item_id)`/
  `item_for_block(block_id)` table, `kNoItemId` for anything
  unregistered. No engine-level change needed: `Lcu::EngineCore`
  already transitively provides both `lcu::voxel::BlockId` and
  `lcu::items::ItemId` to `game/`.
- [x] `VoxelClient`'s `grant_item_for_broken_block` and `VoxelServer`'s
  `item_for_block` both now populate the same table shape (three
  `register_pair` calls right after each block/item pair is
  registered) and do a single lookup instead of their own hardcoded
  chain - the two are still independently populated (client and server
  are separate processes with separate `ItemRegistry` instances, same
  as every other piece of content each side registers), but the *logic
  shape* is now identical and adding a fourth pair is one call in each,
  not a new `if` branch in each.
- [x] 4 new unit tests (`BlockItemMapping.*`): unmapped block returns
  `kNoItemId`, a registered pair round-trips, re-registering a block id
  overwrites its previous mapping, and multiple blocks can map to the
  same item (proving the table isn't accidentally 1:1-only, even though
  today's actual content happens to be).
- [x] Verified via a real single-player run and a real two-process
  networked run - both reproduce the exact same log lines Phase 21's
  own verification produced (`"Picked up 1 game:grass..."`,
  `"Applied server BlockChange at world (0, 29, -1): block_id=2"`),
  confirming the refactor changed *how* the lookup works, not *what* it
  returns - a true refactor, not a behavior change.

`ctest` now 344/344 (non-bgfx, up from 340) / 347/347 (bgfx, up from
343) - the 4 new `BlockItemMapping` tests.

Honestly scoped: client and server still each maintain their own
separate table populated from their own separate content registration
(no cross-process sync of the mapping itself - same "must agree by
construction" simplification every other piece of shared content in
this project already carries, see NETWORKING.md); still not loaded
from an external data file - "data-driven" here means a real runtime
table populated by code, matching how `BlockRegistry`/`ItemRegistry`
themselves are "datadriven" (in-code registration, not external
config), not a JSON/config-file content pipeline (a separate, larger
piece of future work if modding ever needs it).

Next per brief section 10's priority order - see PROJECT_STATE.md "Next
Task": further content/gameplay systems (a real crafting-UI caller for
`RecipeRegistry`, more block/item variety), then modding depth, then
platform verification, then performance work, then UI/audio polish.

---

Phase 1 is functionally complete for what a headless sandbox can verify:
window, event loop, bgfx rendering bootstrap, action-based input, minimal
debug overlay. Mouse-look (camera control) is intentionally not built yet
— there is no camera/player entity until Phase 2-4 give it something to
control, so a mouse-delta API would have no consumer yet (brief section
98, no overengineering ahead of need).

Phase 2 chunk storage/coordinates/BlockRegistry/job system/greedy
meshing/GPU upload are all done and verified end-to-end: `VoxelClient`
registers a placeholder "game:stone" block, builds a flat ground slab
`Chunk`, dispatches `mesh_chunk_greedy` through `engine/jobs::JobSystem`,
and uploads the result into real bgfx vertex/index buffers (verified
headlessly via the `Noop` backend - confirmed `valid=true index_count=36`
from a real run, not a mock).

A real draw call is now working: `LCU_BUILD_SHADER_TOOLS=ON` builds
bgfx's `shaderc` (confirmed buildable: glslang/SPIRV-Tools/SPIRV-Cross/
Dawn-Tint, ~700 extra build steps, see DECISIONS.md), compiles
`client/shaders/{vs_chunk,fs_chunk}.sc` into spirv/glsl/essl binaries via
`bgfx_compile_shaders()`, and `VoxelClient` loads them into a real
`bgfx::ProgramHandle` and calls `Renderer::submit_chunk_mesh()` ->
`bgfx::submit()` every frame. Verified via a real run: `"Chunk shader
program valid=true"` followed by 3 clean frames with the draw call
actually submitted, under bgfx's `Noop` backend (no GPU/display here).

Phase 2 and Phase 3 are both functionally complete for what this
sandbox can verify: chunk storage, meshing, GPU upload, real draw
calls, multi-chunk `World` with streaming, deterministic terrain
generation, and versioned/corruption-checked save/load are all done
and tested (see the phase sections above for specifics).

Phase 4 is now functionally complete for what this sandbox can verify:
voxel DDA raycasting, AABB collision/player physics (gravity, jump,
auto-step, a dedicated ground probe fixing a real grounding-detection
bug found before it ever shipped), and a first-person camera are all
implemented and unit tested. `VoxelClient` was rewritten from its
Phase 2 single-placeholder-chunk approach to load a real multi-chunk
`World` (36 chunks around spawn), spawn a physics-driven player resting
on the generated terrain surface, drive the camera from arrow-key look
input and WASD movement, raycast every frame for block selection, and
mutate the world on edge-detected Interact (break)/PlaceBlock (place)
presses - remeshing and re-uploading the affected chunk (plus any
neighbor chunk sharing the mutated block's boundary, so cross-chunk
face culling stays correct). Verified via a real headless run with a
synthetic input hook (`LCU_VERIFY_BREAK_PLACE`, see DECISIONS.md): a
block is broken, logged, then the next frame's raycast (now reaching
one block deeper) is used to place a new block back at the exact same
world position - a real round-trip through the mutate/remesh/reupload
pipeline, not a mock of it. This closes brief section 80's slice 1
vertical slice (save/load already existed from Phase 3, just not yet
wired to any trigger in `VoxelClient` - see Known Limitations).

Phase 5 is now functionally complete for what this sandbox can verify:
`engine/items::{ItemRegistry, Inventory, RecipeRegistry}` are all
implemented and unit tested, and `VoxelClient`'s break/place loop now
runs on a real item economy - breaking a block adds a `game:stone` item
to a 9-slot player `Inventory`, and placing one consumes it back out
(refunded if the placement target turns out to be unloaded). Verified
via the same `LCU_VERIFY_BREAK_PLACE` headless hook used for Phase 4:
"Picked up 1 game:stone (inventory: 1)" then "Placing block ...
(inventory: 0)".

Phase 6 is now functionally complete for what this sandbox can verify:
`engine/ecs::Registry`, `engine/lighting::{LightStorage, propagation}`,
`game::systems::{update_ai_wander, DayNightCycle}` are all implemented
and unit tested, and `VoxelClient` now loads real per-chunk lighting,
keeps it correct incrementally through every block edit, and runs 3
wandering AI entities plus a ticking day/night cycle every frame.
Verified via a real headless run: "Sky light 5 blocks above spawn
column: 15", AI entities logging real (deterministic, seeded) positions,
and "Day/night: time_of_day=0.000 sky_light_scale=0.550" alongside the
still-passing `LCU_VERIFY_BREAK_PLACE` round-trip.

Phase 7 is now functionally complete for what this sandbox can verify:
`engine/network` implements all four committed channel semantics over a
hand-rolled ack/retransmit UDP protocol (see `NETWORKING.md` for the
wire format), and `VoxelServer` runs a real `World` + AI simulation and
a real UDP handshake instead of the Phase 0 sleep-only placeholder.
Verified via 46 new unit/integration tests (including real loopback
sockets with deliberately simulated packet loss) and a real two-process
run: a standalone Python UDP client received a genuine Welcome message
and live Heartbeats from a running `VoxelServer`.

Phase 8 is now functionally complete for what this sandbox can verify:
`engine/replication::{PredictionBuffer, PositionInterpolator}` are
implemented, tested, and wired into a real `VoxelClient`<->`VoxelServer`
connection (`LCU_CONNECT_PORT`) - client-side prediction with
server-reconciliation for the local player, server-driven AI rendered
through client-side interpolation instead of local simulation, and
per-client interest-managed entity broadcasts. Verified via a real
two-process run: a `VoxelClient` connected to a running `VoxelServer`
over loopback UDP, received a genuine Welcome, rendered all 3 remote AI
entities via real interpolation, and had its player position
predicted-then-reconciled against the server's authoritative correction.
Chunk network streaming/compression is explicitly deferred (needs
message fragmentation `engine/network::Connection` doesn't have yet -
see NETWORKING.md), and block edits still aren't replicated at all - see
NETWORKING.md "What's deferred" for the complete list.

Phase 9 is now functionally complete for what this sandbox can verify:
`engine/scripting::LuaState` (sandboxed Lua 5.4.7 VM),
`engine/modding::{EventBus, bind_block_registry, bind_item_registry,
ModLoader}`, and a real working `example_mod` are all implemented and
tested. Both `VoxelClient` and `VoxelServer` load `mods/` at startup
into their own `LuaState` + registries (the server also gets an
`EventBus`/`ItemRegistry` purely so a mod script shared between both
hosts has a uniform Lua API and doesn't fail to load on whichever host
doesn't yet consume one of its calls). Verified via real runs (not just
unit tests): the server logs its mod's registrations and `Loaded 1
mod(s)`; the client, under `LCU_VERIFY_BREAK_PLACE`, additionally logs
`[example_mod] block_broken #1: block id 1 broken at (0, 28, -1)` at the
exact moment a real block is broken - the full register -> load ->
subscribe -> emit loop exercised end to end. 27 new unit tests. `ctest`
274/274 (bgfx build) / 271/271 (non-bgfx build).

Phase 10 is now functionally complete for what this sandbox can verify:
`engine/platform::TouchInputBackend` and `lcu::core::QualityProfile`
are implemented and tested, both real, usable, tested code (not
placeholders) despite the sandbox having no real touchscreen or mobile
device to exercise them end to end. `CMakePresets.json`'s Android/iOS
presets were re-verified structurally sound. Real Android Gradle/iOS
Xcode project generation remains BLOCKED here - needs an actual
toolchain, not attempted speculatively (see DECISIONS.md).

Phase 11 is now functionally complete for what this sandbox can verify:
`VoxelBenchmarks` (Google Benchmark) exercises every system this
phase's task list named, against real engine code, and was actually run
in both a `Development` build (flagged by Google Benchmark itself as
unoptimized - a real, useful observation about this project's own
default build type) and a `Release` build (real optimized numbers, see
`BUILD_STATUS.md`). No code changed as a result yet - this phase is
about having real measurements to point at, not guessing at
optimizations nothing has shown are needed (brief section 98 "no
overengineering ahead of need" applies to premature optimization too).

Phase 12 is now functionally complete for what this sandbox can verify:
`engine/audio::{AudioEngine, generate_sine_wave, compute_stereo_pan,
distance_attenuation}` and `engine/ui::draw_debug_overlay` are all
implemented and tested, and wired into a real `VoxelClient` - breaking/
placing a block plays a real positionally-panned synthesized tone, and
every frame draws a real on-screen FPS counter plus touch-control
legend via bgfx's debug-text buffer. Verified via real runs (not just
unit tests): `AudioEngine initialized: 44100 Hz, stereo float` under
`SDL_AUDIODRIVER=dummy`, and a full bgfx (`Noop` backend) startup-to-
shutdown run with `draw_debug_overlay` executing every frame with no
crash/assert. 15 new unit tests (`GenerateSineWave`, `ComputeStereoPan`,
`DistanceAttenuation`). This closed out the entire 12-phase queue this
session started with - but per the master brief, that's a checklist
milestone, not a stopping point (brief section 43: never stop because
the roadmap is complete).

Phase 13 (block edit replication, post-queue) is now functionally
complete: a Reality Audit against the actual code (not this file's own
prior claims) confirmed block edits were the highest-value remaining
gap given the brief's explicit multiplayer emphasis - closed via
`BlockAction`/`BlockChange`, server-authoritative validation, broadcast
to every connected client, and late-joiner catch-up via a replayed
edit history. Verified via a real three-process run proving two
independent clients' worlds actually converge, not just that messages
decode.

Phase 14 (chunk network streaming, post-queue) is also now functionally
complete: the audit's other confirmed gap - `engine/network::Connection`
had no message fragmentation, so a compressed chunk couldn't fit in one
UDP datagram - closed via a generic `fragment_payload`/
`FragmentReassembler` layer plus `ChunkData`/`ChunkDataFragment`
messages; `VoxelServer` now sends every newly-connecting client a full,
fragmented snapshot of its currently-loaded world, and `VoxelClient`
reassembles, applies, relights, and remeshes it. Verified via two real
two-process runs (1-chunk and 36-chunk scale).

Phase 15 (server-side inventory, post-queue) closes Phase 13's
remaining honest gap: `VoxelServer` now keeps a real, authoritative
per-client `Inventory`, gates placing `game:stone` on actually holding
one server-side, and corrects a client's optimistic local guess via a
new `InventoryUpdate` message after every `BlockAction` - accepted or
rejected. Verified via a real two-process run showing the optimistic
guess and the server's authoritative count actually disagree then
converge in both directions, not just that a message decoded.

Phase 16 (per-movement chunk streaming, post-queue) closes Phase 14's
remaining honest gap: `VoxelServer` re-checks every connected client's
loaded-chunk range every tick and broadcasts every newly-loaded chunk
to every connected client; `VoxelClient` mirrors the same trigger
locally. Verified via a real two-process run (a client moves for 6 real
seconds, crossing a chunk boundary - server streams and broadcasts the
new chunk, client applies it) and a real three-process run (adding a
second, stationary client that independently receives the same
broadcast, proving it isn't limited to the triggering client).

Phase 17 (surface/subsurface terrain content, post-queue) closes a
long-flagged content gap: `generate_terrain_chunk` now places a real
`game:grass` surface layer, `game:dirt` beneath it, and `game:stone`
deeper - both new blocks registered identically (and in the same
order) on `VoxelClient` and `VoxelServer`, flowing through the
*existing* collision/meshing/replication pipeline unmodified (nothing
in those systems hardcodes block ids). Verified via a real
single-player run and a real two-process networked run streaming a
chunk that actually contains the new layered content.

Phase 18 (item mappings for grass/dirt, post-queue) closes Phase 17's
immediate follow-up gap: breaking `game:grass`/`game:dirt` now grants a
real like-named item, via a `grant_item_for_broken_block` helper
replacing two duplicated stone-only checks. Verified via a real
single-player run (the player spawns on a grass block, so the existing
`LCU_VERIFY_BREAK_PLACE` hook naturally exercises the new path
unforced) and a real two-process networked run confirming the mapping
works under server-authoritative editing too.

Phase 19 (server-side inventory extended past `game:stone`, post-queue)
closes Phase 15's remaining honest gap: a new `item_for_block` lookup
replaces the single hardcoded `stone_id` check in
`handle_block_action`'s bookkeeping and place-validity gate, and
`send_inventory_updates` now sends one `InventoryUpdate` per tracked
item instead of just `game:stone`'s. `VoxelClient` needed no changes -
its handler was already item-id-generic. Verified via a real
two-process run showing the grass break/pickup/BlockChange round trip
converge cleanly under server-authoritative editing. See
PROJECT_STATE.md "Reality Audit" and "Next Task" for the full picture
and what's next (interest-scoped chunk unloading, then placing
grass/dirt, then a general data-driven block-item mapping).

Phase 20 (interest-scoped chunk unloading, real chunk persistence,
disconnect detection, post-queue) closes Phase 16's remaining honest
gap: the server now tracks real disconnects (a per-client idle
timeout, since UDP has no connection concept), a real per-client
interest set (unloading, with a save-to-disk first, any chunk no
connected client still needs), and real reload-from-disk instead of
silently regenerating (and thereby reverting) a previously-edited
chunk. Verified via real multi-process runs: a clean 5-second
disconnect timeout, a 12-chunk unload-and-save, and a second client's
connect proving those exact 12 chunks reload from disk rather than
being lost. See NETWORKING.md for the full writeup, including one
honestly-documented, unconfirmed finding (a suspected UDP sequence-
wraparound issue under extreme sustained packet volume) left for a
future phase.

Phase 21 (hotbar item selection for placing grass/dirt, post-queue)
closes Phase 18/19's remaining honest gap: a new `Action::CycleHotbar`
(bound to `R`/a new touch button) lets a player cycle which of
stone/grass/dirt `PlaceBlock` places next, replacing the hardcoded
`game:stone`-only request. No protocol change was needed - the
server's Phase 19 validity gate already generalized to any item-backed
block. Verified via a real single-player run (break grass, cycle to
grass, place grass at the exact same position) and a real two-process
networked run (server logs `(0,29,-1) 0 -> 2`, confirming a
client-selected non-stone block converges server-authoritatively for
the first time). Still no graphical hotbar - text log only, same
current state as the debug overlay.

Phase 22 (data-driven block-id-to-item-id mapping, post-queue) closes
Phase 19's remaining honest gap: a new `game::items::BlockItemMapping`
(`game/items/`) replaces the hardcoded `if (block_id == X)` chains
`VoxelClient` and `VoxelServer` each carried since Phase 17-19 with a
real `register_pair`/`item_for_block` table - adding a new item-backed
block is now one call per side, not a new branch in two files kept in
sync by hand. 4 new unit tests. Verified via real single-player and
two-process networked runs reproducing Phase 21's exact same log
lines, confirming the refactor changed how the lookup works, not what
it returns. `ctest` 344/344 (non-bgfx) / 347/347 (bgfx).

Known simplifications carried forward, still accurate and still
acceptable until something needs more: `RecipeRegistry` has no
crafting-UI caller; item drops are now looked up through a real
`game::items::BlockItemMapping` table (Phase 22) rather than hardcoded
`if` chains, but that table is still populated from three explicit
`register_pair` calls per process (client and server each maintain
their own, matching content by construction, not synced), not loaded
from an external data file, and still isn't a loot-table system;
placing a block selects among stone/grass/dirt via a plain cycled index
(Phase 21), not a graphical hotbar UI (no on-screen slot rendering/
selection highlight yet - needs `engine/ui`'s texture-atlas work);
worldgen still has no climate/biome/caves/ores/structures/
vegetation (brief section 21's later pipeline stages - every column
uses the same three block ids regardless of position); lighting is
single-chunk scoped (no cross-chunk
bleed); chunk persistence (Phase 20) is scoped to the current server
process's own session directory, not verified as full cross-restart
persistence; the client's own `World` still never unloads (only the
server does, as of Phase 20) since the client only ever has one
player's-worth of interest to track anyway; `engine/network`'s
reliable channel has no RTT estimation/congestion control, and its
server connection model has no authentication; server-side inventory
has no persistence across a disconnect/reconnect; `EventBus` has only one real event (`block_broken`);
`ModLoader` has no manifest/dependency/version format; mod-registered
ids aren't synced over the network (both hosts must load the same mods
independently and agree by construction); positional audio is pan +
linear falloff, not full HRTF/3D audio; the debug overlay is VGA-style
character text, not a real font/texture-atlas UI (no atlas exists yet -
brief section 12's content pipeline) - see
NETWORKING.md/DECISIONS.md/PROJECT_STATE.md for each.

**Not done, and out of scope for this sandbox regardless of what's
built next**: confirming what any of this actually looks like on a real
GPU/display, since none exists here. Every claim about rendering,
camera behavior, etc. is about the logic/API being correct, not about
visual appearance.

Also outstanding from Phase 1, lower priority, revisit opportunistically:
confirm the bgfx build on a machine/CI runner with a real display and
GPU (Vulkan or GL) — this sandbox can only verify the headless Noop
path.
