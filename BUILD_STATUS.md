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
| `VoxelClient`<->`VoxelServer` real multiplayer | **TESTED** (two-process, loopback) | `LCU_CONNECT_PORT=<port> ./VoxelClient` against a running `VoxelServer` on the same port: client connects, receives a genuine Welcome (`world_seed=1337 tick_rate=20`), renders all 3 remote AI entities via real `PositionInterpolator` interpolation (not local simulation), predicts local player movement via `PredictionBuffer`, sends `PlayerInput`, and reconciles against the server's `PlayerCorrection` - confirmed via a real run: "Received Welcome: world_seed=1337 tick_rate=20", "Remote AI entity (index=1) interpolated position: (4.00, 29.00, 0.00)" (matching the server's own live spawn position), "Player position (server-reconciled): (-0.30, 29.00, -0.30)". Verified in both build configs. **Not** verified: multiple simultaneous clients, a non-loopback network, or any visual rendering of the above. |
| `VoxelTests` (GoogleTest) | **TESTED** | 247/247 tests pass (bgfx build) / 244/244 (non-bgfx build, `ChunkMeshUpload.*` excluded): `Log.*`, `Vec3.*`, `Mat4.*`, `FrameStats.*`, `InputState.*`, `Chunk.*`, `ChunkStorage.*`, `ChunkCoord.*`, `BlockRegistry.*`, `GreedyMesher.*`, `JobSystem.*`, `ChunkMeshUpload.*`, `World.*`, `Worldgen.*`, `ChunkSerializer.*`, `Raycast.*`, `AABB.*`/`Collision.*`/`PlayerPhysics.*`, `FirstPersonCamera.*`, `MovementInput.*`, `ItemRegistry.*`, `Inventory.*`, `RecipeRegistry.*`, `Registry.*`, `ComputeBlockLight.*`/`PropagateAddedBlockLight.*`/`UnpropagateBlockLight.*`/`ComputeSkyLight.*`/`LightStorage.*`, `AIWanderSystem.*`, `DayNightCycle.*`, `Sequence.*`, `PacketHeader.*`, `Connection.*`, `UdpSocket.*`, `Address.*`, `LoopbackIntegration.*`, `PositionInterpolator.*`, `PredictionBuffer.*`, `ReplicationProtocol.*`. |
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
| `engine/world::worldgen` | **TESTED** | Deterministic seeded value-noise terrain height (continental+terrain only). 7 unit tests: determinism, cross-seed variation, bounded range, column-to-column smoothness, and chunk-fill correctness incl. all-air and all-solid extremes. |
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
| Lua scripting | **NOT STARTED** | Phase 9. |
| Networking | **NOT STARTED** | Phase 7. |
| Android build (`CMakePresets.json` `android-arm64`) | **BLOCKED here** | No Android NDK installed in this sandbox; preset requires `ANDROID_NDK_HOME`. Untested, not un-buildable — needs a machine/CI runner with the NDK. |
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
