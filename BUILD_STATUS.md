# Build Status

Honest, current-as-of-last-update record of what actually builds, runs, and
is tested — vs. what is only present as directory structure or CMake
plumbing. See brief section 96: never claim "fertig"/"works" beyond what
was actually verified.

Legend: **TESTED** (built and executed successfully in this environment),
**BUILDABLE** (configures/compiles but not executed, or executed with
caveats noted), **UNTESTED** (exists but not attempted here), **BLOCKED**
(cannot be attempted in this environment and why).

## Environment this was last verified in

Linux x86_64 sandbox (this remote execution container), no GPU, no
display server (Xvfb not installed), CMake 3.28.3, GCC 13.3.0 / Clang
18.1.3, Ninja 1.11.1, 4 cores, 15GB RAM, 30GB disk. Outbound `git clone`
over `https://github.com/...` works; `https://api.github.com` is blocked
(403) by network policy but unused by the build.

## Targets

| Target | Status | Notes |
|---|---|---|
| `LcuCore` | **TESTED** | Builds, unit tests pass (`ctest`, 12/12). |
| `LcuMath` | **TESTED** | Header-only, unit tests pass for Vec3/Mat4. |
| `LcuPlatform` (SDL3 window) | **TESTED** | Builds against fetched SDL3 release-3.2.10. `VoxelClient` creates a real `SDL_Window` and pumps events under `SDL_VIDEODRIVER=dummy` (no real display in this sandbox) and exits cleanly. **Not** visually verified (no GPU/display here) — someone with a desktop must confirm a window actually appears on screen. |
| `VoxelClient` | **TESTED** (headless) / **UNTESTED** (visual) | Loads a real 36-chunk `World` around spawn, spawns a physics-driven player, and runs the full camera/movement/raycast/break-place loop under dummy SDL driver, backed by a real `Inventory`, real per-chunk lighting, 3 wandering AI entities (single-player mode) or server-driven interpolated AI (networked mode), and a ticking `DayNightCycle`. `LCU_MAX_FRAMES` bounds the loop for CI; `LCU_VERIFY_BREAK_PLACE` synthesizes a break-then-place input sequence; `LCU_CONNECT_PORT` enables real networked mode against a `VoxelServer` on `127.0.0.1`. Confirmed via real runs (see below for the networked case): "Sky light 5 blocks above spawn column: 15", "Picked up 1 game:stone (inventory: 1)" then "Placing block ... (inventory: 0)", "Day/night: time_of_day=0.000 sky_light_scale=0.550". **Not** visually verified (no GPU/display here). |
| `VoxelServer` | **TESTED** | Parses `--world`/`--port`, generates/loads a real 36-chunk `World`, spawns and simulates 3 wandering AI entities (`engine/ecs` + `game::systems::update_ai_wander`) at 20 TPS, tracks one real `PlayerPhysicsState` per connected client (driven by received `PlayerInput`), and listens for real UDP connections (`engine/network`). Exits cleanly via `LCU_MAX_TICKS`. Verified end-to-end with a standalone Python UDP client script (Welcome/EntityState/PlayerCorrection all decoded and sanity-checked against hand-computed physics) and with a real `VoxelClient` process - see the networked-mode row below and NETWORKING.md for the exact reproduce steps. `ldd` reconfirms **zero** SDL/bgfx link dependency (only libc/libstdc++/libm/libgcc_s). |
| `VoxelClient`<->`VoxelServer` real multiplayer | **TESTED** (two-process, loopback) | `LCU_CONNECT_PORT=<port> ./VoxelClient` against a running `VoxelServer` on the same port: client connects, receives a genuine Welcome (`world_seed=1337 tick_rate=20`), renders all 3 remote AI entities via real `PositionInterpolator` interpolation (not local simulation), predicts local player movement via `PredictionBuffer`, sends `PlayerInput`, and reconciles against the server's `PlayerCorrection` - confirmed via a real run: "Received Welcome: world_seed=1337 tick_rate=20", "Remote AI entity (index=1) interpolated position: (4.00, 29.00, 0.00)" (matching the server's own live spawn position), "Player position (server-reconciled): (-0.30, 29.00, -0.30)". Verified in both build configs. **Not** verified: a non-loopback network, or any visual rendering of the above. |
| Block edit replication (Phase 13) | **TESTED** (three-process, loopback) | `handle_block_action` on `VoxelServer`: validates a client's `BlockAction` (chunk loaded, break targets non-air/place targets air with a registered block_id, target within `kMaxBlockActionRange` of the requester's server-known position), applies it to the server's `World`, and broadcasts `BlockChange` to every connected client, including the requester (no client mutates its own `World` speculatively for a block edit - see DECISIONS.md). Verified via a real 3-process run (1 server, 2 independent clients - one acting via `LCU_VERIFY_BREAK_PLACE`, one purely observing): server logs `Applied BlockAction from <addr>: (0,28,-1) 1 -> 0` then `(0,29,-1) 0 -> 1`; **both** clients (including the one that never touched either block) log `Applied server BlockChange` for both edits - confirming both `World`s actually converged, not just that a message decoded. A newly connecting client also gets replayed the server's full `block_change_history` right after its Welcome - verified via a real run where a second client connects only *after* both edits happened and still catches up on both (`VoxelServer` logs `Replayed 2 historical block change(s) to <addr>`). **Not** verified: server-side inventory (item pickup/placement is still client-local/optimistic), non-loopback network, more than two simultaneous clients. |
| Chunk network streaming (Phase 14) | **TESTED** (two real two-process runs, loopback, two scales) | `lcu::network::fragment_payload`/`FragmentReassembler` split/rejoin an oversized payload across `ReliableOrdered` datagrams; `game::systems::protocol::ChunkData`/`ChunkDataFragment` carry a `lcu::serialization::serialize_chunk_to_bytes`-compressed chunk. Right after `Welcome`+`block_change_history` replay, `VoxelServer` sends every currently-loaded chunk (`World::loaded_chunk_coords()`) to a newly-connecting client; `VoxelClient` reassembles, `deserialize_chunk_from_bytes`s, fully overwrites its own local chunk, relights, and remeshes it plus its six neighbors. Verified via a `mobile_low` (1-chunk world) run - server logs `Sent 1 chunk(s) (1 fragment(s))`, client logs `Applied server ChunkData for chunk (0, 1, 0)` - and a `desktop` (36-chunk world) run - server logs `Sent 36 chunk(s) (36 fragment(s))`, client logs 36 matching `Applied server ChunkData` lines, zero warnings/errors either run. **Not** verified: a chunk large enough to need >1 fragment in a real run (both test worlds' terrain compressed under the 1024-byte-per-fragment budget - the multi-fragment path itself is covered by unit tests, see `FragmentPayload.LargePayloadSplitsIntoMultipleFragments`), re-streaming after the initial connect-time sync, non-loopback network. |
| Server-side inventory (Phase 15) | **TESTED** (two-process, loopback) | `VoxelServer` registers the same `game:stone` item `VoxelClient` does and gives each `ClientState` a real 9-slot `lcu::items::Inventory`. `handle_block_action` rejects placing `game:stone` unless the requester actually holds one server-side, and adds/removes one on a successful break/place. `InventoryUpdate` (server->one client, `ReliableOrdered`) is sent after every `BlockAction` - accepted or rejected - with that client's current authoritative count; `VoxelClient` reconciles its existing optimistic guess against it (same pattern as `PlayerCorrection`). Verified via a real run (`LCU_VERIFY_BREAK_PLACE`): client log shows `Picked up 1 game:stone (inventory: 1)` then `Requesting place ... (inventory: 0)`, immediately followed by `Reconciled inventory item 1 to authoritative count 1 (was 0)` and `Applied server BlockChange` for the break, then `Reconciled inventory item 1 to authoritative count 0 (was 1)` and `Applied server BlockChange` for the place - the optimistic guess and the server's authoritative count actually converge each time, not just that a message decoded. **Not** verified: a rejected-place-due-to-empty-inventory scenario in a real run (covered by the validity-check code path and existing rejection-logging pattern, not a dedicated synthetic-input test), persistence across reconnect, any item/block besides `game:stone`. |
| Per-movement chunk streaming (Phase 16) | **TESTED** (real two-process and three-process runs, loopback) | `VoxelServer` re-checks every connected client's chunk column every tick (only when it's changed since last checked); loads any not-yet-loaded chunk in `load_settings` range using the same logic the startup area uses, and broadcasts every newly-loaded chunk as `ChunkData` to every connected client. Deliberately append-only - the shared `World` never unloads (see DECISIONS.md). `VoxelClient` mirrors the same trigger locally. New headless hook `LCU_VERIFY_MOVE_SECONDS` holds `MoveForward` for N real (wall-clock) seconds. Two-process run: client holds `MoveForward` for 6s (crosses the 16-block chunk boundary) - server logs `Streamed 1 newly-loaded chunk(s) into range (total 2 loaded)`, client logs `Applied server ChunkData for chunk (0, 1, -1)`, zero warnings/errors. Three-process run: adds a second, entirely stationary client (never sends a nonzero `PlayerInput`) whose own log shows the identical `Applied server ChunkData for chunk (0, 1, -1)` line - proving the broadcast reaches every connected client, not just the one that triggered it. **Not** verified: chunk unloading (deliberately doesn't exist yet), non-loopback network, more than two simultaneous clients. |
| Surface/subsurface terrain content (Phase 17) | **TESTED** (real single-process and two-process runs) | `generate_terrain_chunk`'s signature changed from a single `solid_block` to `(surface_block, subsurface_block, stone_block)`, layering a real grass-over-dirt-over-stone column instead of one block type filling everything below the height. `VoxelClient`/`VoxelServer` both register `game:grass`/`game:dirt` identically (same fields, same order right after `game:stone`), so `BlockId`s coincide. Both new blocks flow through the existing collision/meshing/replication pipeline unmodified - nothing in `engine/physics`, `engine/voxel::mesh_chunk_greedy`, or the `ChunkData` wire format hardcodes a specific block id. Verified via a real single-player run (36-chunk world generates and runs the full break/place round trip with no crash) and a real two-process networked run streaming a chunk that actually contains the new layered content (`Sent 1 chunk(s) (1 fragment(s))` / `Applied server ChunkData`), zero warnings/errors either run. **Not** verified: what the new blocks look like on a real GPU/display (no texture/color distinction exists yet - see DECISIONS.md/Known Limitations). |
| Item mappings for grass/dirt (Phase 18) | **TESTED** (real single-process and two-process runs) | `VoxelClient` registers `game:grass`/`game:dirt` items (1:1 to their block counterparts) and a `grant_item_for_broken_block` helper replaces two duplicated stone-only checks with one lookup covering all three blocks. `VoxelServer` registers the same two items (same order) purely for `ItemId` alignment - doesn't track either server-side yet. Verified via a real single-player run (`LCU_VERIFY_BREAK_PLACE`): the player spawns on a grass surface block (Phase 17's layering), log shows `Breaking block at world (0, 28, -1)` then `Picked up 1 game:grass (inventory: 1)` - an unforced real exercise, not a contrived scenario. Verified via a real two-process networked run: server logs `Applied BlockAction from <addr>: (0,28,-1) 2 -> 0`, client logs `Requesting break`, `Picked up 1 game:grass (inventory: 1)`, `Applied server BlockChange ... block_id=0`. **Not** verified: placing grass/dirt (not wired up - no hotbar/item-selection UI), server-side authoritative tracking for either (client-optimistic only, same as `game:stone` before Phase 15). |
| `VoxelTests` (GoogleTest) | **TESTED** | 343/343 tests pass (bgfx build) / 340/340 (non-bgfx build, `ChunkMeshUpload.*` excluded): `Log.*`, `Vec3.*`, `Mat4.*`, `FrameStats.*`, `InputState.*`, `Chunk.*`, `ChunkStorage.*`, `ChunkCoord.*`, `BlockRegistry.*`, `GreedyMesher.*`, `JobSystem.*`, `ChunkMeshUpload.*`, `World.*`, `Worldgen.*`, `ChunkSerializer.*`, `Raycast.*`, `AABB.*`/`Collision.*`/`PlayerPhysics.*`, `FirstPersonCamera.*`, `MovementInput.*`, `ItemRegistry.*`, `Inventory.*`, `RecipeRegistry.*`, `Registry.*`, `ComputeBlockLight.*`/`PropagateAddedBlockLight.*`/`UnpropagateBlockLight.*`/`ComputeSkyLight.*`/`LightStorage.*`, `AIWanderSystem.*`, `DayNightCycle.*`, `Sequence.*`, `PacketHeader.*`, `Connection.*`, `UdpSocket.*`, `Address.*`, `LoopbackIntegration.*`, `FragmentPayload.*`, `FragmentReassembler.*`, `PositionInterpolator.*`, `PredictionBuffer.*`, `ReplicationProtocol.*`, `LuaState.*`, `EventBus.*`, `RegistryBindings.*`, `ModLoader.*`, `TouchInputBackend.*`, `QualityProfile.*`, `ParseQualityProfile.*`, `GenerateSineWave.*`, `ComputeStereoPan.*`, `DistanceAttenuation.*` (now incl. 10 `ReplicationProtocol.BlockAction*`/`BlockChange*` cases from Phase 13, 6 `ReplicationProtocol.ChunkData*` cases + 11 `FragmentPayload*`/`FragmentReassembler*` + 4 `ChunkSerializer` in-memory cases from Phase 14, 6 `ReplicationProtocol.InventoryUpdate*` cases from Phase 15, real grass/dirt/stone layering cases from Phase 17). |
| `engine/platform::InputState`/`KeyboardInputBackend` | **TESTED** | `InputState` unit tested directly (3 cases). `KeyboardInputBackend` (SDL-backed) exercised every `VoxelClient` run, not unit tested in isolation (would need a live SDL keyboard state). No mouse-look, gamepad, or touch backend yet. |
| `engine/debug::FrameStats` | **TESTED** | Unit tested (3 cases) and verified live: a 2-second `SDL_VIDEODRIVER=dummy` `VoxelClient` run produced real `fps=60165.4 frame_ms=0.02 total_frames=60166` output. Text log line only, no on-screen overlay yet. |
| `engine/voxel::ChunkStorage`/`Chunk` | **TESTED** | Flat-array chunk storage, default 16^3. 8 unit tests incl. an exhaustive sweep proving `index_of` is injective over the full 4096-cell volume, and death tests for out-of-bounds access. Alternative chunk sizes proven via `ChunkStorage<8>`. |
| `engine/voxel::world_to_chunk_and_local` | **TESTED** | Floor-division coordinate splitting. 7 unit tests incl. a round-trip sweep across ~100 positive/negative coordinate pairs and explicit boundary cases (-1, -16, -17 relative to a 16-edge chunk). |
| `engine/voxel::BlockRegistry` | **TESTED** | Namespaced, datadriven block definitions; air always id 0. 5 unit tests incl. death tests for duplicate-id registration. Zero blocks registered outside tests — no gameplay content yet. |
| `engine/jobs::JobSystem` | **TESTED** | Worker pool, priority scheduling, dependency graphs (incl. cascading cancellation), cancellation of not-yet-started jobs. 12 unit tests. Additionally: 200 repeated test-suite runs (normal build) and 50 runs under GCC ThreadSanitizer, both zero failures/races. Now has a real consumer: `VoxelClient` dispatches `mesh_chunk_greedy` through it. |
| `engine/voxel::mesh_chunk_greedy` | **TESTED** (geometry) / **UNTESTED** (visual) | Axis-sweep greedy meshing, registry-driven opacity, opaque layer only (transparent/water layers exist structurally but always empty - no transparent block registered anywhere yet, see DECISIONS.md). 8 unit tests incl. a geometric cross-product check that every triangle's winding matches its stored normal, and merge-count assertions distinguishing "same block type merges" from "different block type doesn't merge". **Not verified**: what it actually looks like rendered - no display/GPU in this sandbox. |
| `engine/rendering::upload_chunk_mesh_layer`/`GpuChunkMesh` | **TESTED** (buffer creation) | Creates real bgfx `VertexBufferHandle`/`IndexBufferHandle` from a `ChunkMeshLayer`. 3 unit tests, plus a real `VoxelClient` run confirming `valid=true index_count=36` end-to-end (chunk -> job-dispatched mesh -> GPU buffers). |
| `client/shaders/{vs_chunk,fs_chunk}.sc` + `engine/rendering::load_chunk_program` | **TESTED** (headless draw call) / **UNTESTED** (visual) | Minimal directional+ambient shader (no texturing - no atlas yet), compiled via bgfx's `shaderc` when `LCU_BUILD_SHADER_TOOLS=ON` (opt-in, see BUILDING.md/DECISIONS.md - pulls in glslang/SPIRV-Tools/SPIRV-Cross/Dawn-Tint). Loaded into a real `bgfx::ProgramHandle` and submitted via `Renderer::submit_chunk_mesh()` -> `bgfx::submit()` every frame. Verified via a real `VoxelClient` run: `"Chunk shader program valid=true"`, 3 clean frames with the draw call executing, under the `Noop` backend. **Not verified**: what it actually renders on a real GPU/display - none exists in this sandbox. Without `LCU_BUILD_SHADER_TOOLS=ON`, `VoxelClient` still runs fine and logs `valid=false` for the program, skipping the draw (no crash). |
| bgfx (`engine/rendering`) | **TESTED** (headless) / **UNTESTED** (real GPU backend) | Builds cleanly (`libbgfx.a`, `libbx.a`, `libbimg.a`) after installing `libgl1-mesa-dev`/`libglu1-mesa-dev`/`mesa-common-dev`/`libwayland-dev` (see `DECISIONS.md`). `engine/rendering::Renderer` wraps `bgfx::init`/`frame`/`shutdown`. `VoxelClient` built with `LCU_ENABLE_BGFX=ON`, run under `SDL_VIDEODRIVER=dummy`, initializes bgfx on the `Noop` backend (no native window handle available) and completes a 5-frame clear loop cleanly. **Not verified**: a real Vulkan/GL backend actually presenting to a screen — no GPU/display in this sandbox. Someone with a desktop must confirm `VoxelClient` shows an actual window with the clear color. |
| `engine/world::World` | **TESTED** | Sparse chunk table, lifecycle state machine, distance-based streaming with hysteresis. 12 unit tests incl. a death test for out-of-order state transitions and streaming hysteresis behavior. |
| `engine/world::worldgen` | **TESTED** | Deterministic seeded value-noise terrain height (continental+terrain only). 8 unit tests: determinism, cross-seed variation, bounded range, column-to-column smoothness, chunk-fill correctness incl. all-air/all-stone extremes, and (Phase 17) real surface(grass)/subsurface(dirt)/stone layering - `generate_terrain_chunk` now takes three block ids instead of one. |
| `engine/serialization::chunk_serializer` | **TESTED** | zstd-compressed, versioned chunk save/load with corruption/version detection. 7 unit tests: full 4096-cell round-trip, empty chunk, missing file, garbage file, flipped version byte, corrupted compressed payload, and confirmation a failed load never touches the output chunk. |
| `engine/physics::raycast` | **TESTED** | Voxel DDA (Amanatides & Woo) against `World`, predicate-driven solidity. 9 unit tests incl. a hand-computed exact-distance case, diagonal-ray normal validity, and the origin-starts-inside-solid edge case. |
| `engine/physics::{AABB, move_and_collide, integrate_player}` | **TESTED** | Axis-independent Y->X->Z collision resolution, gravity, jump, auto-step. 18 unit tests, including a regression test for a real grounding-detection bug (a stationary grounded player briefly reported ungrounded) found and fixed via a dedicated ground-probe before any test caught it - see DECISIONS.md - plus hand-computed auto-step clamp positions. |
| `engine/player::{FirstPersonCamera, movement_direction_from_input}` | **TESTED** | Yaw/pitch camera matching the existing `Mat4::look_at` -Z-forward convention; WASD-relative normalized movement direction. 12 unit tests. |
| `VoxelClient` Phase 4 wiring | **TESTED** (headless logic) / **UNTESTED** (visual) | Loads a 36-chunk `World` area, spawns a physics-driven player on the generated surface, drives camera/movement from `InputState`, raycasts every frame, and mutates the world on edge-detected break/place - remeshing the affected chunk plus any neighbor sharing the mutated boundary. Verified via a real run with the `LCU_VERIFY_BREAK_PLACE` synthetic-input hook: break then place round-trips to the same world position. |
| `engine/items::ItemRegistry` | **TESTED** | Namespaced, datadriven item definitions; `kNoItemId` always id 0 (mirrors `BlockRegistry`'s `kAirBlockId`). 5 unit tests incl. a death test for duplicate-id registration. |
| `engine/items::Inventory` | **TESTED** | Slot-based `ItemStack` storage. 10 unit tests: empty-slot defaults, existing-partial-stack top-up before spilling into a new slot, per-item `max_stack_size` enforcement, leftover-on-full, remove/count semantics. |
| `engine/items::RecipeRegistry` | **TESTED** | Shaped (query-grid bounding-box trim then exact cell match, single orientation) and shapeless (ingredient-multiset match) recipe matching. 9 unit tests incl. an off-center shaped match via bounding-box trim and an exact-multiset shapeless rejection test (extra unrelated item in the grid correctly fails to match). No crafting-UI caller yet - tested standalone. |
| `VoxelClient` Phase 5 wiring | **TESTED** (headless logic) | Breaking a block now adds a `game:stone` item to a real 9-slot player `Inventory`; placing now consumes one (refunded if the target chunk isn't loaded). Verified via a real run with `LCU_VERIFY_BREAK_PLACE`: "Picked up 1 game:stone (inventory: 1)" then "Placing block ... (inventory: 0)". |
| `engine/ecs::Registry` | **TESTED** | Generation-checked `EntityId` handles, sparse-set `ComponentPool<T>` per type (dense contiguous storage, swap-and-pop removal). 13 unit tests incl. stale-handle-after-recycling and swap-and-pop-keeps-other-components-intact regression checks. |
| `engine/lighting::{LightStorage, propagation}` | **TESTED** | Packed 4-bit sky + 4-bit block light per voxel. `compute_block_light`/`compute_sky_light` (initial per-chunk flood); `propagate_added_block_light`/`unpropagate_block_light` (true incremental local updates for a single block add/remove, not a full recompute). 12 unit tests incl. an exact-match check between the incremental add path and a full recompute, and a two-source removal test confirming the refill phase correctly reproduces what a solo-source recompute would give. Single-chunk scope (no cross-chunk bleed) - see DECISIONS.md. |
| `game::systems::{update_ai_wander, DayNightCycle}` | **TESTED** (logic) / **UNTESTED** (visual) | Wandering AI (idle/walk-to-target loop, deterministic via an explicit `std::mt19937`) and a cosine day/night sky-light-scale curve. 13 unit tests. Neither has a visual representation in this sandbox - AI entities and the sky are only observed via log lines. |
| `VoxelClient` Phase 6 wiring | **TESTED** (headless logic) | Computes real per-chunk block+sky light at load, keeps it correct through every break/place edit via the incremental primitives (plus a per-column sky light refresh); spawns 3 wandering AI entities and a `DayNightCycle`, both updated every frame. Also fixed a real spawn-height off-by-one (player/AI were spawning embedded one block into the ground) found while verifying this. Verified via a real run: "Sky light 5 blocks above spawn column: 15", real AI positions logged, "Day/night: time_of_day=0.000 sky_light_scale=0.550". |
| `engine/network::{UdpSocket, Address}` | **TESTED** (Linux) / **UNTESTED** (Windows/macOS) | Cross-platform (POSIX/Winsock, selected at compile time) non-blocking IPv4 UDP socket wrapper. 9 unit tests incl. a real loopback send/receive round-trip on `127.0.0.1` and an oversized-payload rejection. Winsock code path exists but is unexercised in this Linux-only sandbox. |
| `engine/network::{Connection, PacketHeader, sequence_greater_than}` | **TESTED** | Implements all four `ARCHITECTURE.md` channel semantics (`UnreliableUnordered`/`UnreliableSequenced`/`ReliableUnordered`/`ReliableOrdered`) over ack/retransmit - see NETWORKING.md for the wire format. 25 unit tests covering the protocol logic against simulated packets (ordering, dedup, retransmission timing, independent per-channel sequence spaces) plus header serialization edge cases and wraparound-correct sequence comparison. |
| `tests/network::LoopbackIntegration` | **TESTED** | Two real `Connection`s driven over two real `UdpSocket`s on `127.0.0.1`. 2 tests: a `ReliableOrdered` message that survives a deliberately dropped first real UDP datagram (retransmission recovery, not a hypothetical), and an `UnreliableSequenced` round-trip. |
| `VoxelServer` Phase 7 wiring | **TESTED** (via a real two-process run) | Real `World`+AI simulation (see `VoxelServer` row above) plus a real UDP handshake: a connecting peer gets a `ReliableOrdered` Welcome (world seed + tick rate) and per-tick `UnreliableSequenced` Heartbeats. No automated test spawns the `VoxelServer` binary as a subprocess yet - verified manually via a standalone Python UDP client script against a running instance (see NETWORKING.md for the reproduce steps), same class of verification as `LCU_VERIFY_BREAK_PLACE` on `VoxelClient`. |
| `engine/replication::PositionInterpolator` | **TESTED** | Buffers timestamped position samples, linearly interpolates at a configurable render delay, clamps (doesn't extrapolate) past the buffer's ends. 9 unit tests incl. multi-segment interpolation and old-sample eviction. |
| `engine/replication::PredictionBuffer<State, Input>` | **TESTED** | Generic client-side prediction + server reconciliation (predict-and-record, then discard-acknowledged-and-replay-the-rest). 6 unit tests: 5 against a hand-verifiable plain-`float` instantiation (exact arithmetic checks), 1 against a real `lcu::physics::PlayerPhysicsState`/`integrate_player` instantiation with a hand-computed expected reconciled position. |
| `game::systems::protocol` | **TESTED** | Shared `VoxelClient`/`VoxelServer` wire messages (`Welcome`/`Heartbeat`/`EntityState`/`PlayerInput`/`PlayerCorrection`), hand-rolled big-endian encode/decode. 11 unit tests: round-trips for every message (incl. negative floats) and rejection of wrong-type/truncated/empty payloads. |
| `VoxelServer` Phase 8 wiring | **TESTED** (via real runs) | One `PlayerPhysicsState` per connected client, driven by received `PlayerInput` through the same physics `VoxelClient` runs; periodic `PlayerCorrection`; per-client `EntityState` filtered by a real `kInterestRadius` distance check. Verified via both the Python client script and a real `VoxelClient` connection (see the multiplayer row above) - physics results cross-checked by hand (e.g. `PlayerCorrection` position after a `{1,0,0}` input with `dt=0.05` matched the expected `+1.0` x-shift exactly). |
| `VoxelClient` Phase 8 wiring | **TESTED** (via a real two-process run) | `LCU_CONNECT_PORT`-gated networked mode: predicts local player movement via `PredictionBuffer` and reconciles against `PlayerCorrection`; renders remote AI via `PositionInterpolator` fed by `EntityState` instead of local simulation. Single-player mode (env var unset) reverified byte-for-byte unchanged. See the multiplayer row above for the actual verification output. |
| `engine/scripting::LuaState` | **TESTED** | RAII wrapper around a Lua 5.4.7 VM (official upstream, embedded via its own `onelua.c` amalgamation + `-DMAKE_LIB`). Opens only base/table/string/math (no io/os/package - no filesystem/process access for a mod by default). 7 unit tests incl. confirming `io`/`os`/`package`/`require` are all genuinely absent, and a real `register_function`/upvalue round-trip. |
| `engine/modding::EventBus` | **TESTED** | Named pub/sub bus: `lcu.subscribe(event_name, fn)` from Lua, typed `emit_block_broken(x, y, z, block_id)` from C++. An erroring handler is logged and skipped without blocking the remaining subscribers. 7 unit tests incl. multi-subscriber call order and the erroring-handler-doesn't-block-others case. |
| `engine/modding::{bind_block_registry, bind_item_registry}` | **TESTED** | Exposes `register_block(namespaced_id, display_name, is_transparent, has_collision)`/`register_item(namespaced_id, display_name, max_stack_size)` to Lua, writing straight into the same `BlockRegistry`/`ItemRegistry` the base game uses. 6 unit tests incl. Lua-omitted-optional-argument defaults matching `BlockDefinition`/`ItemDefinition`'s own C++ defaults. |
| `engine/modding::ModLoader` | **TESTED** | Enumerates `<mods_dir>/<mod_name>/init.lua`, one shared `LuaState` per host process; a mod whose script errors (or has no init.lua) is logged and skipped, not fatal to the others. 6 unit tests incl. a real scratch-directory round-trip and the partial-failure case (one mod errors, the next still loads). |
| `mods/example_mod` + `VoxelClient`/`VoxelServer` Phase 9 wiring | **TESTED** (via real runs) | A real working mod: registers `example_mod:magic_stone`/`example_mod:magic_wand`, subscribes to `block_broken`, logs every block it sees broken. Both `VoxelClient` and `VoxelServer` construct their own `LuaState`, bind their own registries, and call `ModLoader::load_all("mods")` at startup (the server also gets an `EventBus`/`ItemRegistry` so the shared mod script has a uniform Lua API on both hosts; since Phase 13, the server *does* call `emit_block_broken` too, from `handle_block_action` on a validated networked break). Verified via real runs: `VoxelServer` logs `[example_mod] registered block 'example_mod:magic_stone' -> id 2`, `registered item 'example_mod:magic_wand' -> id 1`, `[example_mod] loaded`, `Loaded 1 mod(s) from 'mods'`; `VoxelClient` under `LCU_VERIFY_BREAK_PLACE` additionally logs `[example_mod] block_broken #1: block id 1 broken at (0, 28, -1)` immediately after `Breaking block at world (0, 28, -1)` - the event fires with the correct coordinates/block id at the exact moment a real block is broken. |
| `engine/platform::TouchInputBackend` | **TESTED** (logic) / **UNTESTED** (real hardware) | Twin-virtual-stick touch-to-`Action` mapping (movement/look drags with a dead zone, fixed button rects for Jump/Interact/PlaceBlock/Sprint/Crouch/Inventory), writing into the same `InputState` `KeyboardInputBackend` does. 13 unit tests against synthetic `TouchPoint` lists. Not wired to any real `SDL_EVENT_FINGER_*` source yet - no touchscreen in this sandbox to test that against. |
| `lcu::core::{QualityProfile, chunk_load_settings_for}` | **TESTED** | `Desktop` numerically matches this project's pre-existing hardcoded chunk-load radius/vertical range (zero behavior change by default); `MobileLow`/`MobileMedium`/`MobileHigh` each load strictly fewer chunks. 4 unit tests. Wired into both `VoxelClient`/`VoxelServer` via `LCU_QUALITY_PROFILE`; verified via real runs: default and an invalid env var value both still log "Loaded 36 chunks"; `mobile_low`/`mobile_high` log "Loaded 1 chunks"/"Loaded 27 chunks". |
| Android build (`CMakePresets.json` `android-arm64`) | **BLOCKED here** | No Android NDK installed in this sandbox; preset requires `ANDROID_NDK_HOME`. Re-verified this phase: `cmake --preset android-arm64` reaches and fails only at Android's own NDK-detection step ("Neither the NDK or a standalone toolchain was found") - confirms the preset itself is structurally correct, not broken CMake. Untested, not un-buildable — needs a machine/CI runner with the NDK. No Gradle project/AndroidManifest.xml exists - deferred until there's a toolchain to build one against (see DECISIONS.md). |
| `tools/benchmark::VoxelBenchmarks` | **TESTED** (built and run for real numbers) | Google Benchmark (FetchContent, opt-in via `LCU_BUILD_TOOLS=ON`), 15 benchmark cases against real engine code: `ChunkStorage::set_block`/`block_at`, `worldgen::generate_terrain_chunk`, `mesh_chunk_greedy` (solid + checkerboard chunks), `compute_block_light`/`compute_sky_light`, `raycast`/`move_and_collide`, `save_chunk_to_file`/`load_chunk_from_file` (zstd compression happens inside these), a `Connection` `ReliableOrdered` round trip, and `update_ai_wander` at 10/100/1000 entities. Run in both `Development` (this project's default - Google Benchmark itself flags it "Library was built as DEBUG", since `Development` applies no optimization flags) and `Release` (clean, optimized) configurations in this sandbox (4-core, 2.1GHz container) - see the Release numbers in the reproduce section below. This sandbox's numbers only - not representative of any other machine. |
| `engine/audio::{AudioEngine, generate_sine_wave}` | **TESTED** (waveform logic) / **TESTED** (SDL device, under `SDL_AUDIODRIVER=dummy`) | `AudioEngine` wraps one `SDL_AudioStream` (`SDL_OpenAudioDeviceStream`, 44.1kHz stereo float); `generate_sine_wave` produces real procedural PCM tone content (own IP, no asset pipeline - see DECISIONS.md). 7 unit tests on the pure waveform math. Verified via a real `VoxelClient` run under `SDL_AUDIODRIVER=dummy`: `"AudioEngine initialized: 44100 Hz, stereo float"`, then the break/place round trip completing with real `play()` calls and no crash. Without a real (or dummy) audio device, `init()` fails, is logged, and `play()` silently no-ops - not fatal. **Not verified**: what it actually sounds like - no speakers/audio device in this sandbox either way. |
| `engine/audio::{compute_stereo_pan, distance_attenuation}` | **TESTED** | Pure math, no SDL dependency. 10 unit tests incl. directly-ahead/fully-left/fully-right/45-degree pan cases (hand-computed expected gains) and the zero-distance no-divide-by-zero edge case. |
| `engine/ui::draw_debug_overlay` + `Renderer::{draw_debug_text, clear_debug_text}` | **TESTED** (headless draw call) / **UNTESTED** (visual) | Draws live FPS + the mobile touch-control button legend via bgfx's built-in debug-text buffer (`BGFX_DEBUG_TEXT`, enabled in `Renderer::init`), reading button positions from the same `lcu::platform::kTouchButtonLayout` `TouchInputBackend` hit-tests against (promoted out of `touch_input.cpp` into a shared header specifically so the two can't drift apart - see DECISIONS.md). Verified via a real bgfx (`Noop` backend) `VoxelClient` run: full startup-to-`LCU_MAX_FRAMES`-shutdown with `draw_debug_overlay` called every frame, no assert/crash. **Not verified**: what it looks like on a real display - no GPU/display in this sandbox. |
| iOS build (`CMakePresets.json` `ios`) | **BLOCKED here** | Requires Xcode on a macOS host; this sandbox is Linux. Untested. |
| Windows (MSVC preset) | **BLOCKED here** | Requires a Windows host/toolchain; this sandbox is Linux. Untested. |
| macOS preset | **BLOCKED here** | Requires a macOS host (Metal via bgfx); this sandbox is Linux. Untested. |

## How to reproduce the verified results

```sh
# Fast, non-graphics build (no bgfx compile, minutes not tens-of-minutes):
cmake -S . -B build/dev-nobgfx -G Ninja -DCMAKE_BUILD_TYPE=Development -DLCU_ENABLE_BGFX=OFF
cmake --build build/dev-nobgfx -j4
ctest --test-dir build/dev-nobgfx --output-on-failure
SDL_VIDEODRIVER=dummy LCU_MAX_FRAMES=5 ./build/dev-nobgfx/bin/VoxelClient
# Exercises the Phase 4 break/place pipeline headlessly (no real keyboard here):
SDL_VIDEODRIVER=dummy LCU_MAX_FRAMES=10 LCU_VERIFY_BREAK_PLACE=1 ./build/dev-nobgfx/bin/VoxelClient
LCU_MAX_TICKS=5 ./build/dev-nobgfx/bin/VoxelServer --world TestWorld --port 25566
ldd ./build/dev-nobgfx/bin/VoxelServer   # confirm no SDL/bgfx

# Exercises the real Phase 7 network handshake end to end (see NETWORKING.md):
LCU_MAX_TICKS=100 ./build/dev-nobgfx/bin/VoxelServer --world TestWorld --port 25599 &
python3 -c "
import socket, struct, time
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sock.settimeout(2.0)
sock.bind(('127.0.0.1', 0))
sock.sendto(bytes([0,0,0,0]) + b'hello', ('127.0.0.1', 25599))  # UnreliableUnordered Data packet
data, _ = sock.recvfrom(2048)  # expect a ReliableOrdered (channel=3) Welcome back
seed = struct.unpack('>I', data[5:9])[0]; tick_rate = data[9]
print(f'Welcome: world_seed={seed} tick_rate={tick_rate}')
"

# Exercises the real Phase 8 multiplayer loop end to end - a real
# VoxelClient connecting to a real VoxelServer over loopback UDP:
LCU_MAX_TICKS=400 ./build/dev-nobgfx/bin/VoxelServer --world TestWorld --port 25602 &
SDL_VIDEODRIVER=dummy LCU_MAX_FRAMES=60 LCU_CONNECT_PORT=25602 ./build/dev-nobgfx/bin/VoxelClient
# Expect in the client's log: "Received Welcome: world_seed=1337 tick_rate=20",
# "Remote AI entity (index=...) interpolated position: (...)" for all 3 AI
# entities, and "Player position (server-reconciled): (...)".

# Exercises the real Phase 9 modding loop end to end - a real mod
# registering content and receiving a live block_broken event:
LCU_MAX_TICKS=5 ./build/dev-nobgfx/bin/VoxelServer --world TestWorld --port 25603
# Expect: "[example_mod] registered block ... -> id 2", "registered item
# ... -> id 1", "[example_mod] loaded", "Loaded 1 mod(s) from 'mods'".
SDL_VIDEODRIVER=dummy LCU_MAX_FRAMES=30 LCU_VERIFY_BREAK_PLACE=1 ./build/dev-nobgfx/bin/VoxelClient
# Expect the same registration lines, plus (right after "Breaking block
# at world (...)"): "[example_mod] block_broken #1: block id 1 broken at (...)"

# Exercises the real Phase 10 quality-profile chunk-load scaling:
LCU_MAX_TICKS=3 ./build/dev-nobgfx/bin/VoxelServer --world TestWorld --port 25604
# Expect: "Loaded 36 chunks" (Desktop, the default - unchanged from before Phase 10)
LCU_MAX_TICKS=3 LCU_QUALITY_PROFILE=mobile_low ./build/dev-nobgfx/bin/VoxelServer --world TestWorld --port 25605
# Expect: "Loaded 1 chunks"
LCU_MAX_TICKS=3 LCU_QUALITY_PROFILE=mobile_high ./build/dev-nobgfx/bin/VoxelServer --world TestWorld --port 25606
# Expect: "Loaded 27 chunks"

# Confirms the Android CMake preset is structurally sound (fails only at
# NDK detection, not a broken preset):
cmake --preset android-arm64
# Expect: "CMake Error ... Neither the NDK or a standalone toolchain was found."

# Exercises the real Phase 11 benchmark suite (opt-in, LCU_BUILD_TOOLS=ON;
# a Release build gives meaningful numbers - Development applies no
# optimization flags and Google Benchmark will warn about it):
cmake -S . -B build/release-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DLCU_ENABLE_BGFX=OFF -DLCU_BUILD_TOOLS=ON
cmake --build build/release-tools --target VoxelBenchmarks -j4
./build/release-tools/bin/VoxelBenchmarks --benchmark_min_time=0.2s
# Measured in this sandbox (4-core, 2.1GHz container), Release build:
#   BM_ChunkStorage_SetBlock                  1445 ns   2.84G items/s
#   BM_ChunkStorage_BlockAt                    600 ns   6.84G items/s
#   BM_Worldgen_GenerateTerrainChunk         17934 ns
#   BM_GreedyMesher_SolidChunk               94426 ns
#   BM_GreedyMesher_CheckerboardChunk      1445606 ns   (~15x the solid case - no face merging possible)
#   BM_Lighting_ComputeBlockLight            71538 ns
#   BM_Lighting_ComputeSkyLight                9564 ns
#   BM_Physics_Raycast                        1010 ns
#   BM_Physics_MoveAndCollide                  288 ns
#   BM_Serialization_SaveChunk               52578 ns
#   BM_Serialization_LoadChunk               20047 ns
#   BM_Network_ReliableOrderedRoundTrip        241 ns
#   BM_EntitySim_UpdateAiWander/1000         20125 ns   (49.7M entity-updates/s)

# Exercises the real Phase 12 audio + UI wiring end to end:
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_MAX_FRAMES=30 LCU_VERIFY_BREAK_PLACE=1 ./build/dev-nobgfx/bin/VoxelClient
# Expect: "AudioEngine initialized: 44100 Hz, stereo float", then the
# existing break/place log lines with no crash (play() calls happen
# but produce no audible/visible output in this sandbox).
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_MAX_FRAMES=30 LCU_VERIFY_BREAK_PLACE=1 ./build/dev-bgfx/bin/VoxelClient
# Same, plus draw_debug_overlay() runs every frame (bgfx Noop backend) -
# expect a clean run through to "LiveCraftUltimate client shutting down".

# Exercises the real Phase 13 block-edit-replication loop end to end -
# one server, two independent clients (one acting, one observing):
LCU_MAX_TICKS=400 ./build/dev-nobgfx/bin/VoxelServer --port 25710 &
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_CONNECT_PORT=25710 LCU_MAX_FRAMES=500000 LCU_VERIFY_BREAK_PLACE=1 ./build/dev-nobgfx/bin/VoxelClient
# Expect: "Requesting break...", "Picked up 1 game:stone...",
# "Requesting place...", "Applied server BlockChange at world (0, 28, -1):
# block_id=0" and "... (0, 29, -1): block_id=1".
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_CONNECT_PORT=25710 LCU_MAX_FRAMES=500000 ./build/dev-nobgfx/bin/VoxelClient
# A second client connecting only after the above already ran: expect
# it to log the identical "Applied server BlockChange" lines for both
# edits too (the server replays block_change_history on connect) - this
# is the actual proof of replication, not just that a message decodes.
# NOTE: the client loop is unthrottled - a purely observing client with
# no synthetic input needs a large LCU_MAX_FRAMES (hundreds of
# thousands) to still be running by the time the server processes a
# request; a small value makes the client exit before any traffic
# arrives, which looks like a failure but isn't one.

# Exercises the real Phase 14 chunk-network-streaming loop end to end -
# a 1-chunk world (mobile_low) then a 36-chunk world (desktop):
LCU_QUALITY_PROFILE=mobile_low LCU_MAX_TICKS=100 ./build/dev-nobgfx/bin/VoxelServer --port 25577 &
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_QUALITY_PROFILE=mobile_low LCU_CONNECT_PORT=25577 LCU_MAX_FRAMES=500000 ./build/dev-nobgfx/bin/VoxelClient
# Expect server: "Loaded 1 chunks", "Sent 1 chunk(s) (1 fragment(s)) to ...".
# Expect client: "Applied server ChunkData for chunk (0, 1, 0)".
LCU_QUALITY_PROFILE=desktop LCU_MAX_TICKS=150 ./build/dev-nobgfx/bin/VoxelServer --port 25578 &
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_QUALITY_PROFILE=desktop LCU_CONNECT_PORT=25578 LCU_MAX_FRAMES=500000 ./build/dev-nobgfx/bin/VoxelClient
# Expect server: "Loaded 36 chunks", "Sent 36 chunk(s) (36 fragment(s)) to ...".
# Expect client: 36 lines of "Applied server ChunkData for chunk (...)",
# one per loaded coordinate, with no WARN/ERROR in either log.

# Exercises the real Phase 15 server-side-inventory loop end to end:
LCU_MAX_TICKS=400 ./build/dev-nobgfx/bin/VoxelServer --port 25720 &
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_CONNECT_PORT=25720 LCU_MAX_FRAMES=500000 LCU_VERIFY_BREAK_PLACE=1 ./build/dev-nobgfx/bin/VoxelClient
# Expect (in order): "Picked up 1 game:stone (inventory: 1)", "Requesting
# place ... (inventory: 0)", "Reconciled inventory item 1 to authoritative
# count 1 (was 0)", "Applied server BlockChange ... block_id=0", "Reconciled
# inventory item 1 to authoritative count 0 (was 1)", "Applied server
# BlockChange ... block_id=1" - the optimistic client guess and the
# server's authoritative count converging after each round trip.

# Exercises the real Phase 16 per-movement-chunk-streaming loop end to
# end - a client walks far enough to cross a chunk boundary:
LCU_QUALITY_PROFILE=mobile_low LCU_MAX_TICKS=200 ./build/dev-nobgfx/bin/VoxelServer --port 25730 &
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_QUALITY_PROFILE=mobile_low LCU_CONNECT_PORT=25730 LCU_MAX_FRAMES=5000000 LCU_VERIFY_MOVE_SECONDS=6.0 ./build/dev-nobgfx/bin/VoxelClient
# Expect server: "Streamed 1 newly-loaded chunk(s) into range (total 2 loaded)".
# Expect client: "Applied server ChunkData for chunk (0, 1, -1)".
# Rerun with a second, purely stationary VoxelClient (no
# LCU_VERIFY_MOVE_SECONDS) connected alongside the mover - its log shows
# the identical "Applied server ChunkData for chunk (0, 1, -1)" line,
# confirming the broadcast reaches every connected client.

# Exercises the real Phase 17 surface/subsurface terrain content:
LCU_MAX_TICKS=60 ./build/dev-nobgfx/bin/VoxelServer --port 25741 &
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_CONNECT_PORT=25741 LCU_MAX_FRAMES=500000 ./build/dev-nobgfx/bin/VoxelClient
# Expect server: "Loaded 36 chunks", "Sent ... chunk(s) (... fragment(s))".
# Expect client: matching "Applied server ChunkData" lines and no
# WARN/ERROR - confirming a chunk now containing real grass/dirt/stone
# layering (not just stone) serializes, streams, and applies correctly.

# Exercises the real Phase 18 grass/dirt item-mapping loop end to end -
# the player spawns standing on grass, so LCU_VERIFY_BREAK_PLACE now
# naturally breaks a grass block instead of stone:
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_MAX_FRAMES=10 LCU_VERIFY_BREAK_PLACE=1 ./build/dev-nobgfx/bin/VoxelClient
# Expect: "Breaking block at world (0, 28, -1)" then "Picked up 1
# game:grass (inventory: 1)".
LCU_MAX_TICKS=200 ./build/dev-nobgfx/bin/VoxelServer --port 25750 &
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LCU_CONNECT_PORT=25750 LCU_MAX_FRAMES=500000 LCU_VERIFY_BREAK_PLACE=1 ./build/dev-nobgfx/bin/VoxelClient
# Expect server: "Applied BlockAction from <addr>: (0,28,-1) 2 -> 0".
# Expect client: "Requesting break...", "Picked up 1 game:grass
# (inventory: 1)", "Applied server BlockChange ... block_id=0".

# Full build with bgfx (default; see BUILDING.md for required system packages):
cmake -S . -B build/dev-bgfx -G Ninja -DCMAKE_BUILD_TYPE=Development
cmake --build build/dev-bgfx -j4
ctest --test-dir build/dev-bgfx --output-on-failure
SDL_VIDEODRIVER=dummy LCU_MAX_FRAMES=5 ./build/dev-bgfx/bin/VoxelClient
```

`LCU_ENABLE_BGFX=OFF` is an escape hatch for a fast non-graphics build
while iterating; the default and the one that must keep working going
forward is `LCU_ENABLE_BGFX=ON`. Both configurations are currently
verified in this sandbox (see the table above) — headlessly. Real-display
verification of the bgfx build is still outstanding.
