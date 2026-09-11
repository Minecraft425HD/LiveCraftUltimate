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

## Phase 23 — Quick-craft: RecipeRegistry's first real caller (post-original-queue; closes a Phase 5 gap long-flagged as content)

`RecipeRegistry` (Phase 5) was implemented and unit tested but had zero
real callers anywhere - "no crafting-grid caller exists yet" had been
an honest, standing gap since Phase 5 itself, restated unchanged
through every later phase's Known Limitations.

- [x] First crafted-only content: `game:compost` - a new item with no
  corresponding block, obtainable only by crafting (not by breaking
  anything), registered client-side alongside stone/grass/dirt/etc.
- [x] One real shapeless recipe: `1x game:grass + 1x game:dirt -> 1x
  game:compost`, registered on a new `lcu::items::RecipeRegistry`
  instance in `VoxelClient`.
- [x] New `Action::Craft` (`engine/platform::Action`), bound to `C` on
  keyboard and a new "CRAFT" touch button, following the same pattern
  every other action already uses.
- [x] Quick-craft trigger: on an edge-detected Craft press, builds a
  query grid from one of each *distinct* item type currently held
  (dedup by inventory-slot scan), calls `RecipeRegistry::find_match`
  for real, and on a match consumes exactly the grid's contents (which
  equals the matched recipe's ingredients exactly, since shapeless
  matching requires an exact multiset match) and grants the result.
  Logs "No recipe matches your held items" on no match - a real
  rejection path, not silently ignored.
- [x] Purely client-side, single-player and networked alike - crafting
  never touches the `World` or needs server validation (same
  client-authoritative precedent as item pickup itself, see
  DECISIONS.md), so it needed zero protocol/server changes.
- [x] New standalone headless hook `LCU_VERIFY_CRAFT` (wall-clock-
  gated like `LCU_VERIFY_MOVE_SECONDS`, not frame-count-gated like
  `LCU_VERIFY_BREAK_PLACE` - see the real bug this caught, below):
  breaks the spawn grass block, breaks the dirt block beneath it,
  crafts (should match), crafts again (should reject, only compost
  held by then) - exercising both `find_match` outcomes in one run.
- [x] A real bug caught and fixed by real networked verification, not
  by reasoning alone: the hook's first version gated the two breaks by
  frame count (mirroring `LCU_VERIFY_BREAK_PLACE`'s style), which
  worked single-player but broke networked mode - the unthrottled
  client loop ran hundreds of frames (verified: even 200 wasn't
  enough) before the first break's `BlockChange` round-tripped back,
  so the second break's raycast still saw the old, unbroken grass
  block and re-requested breaking the *same* position, optimistically
  double-granting the item before the server's rejection + inventory
  correction arrived. Switched to a wall-clock-gated state machine
  (same pattern `LCU_VERIFY_MOVE_SECONDS`, Phase 16, already used for
  the identical class of problem) - confirmed fixed via a second real
  networked run: server applies both breaks at their correct distinct
  positions, zero warnings, craft succeeds, reject path still fires.

No new unit tests - this phase is pure orchestration of already-tested
`RecipeRegistry`/`Inventory`/`ItemRegistry` primitives, exercised by
the real runs above. `ctest` unchanged at 347/347 (bgfx) / 344/344
(non-bgfx).

Honestly scoped: quick-craft's auto-built grid only correctly
represents a recipe needing exactly one of each distinct ingredient
type (true of the one recipe registered) - it isn't a stand-in for a
real grid that could hold more than one of the same item in different
cells; no graphical crafting-grid UI exists (no way to arrange items
into specific cells - Craft press is the entire interaction, feedback
is a log line); shaped-recipe matching still has zero real caller
(only shapeless is exercised by this quick-craft design).

Next per brief section 10's priority order - see PROJECT_STATE.md "Next
Task": more block/item variety, then modding depth (a second real
event beyond `block_broken`), then platform verification, then
performance work, then UI/audio polish.

---

## Phase 24 — item_crafted: EventBus's second real event (post-original-queue; closes a Phase 9 gap long-flagged as modding depth)

`EventBus` (Phase 9) only ever had one real event - `block_broken` -
with its own doc comment honestly stating "add another `emit_<event>()`
the same way once a second real event exists to validate the shape
against." Phase 23's quick-craft gave the project its first genuinely
new gameplay moment since Phase 13 worth exposing to mods.

- [x] New `EventBus::emit_item_crafted(item_id, count)`, same
  error-isolated-per-subscriber pattern as `emit_block_broken`.
- [x] `VoxelClient`'s quick-craft handler (Phase 23) calls it right
  after a successful `find_match` + item grant.
- [x] Purely client-side, like crafting itself - `VoxelServer` never
  calls it (crafting doesn't happen there), but still exposes
  `lcu.subscribe("item_crafted", ...)` since a mod script is shared
  between both hosts and must load identically on either.
- [x] `example_mod/init.lua` extended to subscribe to both events now,
  proving the real register -> load -> subscribe -> emit loop
  generalizes beyond `block_broken` alone, not just that a second typed
  emit method compiles.
- [x] Fixed a stale comment in `server/main.cpp` claiming "the server
  never calls emit_block_broken() itself" - Phase 13 made that false
  (block edits are server-authoritative, so the server's own break
  handling is where `emit_block_broken` actually fires); noticed while
  touching the same code for `item_crafted`'s opposite case.
- [x] 3 new unit tests (`EventBus.EmitItemCrafted*`,
  `EventBus.BlockBrokenAndItemCraftedSubscribersAreTrackedIndependently`).
- [x] Verified via a real single-player run (`LCU_VERIFY_CRAFT`):
  `[example_mod] item_crafted #1: 1 x item id 4` logs at the exact
  moment compost is crafted, right after `"Crafted 1 game:compost
  (inventory: 1)"`. Verified the server still loads the same mod file
  cleanly (no load failure from the new `item_crafted` subscription
  it never fires).

`ctest` 350/350 (bgfx, up from 347) / 347/347 (non-bgfx, up from 344).

Honestly scoped: still no manifest/dependency/version format for
`ModLoader`; mod-registered ids still aren't synced over the network;
only two real events now (`block_broken`, `item_crafted`) - a third
still needs a genuine third engine-side moment to justify it, not
speculative expansion.

Next per brief section 10's priority order - see PROJECT_STATE.md "Next
Task": more block/item variety, then platform verification, then
performance work, then UI/audio polish.

---

## Phase 25 — macOS build audit (user-directed: start of Phases 25-42, visible game + global lighting + procedural terrain)

The user wants to run `VoxelClient` on a real Mac and see it for the
first time. This sandbox cannot run a macOS toolchain, so this phase
is a real code audit (every CMake/FetchContent path, every existing
macOS-specific branch), not a build attempt - result marked **NOT
VERIFIED — ENVIRONMENT LIMITATION**, not TESTED, per this project's
own discipline.

- [x] Audited `third_party/CMakeLists.txt`: no Linux-only assumptions
  in any of the 7 fetched dependencies.
- [x] Audited `engine/network`'s socket code (already POSIX/Winsock
  branched correctly), `engine/platform`'s native-window-handle code
  (already has a correct macOS Cocoa branch), and bgfx.cmake's own
  macOS framework linking (Cocoa/Metal/QuartzCore/IOKit - zero
  Homebrew packages needed beyond `cmake`/`ninja`).
- [x] Confirmed `bgfx_compile_shaders()` already auto-compiles a
  `metal` profile on an `APPLE` host with zero code change needed.
- [x] **Found and fixed a real bug**: `active_shader_profile_dir()`
  had no case for `bgfx::RendererType::Metal`, would have loaded the
  wrong shader binary format on macOS. Fixed with one added `case`.
- [x] New "macOS" section in `BUILDING.md`: prerequisites, configure/
  build/run commands, what a real run should show.

`ctest` unchanged at 350/350 (bgfx) / 347/347 (non-bgfx). See
DECISIONS.md "Phase 25" for the full audit writeup.

---

## Phase 26 — Visible terrain: per-block/per-face colors + procedural shader noise

Closes the first of four things flagged as "not what the user expects
to see": the chunk shader (still the Phase 21 placeholder) never used
per-block color, so every block rendered the same flat gray-blue.

- [x] `BlockDefinition` gained `color` (top-face/default tint) plus
  optional `side_color`/`bottom_color` overrides - real per-face
  color selection happens in `mesh_chunk_greedy` (which already knows
  the axis + facing direction it's building a quad for), not a
  shader-side special case for any specific block name.
- [x] `MeshVertex` gained a `color` field (appended last, matching a
  new `Color0` attribute appended last to the bgfx vertex layout -
  `chunk_mesh_upload.cpp`'s `.add()` order must match the struct's
  memory layout exactly, since it's `memcpy`'d straight into a GPU
  buffer).
- [x] Registered real colors: `game:stone` gray, `game:grass` green
  top / brown sides (the classic grass-block convention, achieved via
  data, not a shader hack), `game:dirt` brown. `game:water`/
  `game:sand`/`game:compost` colors are deferred to their own later
  phases (37+) rather than tinting blocks that don't exist yet - see
  DECISIONS.md.
- [x] `client/shaders/{vs_chunk,fs_chunk}.sc` rewritten: the vertex
  shader passes color and an un-transformed chunk-local position
  through; the fragment shader multiplies the real per-face color by
  the existing directional light (plus a small explicit top-face lift)
  and a subtle procedural hash-noise pattern (deterministic per voxel,
  not per frame) - covers "grau mit subtilem Noise" (stone), "braunes
  Rauschen" (dirt) generically, with no per-block branching in the
  shader itself.
- [x] Real bug caught while wiring this up: `engine/voxel`'s
  `BlockDefinition`/`greedy_mesher.h` used `math::Vec3` without
  `LcuVoxel` ever linking `Lcu::Math` - previously an undeclared
  transitive dependency that happened to work only because every real
  consumer also linked `Lcu::Math` some other way; adding a `Vec3`
  field to `BlockDefinition` broke `block_registry.cpp`'s own
  compilation and exposed it. Fixed by adding the missing link.
- [x] 2 new unit tests (`GreedyMesher.PerFaceColorUsesTopSideBottom
  FallbackChain`, `GreedyMesher.UnsetSideAndBottomColorFallBackTo
  TopColorOnEveryFace`) - the per-face color selection is fully
  testable headlessly (it's C++ data selection, not a shader branch),
  unlike the shader's own noise pattern.
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` build (the only
  way to actually compile `.sc` shaders through bgfx's `shaderc`, not
  just compile the C++ side): `"Chunk shader program valid=true"` -
  proves the new `Color0` attribute, varying wiring, and fragment
  shader logic all link correctly through the real bgfx shader
  pipeline. Verified via a real headless run (`LCU_VERIFY_BREAK_PLACE`)
  under that same shader-enabled build that break/place still works
  with the real program loaded, zero regressions.

`ctest` 352/352 (bgfx) / 349/349 (non-bgfx), up from 350/347.

Honestly scoped: **what a real GPU/display would actually show is
still NOT VERIFIED — ENVIRONMENT LIMITATION** - this sandbox has no
GPU/display, so "Chunk shader program valid=true" proves the shader
*compiles and links*, not that it *looks right*. No texture atlas
exists yet (Phase 12) - this is color-only surface detail, not
texturing. `game:water`/`game:sand` colors aren't registered yet since
those blocks don't exist until Phase 37/39.

## Phase 27 — Skybox + sun/moon

Closes the second of four things flagged as "not what the user expects
to see": there was no sky color variation and no visible sun/moon at
all, just a single flat clear color.

- [x] `Renderer::begin_frame` changed from a pre-packed `u32` clear
  color to an explicit `Vec3` (+ alpha) - a real API improvement, not
  a redundant alias, since every caller now passes real float
  components instead of hand-packing hex.
- [x] Sky color interpolated every frame from the existing
  `DayNightCycle::sky_light_scale()` - night (0.02, 0.03, 0.08), day
  (0.45, 0.65, 0.95), linear interpolation - reusing the one real time
  signal instead of a second, separately-tuned animation clock.
- [x] Sun/moon rendered as camera-facing billboard quads via a new
  `Renderer::submit_billboard()`, built from the camera's own
  `right()`/`cross(right, forward)` basis vectors so they always face
  the camera. Position comes from a new pure `game::systems::sun_
  direction(time_of_day)` (sun yellow-white, moon pale blue-white,
  moon always exactly opposite the sun).
- [x] Own bgfx view (`kSkyViewId`), depth test off on the sky quad
  itself ("Tiefentest aus", per spec) - real occlusion instead comes
  from `bgfx::setViewOrder` making the sky view execute *before* the
  terrain view, so terrain's normal depth test against the freshly-
  cleared depth buffer naturally hides the sky wherever a block is
  actually in front of it.
- [x] Stars at night: explicitly listed as optional in the brief
  ("Sterne bei Nacht optional") - deliberately deferred to control
  scope, not a missing/fake feature.
- [x] Real bugs found and fixed: (1) `bgfx::allocTransientVertexBuffer`/
  `allocTransientIndexBuffer` return `void` in this bgfx version, not
  `bool` - fixed via `getAvailTransientVertexBuffer`/`getAvailTransient
  IndexBuffer` pre-checks (grepped the actual bgfx header to confirm,
  didn't guess); (2) `sun_direction`'s formula was first inlined
  directly in `client/main.cpp` (untestable, nothing in `client/` is
  unit-tested) - extracted into `game::systems::sun_direction()` next
  to `DayNightCycle`, which needed `LcuGame` to gain an explicit
  `Lcu::Math` link (proactively avoiding a repeat of Phase 26's
  identical `LcuVoxel`/`Lcu::Math` transitive-include bug).
- [x] 6 new unit tests (`SunDirection.*`): direction at all four phase
  points (dawn/noon/dusk/midnight), unit-length-in-the-xy-plane, and
  moon-always-exactly-opposite-sun.
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` build: `"Sky
  shader program valid=true"` alongside the existing `"Chunk shader
  program valid=true"` - both real shader programs link through the
  real bgfx pipeline. Verified via a real headless run
  (`LCU_VERIFY_BREAK_PLACE`) under that same build that break/place
  still works with both programs loaded, zero regressions.

`ctest` 358/358 (bgfx) / 355/355 (non-bgfx), up from 352/349.

Honestly scoped: **what a real GPU/display would actually show - sky
color, sun/moon visibility, and the occlusion behavior described above
- is still NOT VERIFIED — ENVIRONMENT LIMITATION**. Stars deferred
(see above).

## Phase 28 — Renderer nutzt Licht (renderer consumes per-voxel light)

Closes the third of four things flagged as "not what the user expects
to see": `engine/lighting` had computed real per-chunk sky/block light
since Phase 6, but nothing in the rendering path ever read it - the
chunk shader lit every face with a fixed fake directional light with no
relationship to that real data (or to Phase 27's real sun/moon).

- [x] `MeshVertex` gained a packed `u8 light` field - low nibble sky,
  high nibble block, the exact `lcu::lighting::LightStorage` packing,
  one byte total ("Licht wird als EIN Byte im Vertex gepackt").
- [x] `mesh_chunk_greedy` reads real light per face from the air cell
  it's actually exposed to (not the solid block's own cell - real
  light propagation never targets opaque cells), computed once by
  `engine/lighting` at chunk load/edit time, never recomputed by
  meshing or the shader per frame.
- [x] Fragment shader formula updated to
  `final = color * (sky * sky_scale + block) / 15.0`, `sky_scale` a
  new `u_skyLightScale` uniform set once per draw call from the
  existing `DayNightCycle::sky_light_scale()` (the same real time
  signal Phase 27's skybox already reuses, not a second lighting
  clock). Replaces the old Phase 26 fake directional light entirely -
  keeping both would have double-counted daylight and never actually
  darkened the world at night.
- [x] Merging now also requires equal light (`MaskCell::merges_with`),
  not just equal block id/facing - otherwise greedy meshing's own
  optimization would silently flatten a real per-voxel lighting
  gradient into one arbitrary quad-wide brightness.
- [x] Real architecture decision: `mesh_chunk_greedy` takes light via a
  duck-typed template parameter (`LightStorageT`) instead of
  `#include`-ing a concrete `lcu::lighting` header - `engine/lighting`
  already depends on `engine/voxel`, so the reverse include would be a
  circular target dependency. A light-less two-argument overload
  (an always-full-bright stand-in) keeps every existing call site
  (tests, `tools/benchmark`) unchanged; only `client/main.cpp`'s real
  remesh path passes its actual per-chunk light.
- [x] Real bug found and fixed: `MeshVertex` had never before ended in
  a byte-sized field, so the compiler now pads its total size up to a
  4-byte multiple that `bgfx::VertexLayout`'s tightly-packed `.add()`
  sum knows nothing about - left alone, every vertex after the first
  would have silently read from the wrong GPU-buffer offset. Fixed via
  `layout.skip(sizeof(MeshVertex) - layout.getStride())` plus an
  `LCU_ASSERT` keeping the two in sync, which did execute against real
  36-chunk production data in this phase's verification run without
  firing.
- [x] 4 new unit tests: the light-less overload stays full-bright, a
  face reads light from its exposed air cell (not the solid block),
  differently-lit coplanar faces don't merge, and a true chunk-boundary
  face (no cross-chunk light yet) defaults to full-bright rather than
  reading out of bounds or guessing dark.
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` build: `"Chunk
  shader program valid=true"` (the new `Color1`/`u_skyLightScale`
  wiring links correctly), plus a real headless `LCU_VERIFY_BREAK_PLACE`
  run under that build with the real 36-chunk world's real light data
  flowing through meshing, zero regressions/crashes.

`ctest` 362/362 (bgfx) / 359/359 (non-bgfx), up from 358/355.

Honestly scoped: **what real per-voxel lighting actually looks like on
a real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**.
Cross-chunk light doesn't exist yet - a chunk-boundary face
unconditionally defaults to full-bright, honestly, not guessed (Phase
29-31's job); lighting isn't smoothed per-vertex yet (Phase 33).

## Phase 29 — WorldLight-Datenstruktur (WorldLight data structure)

Prerequisite for Phase 30 (sky) / Phase 31 (block) cross-chunk light
propagation: a BFS that crosses a chunk boundary needs to read and
write light in a *neighboring* chunk's `LightStorage`, which a
per-chunk-only map (or a bare `std::unordered_map<ChunkCoord, Light>`)
can't do without every caller reimplementing chunk-boundary coordinate
resolution itself.

- [x] New `lcu::lighting::WorldLight<EdgeLength>` - an owning
  `ChunkCoord -> LightStorage` map plus `sky_light_at`/`block_light_at`
  queries that resolve a local coordinate outside `[0, EdgeLength)`
  into its real owning chunk, reusing `voxel::world_to_chunk_and_local`
  (the same floor-division helper `engine/world` already uses for block
  edits) rather than a second hand-written version of that arithmetic.
- [x] Both queries return `std::optional<u8>`: `std::nullopt` means
  "that chunk's light isn't computed" (not loaded, or loaded but not
  yet lit) - honestly, never a guessed brightness. Matches the same
  discipline `mesh_chunk_greedy`'s own boundary-face fallback (Phase
  28) already established for chunk-edge light.
- [x] `find_chunk_light`/`find_chunk_light_mutable` naming mirrors
  `engine/world::World`'s existing `chunk_at`/`chunk_at_mutable` split
  - consistency with an established convention, not a new one.
- [x] `client/main.cpp`'s Phase 6-era ad hoc
  `std::unordered_map<ChunkCoord, Light> chunk_light` replaced with a
  real `DefaultWorldLight world_light` - every call site (initial light
  compute, per-edit incremental update, meshing, the startup "sky light
  above spawn" debug log) now goes through the new type's real API.
- [x] Deliberately does NOT propagate light across a chunk boundary
  yet - a torch near a chunk edge still stops exactly at that edge,
  identical to before this phase. Phase 30/31's BFS is what will
  actually walk across the boundary and write into the neighbor; this
  phase is only the data structure and query surface those phases
  build on.
- [x] 9 new unit tests: in-bounds same-chunk lookup, positive- and
  negative-direction cross-chunk boundary resolution, not-loaded-
  neighbor returns `std::nullopt`, get-or-create/find/remove map
  hygiene.
- [x] Verified via real `LCU_VERIFY_BREAK_PLACE` runs (both bgfx and
  non-bgfx builds): byte-identical log output to Phase 28
  (`"Sky light 5 blocks above spawn column: 15"`), confirming this is a
  real, behavior-preserving refactor, not just new code that happens to
  compile.

`ctest` 371/371 (bgfx) / 368/368 (non-bgfx), up from 362/359.

Honestly scoped: light still doesn't actually cross a chunk boundary -
that's Phase 30 (sky)/Phase 31 (block)'s job, built on this phase's
data structure.

## Phase 30 — Sky-Light cross-chunk (sky-light cross-chunk propagation)

Sky light only ever travels straight down in this engine (a Phase 6
known simplification, unchanged here), so "cross-chunk propagation"
for it is a seeded column scan, not a BFS - unlike Phase 31's block
light, which genuinely needs one.

- [x] `compute_sky_light_column` gained a `sky_open_above` parameter
  (default `true` - every existing caller's behavior is unchanged) -
  `false` starts the column pre-blocked instead of open.
- [x] New `compute_sky_light_column_cross_chunk`/`compute_sky_light_
  cross_chunk`: query the chunk directly above via `WorldLight::
  sky_light_at` (its bottom cell alone is enough - a blocked column is
  always uniformly 0 top-to-bottom) to supply a real `sky_open_above`.
  An unloaded/not-yet-lit neighbor still means "assume open" (same
  convention `WorldLight` itself already established).
- [x] Real bug fixed: a chunk with a solid roof directly above it (in
  the neighboring chunk) now actually shows 0 sky light at its own top
  layer, instead of the old always-full-brightness-at-the-chunk's-own-
  top behavior.
- [x] `client/main.cpp`'s load loops (initial spawn-area load,
  per-movement streaming) restructured into three passes per (x,z)
  column: block light (any order), sky light (**strictly top-down**,
  highest `chunk_y` first - the cascade doesn't work otherwise), then
  meshing. The networked `ChunkData` receipt path (single chunk,
  arbitrary vertical arrival order) uses the same split but honestly
  can't guarantee that ordering by itself - documented as a known gap,
  closed by Phase 35's neighbor-dirtying.
- [x] 4 new unit tests: no-neighbor-above matches the old default, a
  solid roof one chunk up genuinely darkens the chunk below, open sky
  above leaves it fully lit, and a chunk's own internal roof still
  shadows regardless of what's above it.
- [x] Verified via a real two-process networked run (`VoxelServer` +
  `VoxelClient`, loopback UDP): all 36 chunks received via `ChunkData`,
  correctly lit and meshed through the new split path, break/place
  round-trips cleanly, zero warnings/errors. Verified via real
  `LCU_VERIFY_BREAK_PLACE` runs (bgfx and non-bgfx): byte-identical
  `"Sky light 5 blocks above spawn column: 15"` output to Phase 29 (the
  topmost loaded chunk still has nothing above it, unchanged).

`ctest` 375/375 (bgfx) / 372/372 (non-bgfx), up from 371/368.

Honestly scoped: block light still doesn't cross a chunk boundary at
all (Phase 31, a genuine BFS); a chunk loaded/edited *after* an
already-lit neighbor below it doesn't yet retroactively relight that
neighbor (Phase 35); lateral sky light bleed under overhangs remains
an unchanged, documented simplification; what a real cross-chunk shadow
actually looks like on a real GPU/display is still NOT VERIFIED —
ENVIRONMENT LIMITATION.

## Phase 31 — Block-Light cross-chunk (block-light cross-chunk propagation)

Unlike Phase 30's sky light (straight-down only, a seeded column
scan), block light genuinely floods in all 6 directions - crossing a
chunk boundary needs a real BFS frontier that continues into the
neighbor chunk's own `LightStorage`, not just one seed value.

- [x] New `flood_block_light_cross_chunk`/`propagate_added_block_
  light_cross_chunk`/`unpropagate_block_light_cross_chunk`: the same
  decrement-and-spread (and two-phase darken-then-refill removal)
  algorithm as the existing single-chunk versions, generalized so a
  step that would leave the current chunk resolves into its real
  neighbor (reusing `voxel::world_to_chunk_and_local`) instead of being
  clipped at the boundary.
- [x] Templated on a duck-typed `ChunkProviderT` (matches
  `lcu::world::World::chunk_at(ChunkCoord) const` exactly) - same
  reasoning as Phase 28's `LightStorageT`: `engine/world` doesn't
  depend on `engine/lighting`, so a concrete dependency would actually
  be cycle-safe, but the template keeps this header testable without a
  full `World` instance.
- [x] An unloaded neighbor chunk is never crossed into or written to -
  honestly nothing to propagate into, not a guess (matches
  `WorldLight`'s own convention). A chunk that loads *later* doesn't
  retroactively receive light from a BFS that already finished -
  Phase 32 (optional boundary buffer) / Phase 35 (neighbor-dirtying)
  territory, not this phase's.
- [x] "BFS queue only runs over the radius actually affected by a
  change (max 15 voxels)" falls out of the existing algorithm for
  free: light is capped at `kMaxLightLevel` (15) and the BFS already
  stops once a cell's level would decrement to 0 - no separate radius
  cap needed.
- [x] `client/main.cpp`'s `update_lighting_for_edit` now calls the
  cross-chunk versions (passing `world` itself as `ChunkProviderT`);
  its "let light flow back in from the brightest neighbor" logic now
  queries `WorldLight::block_light_at` instead of only checking
  same-chunk neighbors - a real correctness fix this phase's own wiring
  surfaced, not separately motivated.
- [x] 4 new unit tests: light crossing into a loaded neighbor with
  correct continued decay, not crossing into an unloaded neighbor,
  removing a cross-chunk source darkens both chunks, and removing one
  of two cross-chunk sources correctly refills the overlap from the
  remaining one (the trickiest case - no dark gap, no wrongly-darkened
  survivor).
- [x] Verified via a real two-process networked run (`VoxelServer` +
  `VoxelClient`, loopback UDP): a real break/place round-trip through
  the new cross-chunk edit path, zero warnings/errors. Verified via
  real `LCU_VERIFY_BREAK_PLACE` runs (bgfx and non-bgfx): byte-identical
  output to Phase 30.

`ctest` 379/379 (bgfx) / 376/376 (non-bgfx), up from 375/372.

Honestly scoped: a chunk that loads after a nearby source's BFS
already finished doesn't yet retroactively receive that light (Phase
32/35's job); what real cross-chunk torchlight actually looks like on
a real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION.

## Phase 32 — Grenzpuffer (boundary buffer, optional) — SKIPPED

Explicitly optional in the brief. Its stated purpose ("cross-chunk BFS
never blocks - boundary condition buffered") is a concurrency
optimization: deferring a cross-chunk light write into a buffer so two
lighting computations running on different threads don't contend for
the same neighbor chunk's data. Nothing in this codebase dispatches
lighting work onto multiple threads today - every lighting call (Phase
6/30/31 alike) runs synchronously on the main thread against one
shared `WorldLight`. With no concurrent access anywhere, there is no
actual blocking for a boundary buffer to prevent; building one now
would be optimizing against a problem that doesn't exist yet - see
DECISIONS.md for the full reasoning and what would make this worth
revisiting.

## Phase 33 — VoxelClient-Integration + smooth lighting

Two real fixes: closing an actual remesh gap the cross-chunk work
(Phase 30/31) left open, and real per-vertex smooth lighting.

- [x] **VoxelClient integration**: `flood_block_light_cross_chunk`/
  `propagate_added_block_light_cross_chunk`/`unpropagate_block_light_
  cross_chunk` gained an optional `touched_chunks` output set - every
  chunk (besides the edited one) that actually got a light write.
  `client/main.cpp`'s `update_lighting_for_edit` now returns it, and a
  new `remesh_edit_neighbors` helper remeshes the union of that real
  set with the existing geometric `neighbors_sharing_boundary` - closes
  a real gap where an edit not exactly at a chunk's boundary local
  coordinate could still cross-chunk-light a neighbor (any edit within
  light-emission range of a boundary) that never got remeshed.
- [x] **Smooth lighting**: `mesh_chunk_greedy`'s merged quads are now
  lit per-vertex, not one flat value per quad - each of a quad's 4
  corners independently averages the packed light of its up-to-4
  diagonally-adjacent mask cells (`detail::smooth_corner_light`), the
  classic vertex-light-averaging technique (no ambient occlusion,
  deliberately out of scope - see DECISIONS.md).
  `ChunkMeshLayer::add_quad` takes 4 separate per-vertex light bytes.
- [x] `MaskCell::merges_with` no longer compares light (Phase 28's
  flat-shading-only restriction is superseded - shading is per-corner
  now) - merging is purely geometric/material again, a superset of
  Phase 28's more-restrictive merge set.
- [x] Real payoff found while wiring this up: **no shader changes were
  needed**. `v_color1`'s existing varying (Phase 28) was already
  non-`flat`, so bgfx/GLSL already linearly interpolates it across a
  triangle by default - different per-vertex values now produce real
  GPU-interpolated smooth shading automatically, no `.sc` file
  touched.
- [x] 6 new unit tests: 3 for `smooth_corner_light` directly (full
  4-cell average, mask-edge fewer-cells case, degenerate empty-mask
  fallback), 2 Phase-28 tests rewritten for the new semantics (a single
  isolated quad's by-symmetry-uniform averaged value; two differently-
  lit adjacent faces now merging into one quad with a real per-corner
  gradient instead of staying separate).
- [x] Verified via a real two-process networked run (`VoxelServer` +
  `VoxelClient`, loopback UDP) and real `LCU_BUILD_SHADER_TOOLS=ON` +
  `LCU_VERIFY_BREAK_PLACE` runs (bgfx and non-bgfx): zero regressions,
  byte-identical output to Phase 31.

`ctest` 385/385 (bgfx) / 382/382 (non-bgfx), up from 379/376.

Honestly scoped: **what smooth lighting actually looks like on a real
GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; no
ambient occlusion; a chunk loading after a nearby source's BFS already
finished still isn't retroactively relit or remeshed (Phase 35's job).

## Phase 34 — Torch block + lighting benchmarks

New light-emitting content, plus meeting the brief's own explicit
lighting-performance budgets for real, not just claiming them.

- [x] **`game:torch`**: `light_emission=14`, deliberately `is_
  transparent=false` - a transparent torch would be correctly lit but
  completely invisible (`mesh_chunk_greedy` only meshes the opaque
  layer, and no transparent-layer mesher exists), a real fake-feature
  trap caught and corrected before any verification run, not after -
  see DECISIONS.md. Registered identically on `VoxelClient`/
  `VoxelServer`; wired into `placeable_items`, `block_item_mapping`,
  `tracked_items`.
- [x] New headless hook `LCU_VERIFY_TORCH`: breaks the spawn block,
  grants one `game:torch` item directly (nothing drops one yet),
  hotbar-cycles to it, places it into the hole, and logs the real
  post-place `block_light` value read back through `WorldLight::
  block_light_at` - the full place -> propagate -> query pipeline,
  verified via a real run: `"Placed game:torch at world (0, 28, -1):
  block_light=14"`.
- [x] 3 new `tools/benchmark` cases against the brief's own explicit
  budgets (not the file's usual "numbers to profile against"):
  `BM_Lighting_ComputeChunkWithNeighbors` (< 2ms), `BM_Lighting_
  PlaceTorchAtChunkEdge`/`BM_Lighting_UnplaceTorchAtChunkEdge` (each
  < 0.5ms, torch placed at the worst-case chunk-edge position).
- [x] **Build-configuration finding**: `CMAKE_BUILD_TYPE=
  "Development"` (this project's only configured value) is not a
  CMake built-in type, so no optimization flags were ever applied to
  any target - confirmed by Google Benchmark's own `Library was built
  as DEBUG` warning on the first run. Created a dedicated `build/
  bench-release` directory (`-DCMAKE_BUILD_TYPE=Release`, confirmed
  `-O3 -DNDEBUG` in its cache) to get trustworthy numbers - see
  DECISIONS.md. Left as a documented, project-wide gap outside this
  one benchmark directory (not fixed this phase, out of scope).
- [x] **Budget miss found and fixed for real**: even under genuine
  `-O3`, place/unplace still exceeded budget (639,123 ns / 766,988 ns
  vs. 500,000 ns). Root-caused to redundant `unordered_map` lookups
  (two separate maps, ~13 per popped BFS cell) and unconditional
  floor-division for the common case where a BFS step stays within
  the current chunk. Added an in-bounds fast path (plain integer range
  check, reuses already-held pointers, zero hash lookups) to both
  `flood_block_light_cross_chunk` and `unpropagate_block_light_cross_
  chunk`, falling back to the original logic only for genuine
  boundary crossings - behavior-preserving by construction, confirmed
  via the unchanged full `ctest` suite (385/385 bgfx / 382/382
  non-bgfx) before and after. Re-measured: 115,649 ns / 99,158 ns -
  both now comfortably under budget; `BM_Lighting_
  ComputeChunkWithNeighbors` (16,616 ns) stayed comfortably under its
  2ms budget throughout.
- [x] Verified via a real two-process networked run (`VoxelServer` +
  `VoxelClient`, loopback UDP, 600 server ticks, 36 chunks streamed,
  zero warnings/errors) and real `LCU_VERIFY_TORCH`/`LCU_VERIFY_
  BREAK_PLACE`/`LCU_VERIFY_CRAFT` runs (bgfx) - all pre-existing hooks
  behave identically to Phase 33, confirming the BFS optimization
  changed nothing observable.

`ctest` unchanged at 385/385 (bgfx) / 382/382 (non-bgfx) - no new unit
tests this phase (the BFS change is proven behavior-preserving by the
existing suite; the new content is proven by the real `LCU_VERIFY_
TORCH` run and the new benchmarks).

Honestly scoped: **what a placed, lit torch actually looks like on a
real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; the
`CMAKE_BUILD_TYPE="Development"` no-optimization gap remains project-
wide outside `build/bench-release`; a chunk loading after a nearby
source's BFS already finished still isn't retroactively relit or
remeshed (Phase 35's job, next).

## Phase 35 — Chunk unload marks neighbors dirty

The two Phase 30/31 "arrived too late" lighting gaps, plus the literal
"chunk unload" this phase is named for.

- [x] **`reseed_light_for_newly_loaded_chunk`**
  (`engine/lighting/propagation.h`): call once, right after a chunk's
  own initial light is computed. Block light: walks every already-
  loaded neighbor's shared boundary face, collecting every currently-
  lit cell on *both* sides into one queue, then re-floods via the
  existing `flood_block_light_cross_chunk` (safe and idempotent, since
  that function only ever raises a value). Sky light: recomputes every
  already-loaded chunk in the vertical run below `coord`, cascading
  through however many happen to be stacked. Returns every chunk this
  call's light actually touched.
- [x] 4 new unit tests (`ReseedLightForNewlyLoadedChunk.*`): late-
  arriving block light in both directions, a hand-verified two-chunk
  sky-light cascade, and the no-already-loaded-neighbors no-op case.
- [x] Wired into `VoxelClient`'s initial spawn-area load, per-movement
  streaming, and the networked `ChunkDataFragment` receipt path -
  closing the exact gap that handler's own code comment named this
  phase for.
- [x] **Real client-side chunk unloading**: a distance-gated sweep
  (`unload_far_chunks`, Chebyshev XZ beyond `load_radius + 1`, the
  client's own counterpart to `VoxelServer`'s Phase 20 interest-scoped
  unloading) runs on every streaming-center change. Saves the chunk to
  `client_world/chunks/` first (mirroring `VoxelServer`'s own save-
  before-unload exactly - `WorldLight::remove_chunk_light`'s own doc
  comment has named this phase as its real caller since Phase 29),
  destroys its GPU mesh, removes its light data, then unloads it.
  Loading now checks that same directory before regenerating.
- [x] Two new diagnostic log lines (`"Streaming center moved to
  ..."`/`"Unloaded N chunk(s) beyond streaming range ..."`) plus a
  single-player `"Player position: ..."` shutdown line for parity with
  the existing networked-mode one.
- [x] **A real, pre-existing (not introduced by this phase) finding**:
  `LCU_VERIFY_MOVE_SECONDS` moves in a straight line and never jumps,
  so it can get legitimately blocked by terrain taller than the
  player's auto-step height - confirmed byte-identical against a
  pre-Phase-35 build under the same test. Documented in DECISIONS.md,
  not fixed (out of this phase's scope) - this phase's own real-run
  verification temporarily also held Jump to clear the obstacle.
- [x] Verified via a real, longer-distance single-player run: repeated
  `"Streaming center moved to ..."`/`"Unloaded 12 chunk(s) ..."` pairs
  firing correctly across multiple chunk boundaries, loaded count
  steady at 48 (load/unload balance correct), 60 real `.chunk` files
  written. Also verified byte-identical via `LCU_VERIFY_TORCH`/
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_CRAFT` and a real two-process
  networked run (600 server ticks, 36 chunks streamed, zero
  warnings/errors).

`ctest` 389/389 (bgfx, up from 385) / 386/386 (non-bgfx, up from 382).

Honestly scoped: **what any of this looks like on a real GPU/display
is still NOT VERIFIED — ENVIRONMENT LIMITATION**; the reload-from-disk
half of client persistence wasn't separately re-exercised end-to-end
in this run (the verify hook only moves one direction) - save-on-
unload is proven by the 60 real files written, and load-from-disk
reuses the exact same `chunk_serializer` API `VoxelServer` already
round-trip-tests; lateral sky light bleed under overhangs remains
unmodeled (documented since Phase 6).

## Phase 36 — Entity boxes + extended debug overlay

Real entity visualization plus real, honestly-scoped overlay numbers.

- [x] **Entity debug boxes**: new `Renderer::submit_wireframe_box`
  draws a 12-edge line-list box, reusing `submit_billboard`'s exact
  position+color vertex format/shader (Phase 27's sky program) rather
  than a third shader pair. Real depth testing against terrain, no
  depth write. Wired into `VoxelClient`: one box per local AI entity
  (single-player) or remote interpolated entity (networked), reusing
  `make_player_aabb` (the exact box shape the player's own collision
  already uses).
- [x] **Extended debug overlay**: new `DebugOverlayStats` struct
  carries chunks loaded, entity count, real draw-call count, and
  unfinished job count into `draw_debug_overlay`'s new second
  on-screen text line. New `JobSystem::unfinished_job_count()`
  accessor (lock-guarded, 3 new unit tests).
- [x] **Deliberately not added**: CPU/GPU/RAM/ping/bandwidth - no real
  per-platform CPU/RAM reader or per-connection RTT/byte-counter
  exists in this codebase yet, and a fake placeholder number would
  violate this project's own "never claim more than what's verified"
  discipline (brief section 96) - see DECISIONS.md.
- [x] Draw-call counting mirrors each `submit_*` call's own no-op-on-
  invalid-program condition (every `submit_*` silently no-ops when its
  shader program is invalid), so it reflects what actually reached
  `bgfx::submit()`, never an over-count.
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run
  (`"Chunk shader program valid=true"`/`"Sky shader program
  valid=true"` - confirming the new wireframe-box draw call executes
  against real compiled shaders, not just `Noop`), a real
  `LCU_VERIFY_BREAK_PLACE` run (zero regressions), and a real
  two-process networked run (100 frames, 3 remote AI entities
  interpolated and boxed every frame, zero warnings/errors).

`ctest` 392/392 (bgfx, up from 389) / 389/389 (non-bgfx, up from 386).

Honestly scoped: **what the wireframe boxes or overlay text actually
look like on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
LIMITATION**; CPU/GPU/RAM/ping/bandwidth remain deliberately absent
from the overlay until this codebase has a real source for them
(Phase 42's documentation pass, or a dedicated future phase, would be
the place to revisit if ever prioritized).

## Phase 37 — Sea level at y=0 + water block/rendering

Real sea level, a real water block, and a real dry spawn - not just
worldgen numbers moving, three genuinely coupled changes.

- [x] **`worldgen::kSeaLevel`** (world Y=0, new exported constant):
  `terrain_height()` recentered from an always-positive [8,56] range
  to [-20,20], centered on sea level - roughly half of all columns
  land above it (dry land/hills), half below (real lake/ocean basins).
- [x] **`game:water`**: `generate_terrain_chunk` gained a `water_block`
  param, filling any below-sea-level column's gap up to Y=0 (a dry
  column is completely unaffected). `is_transparent=false` (the torch
  precedent - no transparent-layer meshing exists, `true` would make
  it invisible), `has_collision=false` (real - the same `has_
  collision`-driven `is_solid` predicate every block's collision
  already uses, so a player genuinely walks/swims through it; no
  buoyancy/drag beyond that).
- [x] **Quality profiles re-centered**: all four `ChunkLoadSettings`
  tiers shifted `min_chunk_y`/`max_chunk_y` to straddle sea level
  (Desktop: 1/0/3 -> 1/-1/2) while keeping every tier's *total* chunk
  count unchanged (1/18/27/36) - every prior "Loaded N chunks" claim
  stays numerically true.
- [x] **Real dry spawn placement**: a fixed (0,0) spawn column could
  now legitimately land underwater by pure chance (no swim mechanics
  exist) - `VoxelClient`/`VoxelServer` each gained an identical,
  deterministic `find_dry_spawn_column` (a square-ring search outward
  for the nearest column at or above sea level). Same seed, same
  search, both sides agree without sending a coordinate over the wire.
- [x] 6 worldgen unit tests updated/added (incl. a new test spanning a
  deep column's water range across a chunk boundary), 1 quality-
  profile test updated (the old locked "must stay 1/0/3" assertion now
  asserts 1/-1/2 with updated reasoning).
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn
  column (-19,18) found for seed 1337, `"Player position: (-19.00,
  1.90, 18.00)"` - dry land), real `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical behavior
  at the new location), and a real two-process networked run where
  client and server independently compute the identical spawn column
  and the server-reconciled player position lands on dry land, zero
  warnings/errors.

`ctest` 393/393 (bgfx, up from 392) / 390/390 (non-bgfx, up from 389).

Honestly scoped: **what water actually looks like on a real
GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; water
renders as a solid-looking blue block, no transparency (see
DECISIONS.md); no waves/current/buoyancy/swimming physics; no beach/
sand transition at the shoreline; sky light still stops entirely at
water's surface (treated opaque like any other solid block - an
honest consequence of the existing binary light model, not a new
simplification invented for this phase).

## Phase 38 — Continental/mountain terrain

Two genuinely separate noise stages, matching brief section 21's own
pipeline naming for real, plus a real bug this same change surfaced
and fixed.

- [x] **Continental noise**: new low-frequency layer
  (`kContinentalNoiseScale`, ~666-block wavelength vs. the terrain
  layer's ~100-block one) - decides a column's base elevation
  (`kDeepOceanBase`..`kHighlandBase`) and how much amplitude the
  existing 4-octave detail noise gets to work with
  (`kMinMountainAmplitude`..`kMaxMountainAmplitude`). Coastal/oceanic
  columns stay flat regardless of what the detail layer samples;
  highland columns get real mountain-sized relief. A separate
  `kContinentalSeedOffset` keeps the two fields statistically
  independent.
- [x] **Terrain (detail) noise**: the original Phase 3 layer,
  frequency unchanged, now amplitude-scaled by continentalness instead
  of one fixed `kHeightVariation` everywhere.
- [x] **Found and fixed in the same phase**: `find_dry_spawn_column`'s
  search radius (64, sized for the old single-frequency noise) could
  now legitimately never leave one giant ocean basin - confirmed for
  real (seed 1337 needed radius 84, not implausible as originally
  assumed). Fixed by raising `kMaxRadius` to 1024 on both
  `VoxelClient`/`VoxelServer` and rewriting the ring search from
  O(ring-area) (re-scanning the full square, skipping most cells) to
  O(ring-perimeter) (only the new ring's boundary), keeping the worst
  case fast.
- [x] 1 worldgen test's height-range bounds widened (measured
  empirically via a real 20-seed sweep: [-15,20], set to [-20,30] for
  headroom); 1 new test (`LocalRoughnessVariesAcrossRegions`)
  confirming local terrain roughness now genuinely varies by region.
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn
  column (-84,-84) found for seed 1337 in 0.23s wall-clock, dry land,
  full sky light 5 blocks above), real `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs, and a real two-process
  networked run where client and server independently compute the
  identical spawn column and the server-reconciled position lands on
  dry land, zero warnings/errors.

`ctest` 394/394 (bgfx, up from 393) / 391/391 (non-bgfx, up from 390).

Honestly scoped: **what the new mountain/continental terrain shape
actually looks like on a real GPU/display is still NOT VERIFIED —
ENVIRONMENT LIMITATION**; no ridged-multifractal/erosion mountain
shaping (a deliberately scoped, honest simplification, not a shortcut
hiding a gap - see DECISIONS.md); still no climate/biome/caves/ores/
structures/vegetation stages (Phases 39-41).

## Phase 39 — Biomes

Real climate/biome pipeline stage - three genuine categories, not the
full Whittaker-table variety, and honestly scoped as such.

- [x] **`Biome` enum + `biome_at(seed, x, z)`**: a genuinely
  independent, low-frequency climate noise field (own
  `kClimateSeedOffset`, so biome boundaries don't visibly correlate
  with coastlines/ridge lines from the continental/terrain stages).
  Three categories - Snowy/Plains/Desert - split by threshold, Plains
  deliberately the widest band (50% vs. 25% each) since it was every
  column's only behavior before this phase.
- [x] **`BiomeBlocks` struct**: `generate_terrain_chunk`'s signature
  changed from flat surface/subsurface parameters to a per-biome block
  table - each biome maps to its own real surface/subsurface block
  ids. Stone and water stay biome-independent on purpose (deep stone
  everywhere; water doesn't vary by climate yet - no ice-cap variant).
- [x] Two new real blocks: `game:sand` (Desert's surface *and*
  subsurface - sandy all the way down) and `game:snow` (Snowy's
  surface only, dirt subsurface - a snow-capped tundra). Registered
  identically, same sequence position, on `VoxelClient`/`VoxelServer`.
- [x] New spawn-log biome name (`"...biome=Plains)..."`) - real,
  observable confirmation the climate stage ran and produced something
  concrete, not just a claim.
- [x] 2 new worldgen tests (`BiomeAtIsDeterministic`,
  `BiomeAtProducesAllThreeCategoriesOverARealArea` - a real sweep
  confirming all three bands genuinely occur); 5 existing tests
  rewritten to compute the expected surface/subsurface block from the
  actual biome at each test coordinate via `biome_at`, instead of
  assuming Plains.
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn
  column (-84,-84), `biome=Plains`, confirmed by breaking the spawn
  block and picking up `game:grass`), real `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical), and a
  real two-process networked run with matching independently-computed
  spawn columns, zero warnings/errors.

`ctest` 396/396 (bgfx, up from 394) / 393/393 (non-bgfx, up from 391).

Honestly scoped: **what sand/snow/biome transitions actually look
like on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
LIMITATION**; a temperature-only climate model, no humidity axis, no
Whittaker-diagram-style biome table (see DECISIONS.md for why this
scope, not more); no elevation-climate coupling (a highland column can
be Desert just as easily as a lowland one); no item mapping for sand/
snow yet (breaking either removes it without granting an item, the
same state grass/dirt were in before Phase 18/22); no caves/ores/
structures/vegetation stages yet (Phases 40-41).

## Phase 40 — Caves + ores

Real cave-carving and ore pipeline stages - a "noise crevice" tunnel
technique and two real, deliberately small ore types.

- [x] **New 3D noise primitives** (`hash3d`, `lattice_value3d`,
  trilinear `smooth_noise3d`, 4-octave `fractal_noise3d`) - every
  earlier worldgen stage only ever needed 2D column noise.
- [x] **`is_cave(seed, x, y, z, surface_height)`**: two independent 3D
  noise fields (own seed offsets); carves open air wherever their
  values land within a small threshold of each other - a real winding
  tunnel technique, not single-threshold "cheese cave" blobs (see
  DECISIONS.md). A real minimum depth below that column's own
  `terrain_height()` keeps tunnels from ever punching a hole at ground
  level.
- [x] **`OreType`/`ore_at(seed, x, y, z)`**: `None`/`Coal`/`Iron`, each
  ore its own noise field, absolute Y depth band, and rarity threshold.
  `None` overwhelmingly common by design; Iron checked first with a
  narrower/deeper band and a higher threshold, genuinely rarer than
  Coal. New `OreBlocks` struct (same pattern as `BiomeBlocks`).
- [x] Two new real blocks: `game:coal_ore`, `game:iron_ore` - solid,
  collidable, distinct colors only (no new mechanic). Registered
  identically, same sequence position, on `VoxelClient`/`VoxelServer`
  right after `game:water`.
- [x] **`generate_terrain_chunk` extended**: its stone-band branch now
  checks `is_cave` first (carved cells stay air), then `ore_at` for
  anything not carved (substituting the matching ore block), falling
  back to plain stone. New trailing `OreBlocks` parameter on every
  caller (`VoxelClient`, `VoxelServer`, `tools/benchmark`, worldgen
  tests).
- [x] **Ore thresholds tuned from real measured data**: the first
  round-number thresholds (0.90/0.95) proved nearly unreachable for
  Iron and too sparse for Coal once a real test tried to find them in a
  real scan volume - caught the same way Phase 38's spawn-radius bug
  and Phase 39's biome-threshold bug were, by measuring the actual
  system instead of assuming its shape. Fixed to 0.70/0.80 from a real
  measured `fractal_noise3d` output distribution (a standalone probe
  program, not another guess).
- [x] 6 new worldgen tests (`IsCaveIsDeterministic`,
  `IsCaveNeverFiresExactlyAtTheSurface`,
  `IsCaveProducesSomeCarvedCellsWellBelowTheSurface`,
  `OreAtIsDeterministic`, `OreAtProducesBothOreTypesOverARealVolume` - a
  real sweep confirming both ore types genuinely occur); 2 existing
  tests rewritten (one renamed `ChunkFarBelowTerrainIsStoneCaveOrOre`)
  since their old "always plain stone below the subsurface layer"
  assumption stopped holding once caves/ores could carve or substitute
  those cells - both now compute the expected block via the real
  `is_cave`/`ore_at` functions instead of a hardcoded constant.
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn
  column (-84,-84), `biome=Plains`, real shader program validity), real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs
  (byte-identical to Phase 39), and a real two-process networked run
  with matching independently-computed spawn columns, zero
  warnings/errors/rejects.

`ctest` 401/401 (bgfx, up from 396) / 398/398 (non-bgfx, up from 393).

Honestly scoped: **what carved caves/ore veins actually look like on a
real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; no
cave-specific lighting treatment (existing sky/block light propagation
reaches a carved tunnel however it naturally would, no dedicated
ambient occlusion or "always dark" cave handling); no ore item
drops/mapping yet (breaking coal/iron ore removes it without granting
an item, the same state sand/snow were in after Phase 39); no
structures/vegetation stages yet (Phase 41); caves/ores have no
artificial depth ceiling (an honest consequence of the noise fields
having no cutoff of their own, not a hidden gap - see DECISIONS.md).

## Phase 41 — Vegetation

Real vegetation pipeline stage - deliberately single-column tree/cactus
shapes, no cross-chunk canopy spread.

- [x] **`VegetationType`/`vegetation_at(seed, x, z, biome)`**: `None`/
  `Tree`/`Cactus`, own independent 2D noise field per type. Tree only
  for `Biome::Plains`, Cactus only for `Biome::Desert` - `Biome::Snowy`
  stays vegetation-free on purpose. New `VegetationBlocks` struct
  (`wood`/`leaves`/`cactus`, same pattern as `BiomeBlocks`/`OreBlocks`).
- [x] **Deliberately single-column shapes**: a tree is a 4-block `wood`
  trunk capped by a 3-block `leaves` pillar directly above it; a cactus
  is a 3-block `cactus` stack, no canopy. No wide canopy spreading into
  neighboring columns - a real, deliberate scope choice (see
  DECISIONS.md for why this is achievable-but-deferred, not
  infeasible), with a real side effect: placement also works correctly
  across vertically-stacked chunk boundaries with zero special-casing,
  the same way Phase 37's sea-level water fill already does.
- [x] Three new real blocks: `game:wood`, `game:leaves`, `game:cactus` -
  solid, collidable, distinct colors only. Registered identically, same
  sequence position, on `VoxelClient`/`VoxelServer` right after
  `game:iron_ore`.
- [x] **`generate_terrain_chunk` extended**: its above-terrain branch
  now places a dry column's own vegetation above the surface block, air
  everywhere else - checked once per column. New trailing
  `VegetationBlocks` parameter on every caller (`VoxelClient`,
  `VoxelServer`, `tools/benchmark`, worldgen tests).
- [x] **Thresholds measured from the start**: `kTreeThreshold`/
  `kCactusThreshold` picked from `fractal_noise`'s own real,
  empirically-measured output range (a standalone probe program) before
  ever running the tests, applying Phase 40's "measure, don't guess"
  lesson proactively instead of needing another failure to arrive at
  it - both worked on the first real test run (trees ~4.5% of Plains
  columns, cacti ~2.5% of Desert columns).
- [x] 7 new worldgen tests (`VegetationAtIsDeterministic`,
  `VegetationAtNeverReturnsTreeOrCactusForSnowy`,
  `VegetationAtNeverReturnsCactusForPlainsOrTreeForDesert`,
  `VegetationAtProducesBothTreeAndCactusOverARealArea` - a real sweep
  confirming both genuinely occur, and
  `GenerateTerrainChunkPlacesARealTreeWhereVegetationAtSaysOneGrows` - a
  real end-to-end check finding an actual dry Plains column with a
  Tree, generating its chunk, and confirming the real trunk/canopy
  blocks appear); 1 existing test
  (`GenerateTerrainChunkMatchesTerrainHeightColumnByColumn`) rewritten
  since its old "always air above terrain except water" assumption
  stopped holding once vegetation could place blocks there.
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn
  column (-84,-84), `biome=Plains`, real shader program validity), real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs
  (byte-identical to Phase 40), and a real two-process networked run
  with matching independently-computed spawn columns, zero
  warnings/errors/rejects.

`ctest` 406/406 (bgfx, up from 401) / 403/403 (non-bgfx, up from 398).

Honestly scoped: **what a real tree/cactus actually looks like on a
real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; no
wide/spreading tree canopies (a real, documented scope choice, not a
hidden gap - see DECISIONS.md); no varied tree/cactus silhouettes (one
shape per type, not the full variety a shipped game would eventually
want); no wood/leaves/cactus item drops/mapping yet (breaking any of
them removes it without granting an item, the same state coal/iron ore
were in after Phase 40); no structures pipeline stage (out of scope
for this 42-phase plan entirely, not a gap deferred from this phase).

## Phase 42 — Documentation update

Final pass across the governing directive's own listed doc set
(`BUILDING.md`/`PROJECT_STATE.md`/`CHANGELOG.md`/`DECISIONS.md`/
`README.md`) closing out the 18-phase (Phases 25-42) program.

- [x] **`README.md` written** - was previously an empty file. A real
  project-level entry point: what the project is, an honest current
  status pointer (built/verified in a headless Linux sandbox, no
  GPU/display - docs say so explicitly, never silently claim more), a
  feature summary reflecting the actual Phase 41 state, a quick-start
  build/run block, and a documentation map.
- [x] **`PROJECT_STATE.md`'s "Known Limitations" corrected**: a stale
  Phase 17 entry still read "no climate/biome/caves/ores/structures/
  vegetation/decoration" - updated in place (struck through, matching
  this file's own established correction convention) to reflect that
  Phases 39-41 made biome/caves/ores/vegetation all real, and only
  structures remains unimplemented (out of scope for this plan
  entirely, not deferred from any specific phase).
- [x] `BUILDING.md`/`CHANGELOG.md`/`DECISIONS.md` reviewed against the
  current state and found already current - kept up to date
  phase-by-phase throughout Phases 35-41, no further edits needed.

## Phase 43 — Input overhaul + mouse look + Minecraft defaults

First phase of a second user-directed program (Phases 43-46: controls,
a 2D UI framework, persistent options, a menu/options/controls screen).
Real rebindable keymap, real mouse look/click/wheel, and real
Minecraft-parity default fixes.

- [x] **`engine/platform::KeyBindings`** (new): each `Action` maps to
  up to 2 physical keys, a unified `PhysicalKey` space covering both
  SDL scancodes and 3 mouse buttons. Starts from real Minecraft-parity
  defaults, rebindable in place (`bind()`/`reset_to_defaults()`) - the
  real data structure Phase 46's controls menu will read/write, not a
  stub. `physical_key_name`/`parse_physical_key` round-trip via SDL's
  own scancode names (not a hand-rolled table - see DECISIONS.md).
- [x] **`KeyboardInputBackend` renamed `DesktopInputBackend`**,
  rewritten to poll every `Action` through `KeyBindings` (mouse
  buttons included) instead of a fixed table baked into input.cpp -
  `Interact`/`PlaceBlock` are real mouse clicks now, zero changes
  needed to the existing break/place logic in client/main.cpp.
- [x] **Real mouse-look**: `InputState::mouse_delta_x/y`
  (`SDL_GetRelativeMouseState`, polled once per frame) applied to
  `FirstPersonCamera::add_yaw_pitch` additively alongside the existing
  arrow-key look fallback, not replacing it.
- [x] **Real mouse-click actions**: new `Action::PickBlock`
  (middle-click, Minecraft's "pick block" - selects the matching
  hotbar entry for the looked-at block without granting it for free).
- [x] **Real mouse-wheel hotbar cycling**: new `Action::
  CycleHotbarPrev` (scroll down), driven by `Window::
  consume_wheel_delta_y()` accumulating real `SDL_EVENT_MOUSE_WHEEL`
  events per frame into a one-frame action pulse.
- [x] **Direct hotbar selection**: 9 new `Action::SelectHotbar1..9`
  bound to the number row - a silent no-op past the real hotbar's
  current size (4 items), not a crash or wraparound.
- [x] **Real mouse-capture management**: `Window::
  set_relative_mouse_mode`/`consume_focus_lost` (new); ESC/Tab (new
  `Action::Escape`, still a real KeyBindings entry - see DECISIONS.md
  for why it isn't a hardcoded SDL check) releases capture, a click
  while free re-captures it without that click also breaking/placing a
  block (`suppress_click_for_recapture`), losing window focus releases
  it automatically.
- [x] **Real Minecraft-parity default fix**: this project's own
  pre-Phase-43 defaults had Sprint=Shift/Crouch=Ctrl backwards -
  corrected to Sprint=Ctrl/Crouch=Shift.
- [x] **Hygiene fixes** (from a real first macOS run's own bug
  reports): shader load path now resolves via `Window::
  executable_base_path()` (`SDL_GetBasePath`) instead of the current
  working directory - verified via real runs from the repo root and
  from `/tmp`; rendering-only constants gated behind
  `LCU_ENABLE_BGFX`; a real redundant `Lcu::Math` link entry removed
  from `game/CMakeLists.txt`.
- [x] 20 new unit tests (`KeyBindings`/`PhysicalKey` defaults,
  rebinding, reset, name round-trip; `InputState` mouse-delta).
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (from
  multiple working directories), real `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical to Phase
  41), and a real two-process networked run with matching
  independently-computed spawn columns, zero warnings/errors/rejects.

`ctest` 420/420 (bgfx, up from 406) / 417/417 (non-bgfx, up from 403).

Honestly scoped: **real mouse-look/click/wheel/capture behavior
against an actual mouse device and display is still NOT VERIFIED —
ENVIRONMENT LIMITATION** (no real mouse in this sandbox); the exact
unused-variable/duplicate-library linker warnings these fixes target
were never reproduced here either (fixed on code-reading grounds); no
2D UI/menu yet to rebind a key through (Phase 44/46); `Action::
Inventory`/`SwapOffhand`/`Escape`'s pause-menu half and most
`SelectHotbar5-9` slots still have no consumer.

## Phase 44 — 2D UI framework

Real 2D UI quad batch, the rendering foundation Phase 46+'s pause
menu/options/controls screens and a future HUD/inventory will build on.

- [x] **`Renderer::submit_ui_quad`/`flush_ui_quads`** (new): queue
  screen-space rectangles (pixel position/size, RGBA color, UV 0..1)
  across a frame, upload/draw them all in exactly one real
  `bgfx::submit()` via transient buffers - the same idiom
  `submit_billboard`/`submit_wireframe_box` already use for other
  per-frame geometry, not a persistent GPU resource.
- [x] **New `Mat4::orthographic`** (real unit tests: screen corners map
  to clip-space corners, center maps to the clip-space origin).
- [x] **New `kUi2dViewId` bgfx view**, own `vs_ui2d.sc`/`fs_ui2d.sc`
  shader pair (position + UV + color, no lighting concept - same
  minimal approach `vs_sky.sc`/`fs_sky.sc` already established).
- [x] **Real, documented deviation from this phase's own literal view
  order** ("after sky, before terrain"): submitted *last* instead
  (after terrain) - the literal order would make the UI invisible
  behind any solid geometry, since bgfx composites views in submission
  order. See DECISIONS.md for the full reasoning.
- [x] **Real first consumer**: a permanent, screen-centered crosshair
  (two thin bars) - doubles as this phase's own "Test-Rechteck in
  Bildschirmmitte sichtbar" verification, not a separate throwaway
  test element.
- [ ] **`ItemDefinition::icon_color`/item-icon pattern rendering** -
  PARTIAL, deferred: no inventory/hotbar widget exists yet to consume
  it, and this project's own `ItemDefinition` doc comment already
  argues against adding fields speculatively (see DECISIONS.md). The
  vertex format already carries real UV data ready for this once a
  real consumer exists.
- [x] 7 new unit tests (5 `QuadBatch2D`: batch accumulation, flush
  clears it, empty-flush safety; 2 `Mat4::orthographic`).
- [x] Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (`UI2D
  shader program valid=true`), real `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical to Phase
  43), and a real two-process networked run with matching
  independently-computed spawn columns, zero warnings/errors/rejects.

`ctest` 427/427 (bgfx, up from 420) / 419/419 (non-bgfx, up from 417).

Honestly scoped: **what the crosshair/any UI quad actually looks like
on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
LIMITATION** (headless Noop backend proves the pipeline runs
end-to-end, not that it looks right); no item-icon rendering yet
(deferred, see above); no slot backgrounds/health/hunger/menu
backgrounds yet (real future consumers of this same batch API, Phase
46+).

---

## Phase 45 — Persistent options

Real config persistence: the storage layer Phase 46's options/controls
screens will read from and write to.

- [x] **New `engine/platform::Options`** (`options.{h,cpp}`):
  `mouse_sensitivity`/`fov`/`hud_enabled`/`debug_overlay_enabled` plus
  a full `KeyBindings` instance, `load(path)`/`save(path)`, real
  `key=value` text format (`# comments`, blank lines skipped).
- [x] **Real per-OS storage location**: `Options::default_path()` uses
  `SDL_GetPrefPath("LiveCraftUltimate", "LiveCraftUltimate")`, not a
  hand-picked path - verified on this Linux sandbox at
  `~/.local/share/LiveCraftUltimate/LiveCraftUltimate/options.txt`.
- [x] **New bidirectional `action_name`/`parse_action_name` table** (29
  entries): every `Action` persists as a human-readable name
  (`key.move_forward=W`), not a raw enum index that would silently
  break on any future enum reordering (see DECISIONS.md).
- [x] **Real tolerance, not just a happy path**: a missing file returns
  `false` and keeps every default untouched; a corrupt or unrecognized
  line (bad number, unknown action or key name) is skipped and every
  other real line still loads - verified against a real file with
  deliberately interleaved garbage lines between real ones.
- [x] **`VoxelClient` wired to actually use it**: the former
  `kMouseSensitivity` constant is gone, replaced by
  `options.mouse_sensitivity`; the Phase 44 crosshair is now gated
  behind `options.hud_enabled`; the debug overlay is now gated behind
  `options.debug_overlay_enabled`, defaulting `false` - a real behavior
  change from Phase 44's always-on overlay. Options load at startup,
  save on exit.
- [ ] **`options.fov` applied to the camera's projection** - PARTIAL,
  deferred: the field is persisted and round-trips, but nothing in
  `client/main.cpp` currently reads it for rendering. Wiring it in with
  no menu (Phase 46) to actually change it in-game would be
  speculative and unverifiable, so it stays honestly unused until a
  real consumer exists.
- [x] 9 new unit tests (7 `Options`: defaults, missing-file behavior,
  scalar round-trip, keybinding round-trip, corrupt-line tolerance,
  unrecognized-name tolerance, conditional `.alt` line; 2 `ActionName`:
  every real `Action` round-trips, an unknown name fails to parse).
- [x] Verified via real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
  `LCU_VERIFY_CRAFT` runs (byte-identical to Phase 44, plus the new
  load/save log lines), a real `LCU_BUILD_SHADER_TOOLS=ON` run
  (`Chunk`/`Sky`/`UI2D` shader programs all still `valid=true`), a real
  two-process networked run (zero warnings/errors/rejects, matching
  spawn columns), and direct inspection of the real written
  `options.txt` confirming every field, including all 29 keybindings,
  round-trips as human-readable text at the real OS path.

`ctest` 436/436 (bgfx, up from 427) / 428/428 (non-bgfx, up from 419).

Honestly scoped: no options menu UI exists yet to change these values
in-game (Phase 46 - this phase is the storage layer only, per its own
spec); `options.fov` is persisted but not yet applied to the camera
projection (see above).

---

## Phase 46 — Menu framework: pause/options/controls

Real stacked-screen menu (the UI Phase 46's own directive asked for)
built on top of Phase 44's 2D UI quads and Phase 45's persisted
`Options` - the first phase this program that actually lets a player
change something in-game rather than only via a hand-edited
`options.txt`.

- [x] **New `engine/ui::MenuStack`** (`menu_stack.{h,cpp}`): stacked
  `MenuScreen`s, each with `MenuItem` rows carrying real `on_activate`/
  `on_adjust` callbacks. Pure logic, zero SDL/bgfx dependency - `engine/
  ui` now builds under `LCU_BUILD_CLIENT` unconditionally (not only
  `LCU_ENABLE_BGFX`), so this is tested in both the bgfx and non-bgfx
  configs. `menu_item_layout`/`menu_item_at_point` give real,
  hit-testable pixel rects computed from screen size alone.
- [x] **Real pause menu**: ESC opens Pause (Zurueck zum Spiel/Optionen/
  Steuerung/Beenden) instead of only releasing mouse capture. A
  non-empty `menu_stack` genuinely pauses movement/physics/AI/day-night
  - verified via a real headless run: player position provably frozen
  while paused, provably moving again once resumed.
- [x] **Real, documented deviation**: network *receive* keeps running
  while paused (only this client's own outgoing input pauses) - halting
  it fully risked the connection reading as dead by the time the player
  unpauses. See DECISIONS.md.
- [x] **Real Options screen**: Maus-Empfindlichkeit/Sichtfeld(FOV) as
  +/- rows (via the existing `LookLeft`/`LookRight` actions), HUD/
  Debug-Overlay toggles, Zurueck (saves on leaving). **FOV is now
  genuinely wired into the camera's projection matrix** - closes the
  gap Phase 45 deliberately left open.
- [ ] **Renderdistanz** - PARTIAL, deferred: `load_settings.radius_xz`
  is `const` and live re-streaming/unloading on a change is a real,
  separate structural change this phase's own directive explicitly
  allows deferring rather than shipping a +/- row that would visibly do
  nothing. See DECISIONS.md.
- [x] **Real Controls screen**: every rebindable `Action` listed as
  `<name>: <key>` (Escape/MenuConfirm excluded - not rebindable);
  Enter/click enters a real "waiting for input" capture (new
  `lcu::platform::poll_any_pressed_key`/`is_escape_key`); ESC cancels;
  a release-then-press debounce (`rebind_ready`) stops the activating
  key from immediately binding itself; Reset restores every default;
  changes save on leaving.
- [x] **New `Action::MenuConfirm`** (Enter) - a real menu-meta action
  alongside `Action::Escape`.
- [x] **Real use-after-free found and fixed**: popping/pushing
  `menu_stack` directly from inside a currently-executing `MenuItem`
  callback destroys (or, for push, potentially reallocates) that same
  callback's own storage mid-execution - reproduced as a real segfault
  via headless testing with the new `LCU_VERIFY_MENU` hook, fixed by
  deferring every such mutation through `pending_menu_action`,
  processed once per frame after `activate_selected()`/
  `adjust_selected()` have fully returned. See DECISIONS.md.
- [x] New `LCU_VERIFY_MENU` headless hook: pause/resume movement
  gating, real navigation via edge-detected `LookDown`/`MenuConfirm`, a
  real two-step sensitivity adjustment confirmed by inspecting the
  saved `options.txt` (`0.0022` -> `0.0026`).
- [x] 21 new unit tests (`MenuStack` navigation/callbacks,
  `MenuItemLayout`/`MenuItemAtPoint` real rect math).
- [x] Verified via the real `LCU_VERIFY_MENU` run, real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs
  (byte-identical to Phase 45), a real `LCU_BUILD_SHADER_TOOLS=ON` run
  (`Chunk`/`Sky`/`UI2D` shader programs all still `valid=true`), and a
  real two-process networked run (zero warnings/errors/rejects,
  matching spawn columns).

`ctest` 455/455 (bgfx, up from 436) / 447/447 (non-bgfx, up from 428).

Honestly scoped: the menu's real on-screen appearance is still **NOT
VERIFIED — ENVIRONMENT LIMITATION** (headless Noop backend proves the
pipeline runs, not that it looks right); the controls screen's rebind
capture is real, reviewed code but **NOT VERIFIED against a real
keyboard/mouse** (`poll_any_pressed_key` reads real SDL hardware state
the dummy input driver never produces); Renderdistanz deferred (see
above); no chat, no multiplayer UI, no advancements (out of scope per
this phase's own directive).

---

## Phase 47 — HUD overhaul: hotbar + health/hunger bars + F-toggles

Real visible gameplay HUD - crosshair (Phase 44), now a real hotbar,
health/hunger bars, and the F-key toggles Phase 43's own "verbindlich"
table already reserved bindings for.

- [x] **New `engine/ui::hud.{h,cpp}`**: pure layout math
  (`hotbar_slot_layout`/`stat_bar_layout`), zero SDL/bgfx dependency -
  tested in both the bgfx and non-bgfx configs.
- [x] **Real Minecraft-position hotbar**: 9 bottom-center slots,
  bordered/filled quads (brighter border on the selected slot), flat
  colored icon quads for the 4 real `placeable_items`, real held-count
  labels via debug text.
- [x] **New `ItemDefinition::icon_color`** - closes the exact gap
  Phase 44 deferred ("no inventory/hotbar widget exists yet to consume
  it"); set per item to match its own block's tint where one exists.
- [x] **Real health/hunger bars**: 10-icon Minecraft-style bars, real
  half-icon fill math, positioned above the hotbar, left-aligned to its
  own left edge. Hardcoded full this phase (Phase 51 wires real values
  in) - the layout/rendering itself is real, not a placeholder.
- [x] **5 new F-key `Action`s**: `ToggleHud`/`ToggleDebugOverlay`/
  `Screenshot`/`TogglePerspective`/`Fullscreen`, bound to F1/F3/F2/F5/
  F11. `ToggleHud`/`ToggleDebugOverlay` flip the same real persisted
  `options.hud_enabled`/`debug_overlay_enabled` the options menu
  already reads/writes.
- [x] **New `Renderer::request_screenshot`** (`bgfx::requestScreenShot`
  against the default backbuffer) and **`Window::set_fullscreen`**
  (`SDL_SetWindowFullscreen`).
- [x] **Real third-person-behind camera**: only the render eye shifts
  back along the real look direction - raycast/movement/`camera.
  position` are untouched.
- [ ] **Third-person-front** - PARTIAL, deferred: no player model
  exists anywhere in this codebase to render in front of the camera, so
  this mode is honestly not implemented rather than shipped as an empty
  no-op. See DECISIONS.md.
- [x] **Real shared debug-text-buffer ownership fix**: `draw_debug_
  overlay`/`draw_menu_labels` no longer clear the buffer themselves -
  `client/main.cpp` now owns the one real `clear_debug_text()` call per
  frame, in a real deliberate order (overlay -> HUD -> menu), fixing a
  real bug where whichever of the three ran first would have had its
  text wiped by the next.
- [x] New `LCU_VERIFY_HUD` headless hook: each F-key pressed on its own
  frame, real log output confirms each real resulting state.
- [x] 21 new unit tests (`HotbarSlotLayout`/`StatBarLayout` real rect
  math).
- [x] Verified via the real `LCU_VERIFY_HUD` run, real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT`/
  `LCU_VERIFY_MENU` runs (byte-identical to Phase 46), a real
  `LCU_BUILD_SHADER_TOOLS=ON` run (`Chunk`/`Sky`/`UI2D` shader programs
  all still `valid=true`), and a real two-process networked run (zero
  warnings/errors/rejects, matching spawn columns).

`ctest` 466/466 (bgfx, up from 455) / 458/458 (non-bgfx, up from 447).

Honestly scoped: the HUD's real on-screen appearance is still **NOT
VERIFIED — ENVIRONMENT LIMITATION** (headless Noop backend proves the
pipeline runs, not that it looks right); `bgfx::requestScreenShot`'s
real output can't be inspected under the headless `Noop` backend (no
real framebuffer content); third-person-front deferred (see above);
hotbar slots 5-9 still show nothing (only 4 real placeable items exist,
unchanged since Phase 43).

---

## Phase 48 — Block highlight + break progress + hand

Real visible feedback for the game's most-repeated action - looking at
and breaking a block - plus the real hold-to-break mechanic Minecraft
players expect instead of an instant click.

- [x] **Real block highlight**: black wireframe box on the raycast-
  targeted block (`Renderer::submit_wireframe_box`, a new consumer of
  the real Phase 36 call).
- [x] **Real hold-to-break**: `BlockDefinition::hardness` (real
  per-block seconds - stone 2.0, wood 1.5, dirt 0.5, leaves 0.2, grass
  0.6, sand 0.5, snow 0.1, cactus 0.4, ores 3.0, torch 0.0 = instant)
  gates how long Interact must be held against the *same* targeted
  block. New pure `lcu::voxel::break_progress_fraction`/`is_break_ready`
  (`break_progress.{h,cpp}`) - one real source both the break trigger
  and the darkening overlay read. Switching targets/releasing resets
  progress; a real one-shot latch stops a held click from re-sending a
  networked break request every frame while awaiting the server's own
  `BlockChange`.
- [x] **Real break-progress overlay**: a solid box (new `Renderer::
  submit_solid_box`, reuses the existing sky shader - no new shader
  files) darkens toward black as progress advances.
- [ ] **Real crack-noise-density shader effect on the block's own
  face** - PARTIAL, deferred: needs a new per-fragment world-position
  uniform threaded through `fs_chunk.sc`, a real, separate shader
  feature outside this phase's scope. The solid-box overlay above is a
  real, visible, honestly-scoped substitute, not a placeholder. See
  DECISIONS.md.
- [x] **Real hand icon**: the selected placeable item's own real
  `icon_color` (Phase 47), bottom-right corner, real elapsed-time
  sine-ease swing on every break/place.
- [x] **Water stays genuinely unbreakable with zero special-case
  code**: `has_collision=false` (Phase 37) already keeps the raycast
  from ever targeting it.
- [x] **Real headless-hook rewrite**: `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` rewritten from single-frame
  instant-break pulses to real elapsed-time hold windows sized to each
  target's own hardness - a real, necessary consequence of the
  mechanic change, not a regression.
- [x] **A real networked timing bug found and fixed during
  verification**: `LCU_VERIFY_CRAFT`'s networked run failed with its
  first chosen 0.2s inter-break gap (the second break's raycast could
  still see the stale grass block before `BlockChange` landed) -
  confirmed by an actual failed run, then fixed by widening to a real
  0.8s. See DECISIONS.md.
- [x] 9 new unit tests (`BreakProgress`/`IsBreakReady`).
- [x] Verified via all four rewritten/regression hooks (full pipelines
  confirmed real end-to-end: break -> pick up -> cycle -> place; two
  sequential breaks -> craft match -> craft reject), a real two-process
  networked `LCU_VERIFY_CRAFT` run (zero warnings/errors/rejects with
  the widened gap), real `LCU_VERIFY_MENU`/`LCU_VERIFY_HUD` regression
  runs (unaffected), and a real `LCU_BUILD_SHADER_TOOLS=ON` run
  (`Chunk`/`Sky`/`UI2D` shader programs all still `valid=true`).

`ctest` 475/475 (bgfx, up from 466) / 467/467 (non-bgfx, up from 458).

Honestly scoped: what the highlight/overlay/hand icon actually look
like on a real GPU/display is still **NOT VERIFIED — ENVIRONMENT
LIMITATION**; no real crack-noise-density shader effect (deferred, see
above); the break-progress overlay is opaque, not alpha-blended, so it
appears at whatever darkness the first held frame computes rather than
fading in from invisible (a real, documented rough edge).

## Phase 49 — Inventory screen + drag/drop + crafting grid

The visible gameplay elements Minecraft players expect in their first
minutes: a real inventory screen, drag/drop, and a working crafting
grid - built on a genuinely real hotbar/inventory integration rather
than layering a UI on top of Phase 21's old virtual item-type selector.

- [x] **Real, deep hotbar integration**: `placeable_items`/
  `selected_placeable_index` removed entirely. `selected_hotbar_slot` is
  now a real index (0-8) into 9 of `player_inventory`'s own 36 slots;
  new `BlockItemMapping::block_for_item` reverse lookup drives placement
  from whatever's actually held there. Any block/item pair registered
  via `register_pair` is automatically placeable with zero
  hotbar-specific wiring.
- [x] **Real inventory screen** (`E`): 2x2 craft grid + result slot, 3x9
  main storage, the hotbar again at the bottom. New pure
  `engine/ui::inventory_screen.{h,cpp}` (layout/hit-testing) +
  `inventory_screen_renderer.{h,cpp}` (drawing), mirroring `hud.h`'s own
  split. Does **not** pause the simulation - only player control locks
  (new `!inventory_open` gate, independent of `!paused`); `ESC` closes
  the screen instead of the pause menu when it's open.
- [x] **Real drag/drop**: new `engine/items::inventory_ops.{h,cpp}` -
  `inventory_left_click`/`inventory_right_click`/`inventory_shift_click`
  (pure logic) + new `Inventory::add_item_to_range`, dispatched from
  real mouse clicks (Interact=left, PlaceBlock=right, Crouch=shift
  modifier).
- [x] **Real crafting-grid integration**: the 2x2 grid is a genuine
  `RecipeRegistry::find_match(grid, 2, 2)` query against a separate
  5-slot craft-grid `Inventory`, recomputed on every input change.
  Taking the result consumes 1 of each non-empty ingredient slot and
  grants the crafted stack to the cursor. Phase 23's quick-craft stays
  as a convenience path.
- [x] **New real recipe**: `game:wood` finally has an item (the block
  existed since Phase 41 with no item - a real, now-closed gap), plus
  `game:planks` (crafted-only) and `1 wood -> 4 planks` (shapeless) -
  the grid's first real reachable recipe.
- [x] **New `Window::warp_mouse`** (`SDL_WarpMouseInWindow`) - the first
  real mouse-position-driven headless verification here.
- [x] **New `LCU_VERIFY_INVENTORY` hook**: grants 1 wood, opens the
  screen, then drives 5 real clicks via `warp_mouse` + synthesized
  Interact/Crouch presses (pick up wood -> drop in craft grid -> take
  result -> place in main inventory -> shift-click back to hotbar) ->
  close. A real ordering bug found and fixed during verification: its
  first draft's `input.set_down` calls sat after the E-toggle/click
  code that reads them that frame, so the inventory silently never
  opened - fixed by moving the hook next to `LCU_VERIFY_MENU`/
  `LCU_VERIFY_HUD`. See DECISIONS.md.
- [x] `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH` updated for the real
  slot-based hotbar (both now press `CycleHotbar` *and*
  `CycleHotbarPrev` - a real net-zero round trip - to land back on the
  right slot before placing). `LCU_VERIFY_CRAFT` needed no changes.
- [x] 44 new unit tests (30 drag/drop, 2 `Inventory::add_item_to_range`,
  8 inventory-screen layout/hit-testing, 3 `BlockItemMapping`
  reverse-lookup regression).
- [x] Verified via real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
  `LCU_VERIFY_CRAFT` regression runs, a real `LCU_VERIFY_INVENTORY` run
  in both bgfx and non-bgfx builds (full pipeline confirmed), real
  `LCU_VERIFY_MENU`/`LCU_VERIFY_HUD` regression runs, a real
  two-process networked `LCU_VERIFY_BREAK_PLACE` run, and a real
  `LCU_BUILD_SHADER_TOOLS=ON` build.

`ctest` 506/506 (bgfx, up from 475) / 498/498 (non-bgfx, up from 467).

Honestly scoped: what the inventory screen actually looks like on a
real GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**;
shift-clicking a craft-grid slot lands anywhere in the whole inventory
rather than hotbar-first (a real, minor simplification); a recipe
needing >1 of the same ingredient in one cell isn't correctly consumed
by the result-click logic (documented, no registered recipe needs it
yet); still no icon/texture atlas.

## Phase 50 — Item entities + crafting table

Real physical dropped items (closing Phase 17's own long-standing
"pickup goes straight to inventory" gap) and a real crafting table with
its own 3x3 grid screen.

- [x] **Real item entities**: breaking a block spawns a real
  `game::components::ItemEntity` + `Position` on the shared
  `entity_registry` with a real small upward toss, instead of a direct
  inventory grant. `game::systems::update_item_entities` applies real
  gravity/ground collision every frame by reusing `lcu::physics::
  apply_gravity`/`move_and_collide` - the same primitives the player's
  own controller already uses. Real per-frame Y-axis spin (visual
  only), real 5-minute despawn.
- [x] **Real pickup**: `pickup_item_entities` adds to the inventory once
  the player's own (inflated) AABB overlaps the entity and its 0.5s
  pickup delay has elapsed; a partial fit keeps the entity alive with
  its count reduced to the real leftover.
- [x] **New `Renderer::submit_world_billboard`**: a real depth-tested
  camera-facing quad in the terrain view (unlike `submit_billboard`'s
  own sky view, which has no depth test) - reuses the existing sky
  shader, zero new shader files. Item entities spin around world-Y,
  tinted their own item's real `icon_color`.
- [x] **Real crafting table** (`game:crafting_table`): right-click opens
  a real workbench screen - a 3x3 grid + result plus the same main
  storage/hotbar rows the regular inventory screen shows (a grid-only
  screen would have no way to move items into it - see DECISIONS.md).
  New pure `engine/ui::crafting_table_screen.{h,cpp}` +
  `crafting_table_screen_renderer.{h,cpp}`, built from
  `inventory_screen.h`'s own shared building blocks. Real click
  dispatch reuses the exact same `inventory_left_click`/`right_click`/
  `shift_click` functions Phase 49 already built. Breaking a crafting
  table drops itself.
- [x] **Server parity extended**: `game:wood`/`game:crafting_table`
  items now registered on `VoxelServer` too (Phase 49 only added wood
  client-side - a real, now-closed gap), `tracked_items` grown from 4
  to 6 entries.
- [x] **A real item-entity pickup-range bug found and fixed twice
  during this phase's own headless verification**: an exact player-
  AABB overlap almost never triggers in practice (a dropped item with
  no horizontal velocity can settle one or two blocks below the
  player's own standing height) - a real 0.75 inflate missed a
  single-block-deep item by 0.005 (`LCU_VERIFY_BREAK_PLACE` failed for
  real), 1.0 still missed a two-blocks-deep item
  (`LCU_VERIFY_CRAFT` failed for real) - fixed at a real 2.0 with
  margin, both confirmed via a real re-run. See DECISIONS.md.
- [x] **New `LCU_VERIFY_WORKBENCH` hook**: grants wood, directly seeds
  a real `game:crafting_table` block at the established spawn-look
  target, right-clicks it open, picks up the wood, drops it anywhere in
  the real 3x3 grid (proving the shapeless "1 wood -> 4 planks" recipe
  works in the bigger grid too), takes the result, closes via Escape.
- [x] 17 new unit tests (9 `UpdateItemEntities`/`PickupItemEntities`, 8
  `CraftingTableScreenLayoutTest`/`HitTestCraftingTableScreen`/
  `CraftingTableScreenConstants`).
- [x] Verified via a real `LCU_VERIFY_WORKBENCH` run (both bgfx and
  non-bgfx builds), real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
  `LCU_VERIFY_CRAFT`/`LCU_VERIFY_INVENTORY`/`LCU_VERIFY_MENU`/
  `LCU_VERIFY_HUD` regression runs, a real two-process networked
  `LCU_VERIFY_CRAFT` run, and a real `LCU_BUILD_SHADER_TOOLS=ON` build.

`ctest` 523/523 (bgfx, up from 506) / 515/515 (non-bgfx, up from 498).

Honestly scoped: what a dropped item or the workbench screen actually
look like on a real GPU/display is still **NOT VERIFIED — ENVIRONMENT
LIMITATION**; item entities have no horizontal scatter on spawn
(vertical toss only); the workbench's own shift-click lands anywhere in
the whole inventory rather than hotbar-first; a recipe needing >1 of
the same ingredient in one cell still isn't correctly consumed by
either result-click.

## Phase 51 — Health, hunger, fall damage, respawn

Real player vitals, closing Phase 47's own "hardcoded full this phase"
HUD gap.

- [x] **Real health/hunger**: new `game::components::PlayerHealth`/
  `PlayerHunger` (plain structs, not ECS components - matches
  `PlayerPhysicsState`'s own placement, see DECISIONS.md) and
  `game::systems::player_vitals_system.{h,cpp}` (pure logic): real
  Minecraft-shaped fall damage (`fallDistance` accumulates only while
  airborne and descending; damage applied/reset only on the real
  landing-frame transition, so a normal jump deals zero), natural regen
  (+1 HP/4s at hunger>=18), starvation (-1 HP/4s at hunger==0), hunger
  drain (-1/30s, 2x sprinting-and-moving), per-jump hunger cost, eating.
- [x] **Wired into `client/main.cpp`**: `previous_player_y` captured
  before gravity/collision resolve each frame, fed to
  `update_fall_tracking` after (works for both single-player and
  networked physics paths); jump hunger cost applied on a real
  edge-detected fresh Jump press, read *before* `try_jump` changes
  `player.grounded`; hunger drain/regen/starvation tick every frame
  gated on `paused` alone (keeps running while the inventory/workbench
  screen is open, matching real Minecraft). Real HUD wiring:
  `hud_state.health`/`hunger` now read from `player_health`/
  `player_hunger` instead of `HudState`'s own 20/20 defaults.
- [x] **Real eating**: new `game:apple` (+4 hunger)/`game:bread`
  (+5 hunger) items. Right-click dispatch is now a real three-way
  branch: crafting-table intercept (checked first) -> eating (new -
  doesn't require a raycast hit) -> normal placement. Neither food item
  has a survival obtain path yet (no farming/mob drops, out of scope) -
  `LCU_VERIFY_HEALTH` grants one directly.
- [x] **Real death + respawn**: a death transition drops the *entire*
  36-slot inventory as real item entities at the player's position
  (reusing Phase 50's `ItemEntity` pipeline wholesale), closes any open
  inventory/workbench screen, and pushes a "Du bist gestorben"
  `MenuScreen` (reusing Phase 46's `MenuStack`) with a Respawn row that
  resets position/health/hunger/every accumulator.
- [x] **New `LCU_VERIFY_HEALTH` hook**: teleports the player 10 blocks
  above spawn with `grounded=false` (same direct-state-seed honesty
  `LCU_VERIFY_WORKBENCH`'s own block seed uses), seeds hunger below
  max, grants an apple, simulates a real right-click once the fall has
  landed. Real run: `Fall damage: 6.9 (health: 13.1/20.0)` then
  `Ate game:apple (hunger: 14.0/20.0)`. Death+respawn separately
  confirmed via a real run with a temporarily-lethal fall height.
- [x] **A real, confirmed single-player-only gap found this phase**: in
  networked mode the synthetic teleport is invisible to `VoxelServer`'s
  own authoritative simulation, so the next `PlayerCorrection` snaps
  the client back down before a real fall distance can accumulate -
  confirmed via an actual two-process run (eating still verifies; fall
  damage doesn't). Documented in the hook's own doc comment and
  DECISIONS.md, not silently worked around.
- [x] 24 new unit tests (`ApplyDamage`/`FallDamageForDistance`/
  `UpdateFallTracking`/`UpdateHealthRegen`/`UpdateStarvation`/
  `UpdateHungerDrain`/`ApplyJumpHungerCost`/`Eat`).
- [x] Verified via real `LCU_VERIFY_HEALTH` runs (bgfx + non-bgfx), a
  real two-process networked run, real regression runs of every
  existing hook (`LCU_VERIFY_BREAK_PLACE`/`CRAFT`/`TORCH`/`MENU`/`HUD`/
  `INVENTORY`/`WORKBENCH`), and a real `LCU_BUILD_SHADER_TOOLS=ON`
  build.

`ctest` 547/547 (bgfx, up from 523) / 539/539 (non-bgfx, up from 515).

Honestly scoped: no armor/enchantments reduce fall damage (out of
scope entirely); `Action::Sprint` drives hunger drain's 2x multiplier
but still doesn't move the player any faster (a real, pre-existing gap
from Phase 43); apple/bread have no survival obtain path (no
farming/mob drops); real fall damage isn't verifiable in networked
mode (see above).

## Phase 52 — Documentation

Closes the Phases 43-52 program. Documentation-only, no code changes.

- [x] **README.md**: new `## Controls` section - a full table of every
  real default keybinding (from `KeyBindings::reset_to_defaults()`),
  each noted as rebindable via Esc -> Steuerung. Feature summary bullets
  updated to mention the real inventory/crafting/HUD/menu/vitals
  systems Phases 44-51 actually built, replacing the old generic
  "inventory/item/crafting system" line.
- [x] **BUILDING.md**: new `## Options file (options.txt)` section -
  its real `SDL_GetPrefPath`-derived path (logged on every run), the
  plain-`options.txt`-in-CWD fallback if that call fails, and that
  deleting it is a safe, real reset to code defaults.
- [x] **CHANGELOG.md**: Phases 43-51 already had their own entries,
  added incrementally as each phase landed - confirmed present, no
  backfill needed. This phase's own entry added.
- [x] **PROJECT_STATE.md**: "Current Phase" and "Next Task" updated to
  reflect Phases 43-52 as a closed, done program; "Test Status"
  ctest counts/test-suite list refreshed to the real current numbers
  (were stale since before Phase 49); new Known Limitations items for
  the standing exclusion list (mobs/redstone/enchantments/Nether/
  villagers/structures/farming/chat/skins) and Phase 51's own real
  gaps (fall-damage armor, Sprint's missing speed boost, apple/bread's
  missing obtain path, the networked fall-damage verification gap).
- [x] **DECISIONS.md**: new entry recording *why* the standing
  exclusion list holds up - each excluded vertical is a genuinely
  separate system with no current attachment point, and none of them
  block what Phases 43-51 actually built.
- [x] `ctest` unchanged at 547/547 (bgfx) / 539/539 (non-bgfx) - no
  code touched this phase.

## Phase 53 — Texture-atlas pipeline

Starts the Phases 53-57 program. Infrastructure only - no real block
textures yet (Phase 54).

- [x] New `engine/assets::texture_atlas.{h,cpp}` (pure logic): fixed
  256x256 atlas, 16x16 grid of 16x16-pixel tiles, `tile_uv_range()`.
  Anti-bleed via a real half-texel UV inset, not literal padding pixels
  (see DECISIONS.md for why).
- [x] New `Renderer::create_texture_from_pixels`/`destroy_texture` -
  real bgfx RGBA8 2D texture, nearest-filtered + clamp-addressed.
- [x] `voxel::MeshVertex::texture_index` (u16), placed before the
  trailing `light` byte to avoid a real internal-padding corruption bug
  (see the field's own doc comment). `ChunkMeshLayer::add_quad` takes a
  new defaulted `texture_index` param; every face still resolves to
  tile 0 until Phase 55.
- [x] Real chunk-shader atlas sampling: `vs_chunk.sc`/`fs_chunk.sc`/
  `varying.def.sc` extended (3 new uniforms + `s_atlas` sampler),
  `fract(v_texcoord0)` wraps per-block UV for real tiling across
  greedy-meshed multi-block quads.
- [x] New `LCU_USE_TEXTURES` toggle (default ON, `=0` = exact pre-
  Phase-53 path). `client/main.cpp` creates a real placeholder flat-
  white atlas when on, proving the GPU round-trip works end to end.
- [x] 6 new unit tests.
- [x] Verified via real `LCU_USE_TEXTURES=1`/`=0` headless runs, real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_HEALTH` regression runs (still
  pass byte-identical), and a real `LCU_BUILD_SHADER_TOOLS=ON` build
  (all 3 shader profiles compile cleanly).

`ctest` 553/553 (bgfx, up from 547) / 545/545 (non-bgfx, up from 539).

Honestly scoped: no real block textures exist yet (flat white
placeholder atlas - Phase 54); Phase 53.2's optional stb_image debug
PNG dump deliberately not implemented (PARTIAL, see DECISIONS.md).

## Phase 54 — Procedural MC-style textures

- [x] New `engine/assets::procedural_textures.{h,cpp}`: 17 real,
  deterministic 16x16 RGBA generators (grass top/side, dirt, stone,
  sand, snow, water, wood side/top, leaves, coal/iron ore, torch,
  crafting-table top, cactus, compost, planks) - each a pure hash
  function of a fixed per-texture seed + pixel position, no RNG-engine
  state.
- [x] New `TileId` enum fixes each real atlas slot; `build_block_
  atlas_pixels()` packs all 17 into one real 256x256 RGBA8 buffer.
- [x] `client/main.cpp` now uploads the real generated atlas instead of
  Phase 53's flat-white placeholder.
- [x] 8 new unit tests, including a real proof the packing math lands
  each tile at its own correct slot.
- [x] Verified via a real headless run (`atlas_texture_valid=true` with
  real content) and a real `LCU_BUILD_SHADER_TOOLS=ON` build.

`ctest` 561/561 (bgfx, up from 553) / 553/553 (non-bgfx, up from 545).

Honestly scoped: no block/item yet references any of these textures -
every face/icon still renders tile 0 regardless of block type (Phase
55/56); what any of this looks like on a real GPU/display is still
**NOT VERIFIED — ENVIRONMENT LIMITATION**.

## Phase 55 — Blocks use texture atlas

- [x] `BlockDefinition` gains `top_texture`/`side_texture`/
  `bottom_texture` (`u32`, mirrors `color`'s own fallback chain - stays
  a plain `u32` not `TileId` since engine/voxel can't depend on the
  `LCU_BUILD_CLIENT`-only engine/assets).
- [x] `mesh_chunk_greedy` resolves the real per-face texture index at
  the same place/time it resolves `quad_color`; `ChunkMeshLayer::
  add_quad` takes the new `texture_index` param.
- [x] Every real block in `client/main.cpp` wired to its own real
  Phase-54 texture(s) - grass's underside is the real dirt tile (not a
  reuse of the side texture), wood's top/bottom both show growth rings.
- [x] 2 new `GreedyMesher.*` tests proving the real fallback chain.
- [x] Verified via a real headless run, a real `LCU_VERIFY_BREAK_PLACE`
  regression run (byte-identical), and a real `LCU_BUILD_SHADER_TOOLS=
  ON` build.

`ctest` 563/563 (bgfx, up from 561) / 555/555 (non-bgfx, up from 553).

Honestly scoped: what any real block looks like textured on a real
GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**;
crafting-table sides reuse the plain planks tile; torch/cactus show
one texture on every face (no per-face variant exists for either).

## Phase 56 — Items use texture atlas

- [x] `ItemDefinition` gains `texture_index` (`std::optional<u32>`);
  block-as-items reuse their block's own atlas slot; apple/bread keep
  none (no Phase 54 texture exists for either).
- [x] One `resolve_item_display` lambda in `client/main.cpp` replaces
  all 13 previous `.icon_color` lookup call sites, filling `icon_color`
  (always) and `texture_uv` (only when textures are on and a real
  `texture_index` exists).
- [x] `UiVertex2D` gains a per-vertex `use_texture` flag (not a
  per-draw uniform - a single UI batch mixes textured icons with
  flat-color borders/bars); new `Renderer::submit_textured_ui_quad`;
  `vs_ui2d.sc`/`fs_ui2d.sc` sample `s_atlas` and mix per-vertex.
- [x] `HotbarItem`/`InventorySlotDisplay` gain `texture_uv`;
  `hud_renderer.cpp`/`inventory_screen_renderer.cpp`/
  `crafting_table_screen_renderer.cpp` all branch on it identically.
- [x] Hand icon (Phase 48) textured via the same helper +
  `submit_textured_ui_quad`.
- [x] Dropped items (Phase 50's `submit_world_billboard`) textured -
  reuses `vs_sky.sc`/`fs_sky.sc`, mechanically extending the other 3
  callers of that program with always-zero UV/flag fields so their own
  output stays byte-identical.
- [x] Verified via real headless runs (`atlas_texture_valid` unchanged
  from Phase 55 under both `LCU_USE_TEXTURES` settings), real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_HEALTH` regression runs
  (byte-identical), and a real `LCU_BUILD_SHADER_TOOLS=ON` build
  (`vs_ui2d.sc`/`fs_ui2d.sc`/`vs_sky.sc`/`fs_sky.sc` all compile
  cleanly to spirv/glsl/essl).

`ctest` 563/563 (bgfx) / 555/555 (non-bgfx) - unchanged counts, since
this phase is real-rendering wiring on top of Phase 55's own
already-tested fallback-chain logic, not new pure-logic surface.

Honestly scoped: what any of this looks like textured on a real
GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**; the
sky-shader family still has no alpha blending, so a dropped torch's
transparent texture pixels render solid black on its billboard rather
than see-through (a real, accepted limitation, see DECISIONS.md).

## Phase 57 — Bitmap font atlas + real text renderer

- [x] New `engine/assets::font_atlas.{h,cpp}` - real, own-design
  procedurally-generated monospace font, ASCII 32-126 (95 chars), 6x8
  cells (5x7 glyph + 1px spacing), packed into its own SEPARATE 96x48
  RGBA8 atlas (not merged into the Phase 53 block atlas - directive's
  own wording + independent reasoning, see DECISIONS.md). 95
  hand-authored 5x7 dot-matrix glyphs, deterministic
  `generate_glyph_pixels`, same half-texel UV-inset anti-bleed as
  `texture_atlas.h`.
- [x] New `engine::ui::TextRenderer` - stateless `draw_text`/
  `measure_text_width`, one real quad per character via new
  `Renderer::submit_text_glyph_quad`.
- [x] `UiVertex2D`'s per-vertex sample flag becomes a real tri-state (0
  flat / 1 item-atlas as-is / 2 font-atlas tinted by vertex color);
  `fs_ui2d.sc` gains a second sampler (`s_font`) and a chained-mix
  3-way selector - still one draw call/batch.
- [x] `draw_debug_overlay`/`draw_hud_labels`/`draw_menu_labels`/
  `draw_inventory_screen_labels`/`draw_crafting_table_screen_labels`
  all gain a real `legacy_debug_text` param - false (new default) draws
  through TextRenderer at real pixel positions; true keeps the exact
  old `bgfx::dbgTextPrintf` behavior (`LCU_LEGACY_DEBUG_TEXT=1`).
- [x] 12 new unit tests (`FontAtlasConstants`, `GlyphUvRange.*`,
  `GenerateGlyphPixels.*`, `BuildFontAtlasPixels.*`).
- [x] Verified via real headless runs (`Font atlas: font_atlas_texture_
  valid=true`), real `LCU_VERIFY_MENU`/`INVENTORY`/`WORKBENCH` runs
  (all three real UI-text screens complete cleanly), real
  `LCU_VERIFY_BREAK_PLACE`/`HEALTH` regression runs (byte-identical),
  and a real `LCU_BUILD_SHADER_TOOLS=ON` build.

`ctest` 575/575 (bgfx, up from 563) / 567/567 (non-bgfx, up from 555).

Honestly scoped: what the real bitmap font looks like rendered on a
real GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**;
glyph shapes are plain geometric block letters, not refined typography;
no kerning (fixed monospace advance only, by design). This closes out
the Phase 53-57 program in full.

## Phase 58 — MC-sized player + visible body + skin

- [x] `kPlayerHalfWidth`/`kPlayerHeight`/`kEyeHeight` verified already
  Minecraft-correct (0.3/1.8/1.62) - no numeric change needed.
- [x] New `engine/assets::skin_texture.{h,cpp}` - real 64x64 procedural
  skin, real Minecraft "modern" dual-arm/dual-leg UV layout, 36 named
  per-face regions.
- [x] New `Renderer::submit_textured_box` - arbitrary 8-corner box,
  independent UV per face, reuses `vs_sky.sc`/`fs_sky.sc` unchanged.
- [x] Real `rotate_yaw`/`rotate_pitch`/`character_part_corners` math in
  `client/main.cpp`, derived to reproduce `FirstPersonCamera::
  forward()` exactly for the same yaw/pitch.
- [x] First-person hand icon (Phase 48) replaced by a real 3D arm box,
  textured with the held item's own atlas UV (not the skin).
- [x] Third-person: real 6-box Steve-like body (head/torso/2 arms/2
  legs); frame-rate-independent walk-cycle limb swing; head follows
  camera pitch; body yaw follows camera yaw (documented simplification);
  whole model scaled to fit exactly inside the 1.8-block hitbox.
- [x] Real 3-way F5 perspective cycle (First/ThirdBehind/ThirdFront),
  closing the previous "no third-person-front" PARTIAL.
- [x] 12 new unit tests (`SkinTextureConstants`, `SkinUvRange.*`,
  `GenerateDefaultSkinPixels.*`).
- [x] Verified via real headless runs, a real extended `LCU_VERIFY_HUD`
  run (3 F5 presses cycling all 3 perspectives, exercising all 7
  real per-frame `submit_textured_box` call sites), real
  `LCU_VERIFY_BREAK_PLACE`/`HEALTH`/`MENU`/`INVENTORY`/`WORKBENCH`/
  `CRAFT`/`TORCH` regression runs, and a real `LCU_BUILD_SHADER_TOOLS=
  ON` build.

`ctest` 587/587 (bgfx, up from 575) / 579/579 (non-bgfx, up from 567).

Honestly scoped: what the real character model/skin looks like on a
real GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**;
body yaw has no real independent lag behind camera yaw; no idle/
breathing animation for the player (Phase 59's own NPC job); no
third-person camera collision (pre-existing gap, unchanged).

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

Phase 23 (quick-craft, post-queue) gives `RecipeRegistry` its first
real caller, closing a gap flagged since Phase 5: a new `Action::Craft`
auto-assembles a query grid from one of each distinct held item type
and calls `find_match` for real - one new recipe (1 grass + 1 dirt ->
1 game:compost, the first crafted-only item) exercises both the match
and reject paths in a real run. Purely client-side, no protocol
changes. Caught and fixed a real bug along the way: the verification
hook's first, frame-count-gated version broke under real network
latency (the unthrottled client loop outran the server round trip,
causing a double-break/double-grant race) - fixed by switching to the
same wall-clock-gated pattern `LCU_VERIFY_MOVE_SECONDS` (Phase 16)
already established for this exact class of problem. `ctest` unchanged
at 344/344 (non-bgfx) / 347/347 (bgfx).

Phase 24 (item_crafted, post-queue) gives `EventBus` its second real
event, closing a gap flagged since Phase 9: `emit_item_crafted`,
called from Phase 23's quick-craft, follows the exact same
error-isolated-per-subscriber pattern `emit_block_broken` already
established. `example_mod` now subscribes to both, proving the real
register -> load -> subscribe -> emit loop generalizes, not just that
a second typed method compiles - verified via a real run showing
`[example_mod] item_crafted #1: 1 x item id 4` fire at the exact
craft moment. 3 new unit tests. `ctest` 350/350 (bgfx) / 347/347
(non-bgfx).

Known simplifications carried forward, still accurate and still
acceptable until something needs more: ~~`RecipeRegistry` has no
crafting-UI caller~~ **Fixed** (Phase 23): a real quick-craft trigger
now calls `find_match` for real, though it's still not a graphical
crafting-grid UI (no way to arrange items into specific cells - one
Craft press against an auto-built grid is the entire interaction) and
only correctly represents recipes needing exactly one of each distinct
ingredient type; ~~`EventBus` has only one real event (`block_broken`)~~
**Fixed** (Phase 24): `item_crafted` is a second real event, though
still purely client-side content moments so far - nothing server-side
fires an event yet; item drops are now looked up through a real
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
has no persistence across a disconnect/reconnect; `EventBus` has two
real events now (`block_broken`, `item_crafted`, Phase 24) - both are
client-triggered content moments, nothing server-side fires one yet;
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
