# Changelog

All notable changes to this project are recorded here, newest first.

## Unreleased — Phase 0 / Phase 1 / Phase 2 / Phase 3 / Phase 4 / Phase 5 / Phase 6 / Phase 7 / Phase 8 / Phase 9

### Phase 9

- Added Lua 5.4.7 as a build dependency: official upstream
  (`github.com/lua/lua`, which ships no CMake support) fetched via
  `FetchContent_Populate`, built from its own `onelua.c` amalgamation
  with `-DMAKE_LIB` to produce just the embeddable library (no `main()`).
  Root `CMakeLists.txt` now declares `LANGUAGES CXX C` for it.
- `engine/scripting::LuaState`: RAII wrapper around one Lua VM. Opens
  only the base/table/string/math standard libraries - not `io`/`os`/
  `package` - so a mod script has no filesystem/process access by
  default. Forward-declares `lua_State` so `<lua.h>` is only ever
  `#include`d inside `engine/scripting`'s and `engine/modding`'s own
  `.cpp` files (mirrors the existing bgfx-header-confinement rule).
  `register_function` exposes a native C function to Lua's global
  namespace with a stateful `void*` upvalue, the standard technique for
  binding a C++ object to Lua's C-style callback ABI. 7 new unit tests.
- `engine/modding::EventBus`: a named pub/sub bus - mod scripts call
  `lcu.subscribe(event_name, fn)`, the engine calls a typed
  `emit_<event>()` method (currently just `emit_block_broken`) at the
  real moment that event happens. An erroring handler is logged and
  skipped without blocking the remaining subscribers. 7 new unit tests.
- `engine/modding::{bind_block_registry, bind_item_registry}`: expose
  `register_block(namespaced_id, display_name, is_transparent,
  has_collision)`/`register_item(namespaced_id, display_name,
  max_stack_size)` to Lua, writing directly into the given
  `BlockRegistry`/`ItemRegistry` - mod content and base game content are
  otherwise indistinguishable. 6 new unit tests.
- `engine/modding::ModLoader`: enumerates immediate subdirectories of a
  mods directory, running each one's fixed `<mod>/init.lua` entry point
  against one shared `LuaState`. A mod without an `init.lua`, or whose
  script errors, is logged and skipped - not fatal to the others.
  Deliberately no manifest/dependency/version format yet. 6 new unit
  tests.
- `mods/example_mod/init.lua`: a real, working demonstration mod -
  registers `example_mod:magic_stone`/`example_mod:magic_wand`,
  subscribes to `block_broken`, and logs every block it sees broken.
- `VoxelClient`/`VoxelServer`: both now construct their own `LuaState`,
  bind their own block/item registries, and call
  `ModLoader::load_all("mods")` at startup (guarded by the new
  `LCU_ENABLE_SCRIPTING` compile definition, on by default via
  `LCU_BUILD_SCRIPTING`). `VoxelClient` fires a real
  `EventBus::emit_block_broken()` at the exact point in the existing
  break-handling code where a block actually becomes air.
  `VoxelServer` also constructs an `EventBus`/`ItemRegistry` purely so a
  mod script shared between both hosts has a uniform Lua API surface,
  even though the server never itself calls `emit_block_broken` (block
  edits aren't replicated yet).
- Verified via real runs, not just unit tests: `VoxelServer` logs its
  mod's registrations and `Loaded 1 mod(s) from 'mods'`; `VoxelClient`
  under `LCU_VERIFY_BREAK_PLACE` additionally logs `[example_mod]
  block_broken #1: block id 1 broken at (0, 28, -1)` immediately after
  breaking that exact block.
- `ctest` 274/274 passing (bgfx build) / 271/271 (non-bgfx build), up
  from 247/247 / 244/244 - 27 new tests across `LuaState`, `EventBus`,
  `RegistryBindings`, `ModLoader`.

### Phase 8

- `engine/replication::PositionInterpolator`: buffers timestamped
  position samples and linearly interpolates between them for a
  slightly-delayed render time, clamping (never extrapolating) past
  either end of the buffer. 9 new unit tests.
- `engine/replication::PredictionBuffer<State, Input>`: generic
  client-side prediction + server reconciliation - `predict_and_record`
  applies an input immediately and remembers it; `reconcile` discards
  acknowledged history and replays what's left on top of an
  authoritative correction. Generic over any pure step function, not
  tied to player movement. 6 new unit tests, including one wiring the
  real `lcu::physics::PlayerPhysicsState`/`integrate_player` as the
  concrete step function with a hand-computed expected result.
- `game::systems::protocol`: the shared application-level messages
  `VoxelClient` and `VoxelServer` both use now
  (`Welcome`/`Heartbeat`/`EntityState`/`PlayerInput`/`PlayerCorrection`),
  replacing `VoxelServer`'s own local copies from Phase 7 - one
  definition instead of two that could silently drift apart. 11 new
  unit tests (round-trips, negative floats, malformed-payload
  rejection).
- `VoxelServer`: tracks one real `lcu::physics::PlayerPhysicsState` per
  connected client now, driven by received `PlayerInput` messages
  through the same physics `VoxelClient` runs (server-authoritative
  movement, not an echo), with a `dt` ceiling clamp as a light
  anti-cheat measure. Broadcasts a per-client `EntityState` filtered by
  a real interest-management distance check (`kInterestRadius`), and a
  periodic `PlayerCorrection`.
- `VoxelClient`: new `LCU_CONNECT_PORT` env var enables a real networked
  mode (loopback IPv4 only) alongside the existing single-player path,
  which is completely unaffected when unset. When networked: connects,
  logs the real `Welcome`; predicts local player movement immediately
  via `PredictionBuffer` and reconciles against `PlayerCorrection`;
  stops simulating AI locally and instead renders each remote entity's
  `EntityState` samples through its own `PositionInterpolator`.
- Verified via a real two-process run: an actual `VoxelClient` connected
  to an actual `VoxelServer` over real loopback UDP, received a genuine
  Welcome (`world_seed=1337 tick_rate=20`), rendered all 3 remote AI
  entities' interpolated positions matching the server's live
  simulation, and had its player position predicted, sent, and
  reconciled - the full loop exercised end to end, not simulated.
  Single-player mode reverified byte-for-byte unchanged in both build
  configs.
- New `NETWORKING.md` sections documenting the replication protocol,
  prediction/reconciliation flow, interest management, and what's
  deferred (chunk streaming - needs message fragmentation `Connection`
  doesn't have; block-edit replication).
- 26 new unit tests. `VoxelTests` now at 247/247 passing (bgfx build) /
  244/244 (non-bgfx build).

### Phase 7

- `engine/network::UdpSocket`: cross-platform (POSIX/Winsock, selected
  at compile time) non-blocking IPv4 UDP socket wrapper - `bind`/
  `send_to`/`try_receive`. Winsock startup/cleanup is reference-counted
  so callers never have to think about it. 9 new unit tests incl. a real
  loopback send/receive round-trip.
- `engine/network::{Channel, PacketHeader, sequence_greater_than}`: the
  pure, independently-tested building blocks. `Channel` names all four
  semantics `ARCHITECTURE.md` commits to
  (`UnreliableUnordered`/`UnreliableSequenced`/`ReliableUnordered`/
  `ReliableOrdered`); `PacketHeader` is a 4-byte wire header
  (type/sequence/channel) with round-trip serialization;
  `sequence_greater_than` is the standard wraparound-correct `u16`
  sequence comparison (the same technique TCP uses for its own
  sequence numbers).
- `engine/network::Connection`: implements all four channel semantics
  over an abstract byte-packet transport - it never touches a socket
  itself, only produces/consumes raw packets (the same
  dependency-injection shape as `physics::raycast`'s `is_solid`
  predicate), so the protocol logic (ordering, deduplication,
  retransmission timing) is fully unit-tested with zero real I/O.
  Reliable channels use per-channel sequence counters and ack-based
  retransmission (fixed interval - see DECISIONS.md); `ReliableOrdered`
  additionally buffers out-of-order arrivals and drains them in
  sequence. See the new `NETWORKING.md` for the full wire format.
- 46 new unit/integration tests, including two full loopback integration
  tests running real `Connection` pairs over real `UdpSocket`s on
  `127.0.0.1` - one deliberately drops the first real UDP datagram sent
  and confirms retransmission recovers it, not a hypothetical case a
  mock would assume away. `VoxelTests` now at 221/221 passing (bgfx
  build) / 218/218 (non-bgfx build).
- `VoxelServer`: replaced the Phase 0 sleep-only placeholder tick loop
  with a real one. Generates/loads a real 36-chunk `World`, runs the
  same wandering-AI simulation as `VoxelClient` (`engine/ecs` +
  `game::systems::update_ai_wander`) every tick regardless of whether
  any client is connected, and listens for real UDP connections - a
  peer is "connected" the moment the server sees any datagram from its
  address, and gets a real `ReliableOrdered` Welcome message (world
  seed + tick rate) plus a per-tick `UnreliableSequenced` Heartbeat
  (tick number + live entity count) from then on. Verified via a real
  two-process test: a standalone Python UDP client connects to a
  running `VoxelServer` and receives the genuine handshake and live
  heartbeats (see `NETWORKING.md`/`BUILD_STATUS.md` for the exact
  reproduce steps). `ldd` reconfirmed zero SDL/bgfx dependency.
- New `NETWORKING.md`: wire format, channel semantics, ack/retransmit
  behavior, the application-level Welcome/Heartbeat message format, and
  what's verified vs. deferred.

### Phase 6

- `engine/ecs::Registry`: generation-checked `EntityId` handles (a stale
  handle from a destroyed entity never aliases whatever later reuses its
  slot) and sparse-set `ComponentPool<T>` per component type (dense
  contiguous storage for cache-friendly iteration, swap-and-pop removal
  so dense arrays never develop holes). `create`/`destroy_entity`,
  `add`/`get`/`has`/`remove_component`, `pool_for<T>()` for dense
  iteration over every live component of a type. No query DSL,
  archetypes, or multithreaded system dispatch - not needed yet. 13 new
  unit tests.
- `engine/lighting::LightStorage<EdgeLength>`: packed 4-bit sky + 4-bit
  block light per voxel, same flat-array layout as `ChunkStorage`.
  `compute_block_light`/`compute_sky_light`: the one-time initial
  per-chunk flood (from every `BlockDefinition::light_emission` source,
  and a top-down per-column sky fill). `propagate_added_block_light`/
  `unpropagate_block_light`: true incremental local updates for a single
  block add/remove - the standard two-phase BFS removal algorithm
  (darken everything strictly dimmer than the retracted source, collect
  still-validly-lit boundary cells, re-flood from them), directly
  satisfying brief section 24's "local updates, not full recompute"
  rather than re-flooding the whole chunk per edit. Header-only
  (templated on edge length, like `mesh_chunk_greedy`). Single-chunk
  scope for now (no cross-chunk light bleed) - see DECISIONS.md. 12 new
  unit tests, including an exact-match check between the incremental add
  path and a full recompute, and a two-source removal test verifying the
  refill phase correctly reproduces what a solo-source recompute would
  give.
- `game::components::{Position, AIWander}` and
  `game::systems::update_ai_wander`: a real gameplay-layer `engine/ecs`
  consumer. An entity with both components idles for a random duration,
  then walks toward `AIWander::target` at `AIWander::speed`; on arrival,
  picks a new target within a configurable radius and idles again. An
  explicit `std::mt19937` (not a hidden global RNG) keeps this
  deterministic and testable, matching `worldgen`'s "no hidden global
  state" approach. 6 new unit tests.
- `game::systems::DayNightCycle`: tracks elapsed time through a
  repeating cycle and reports a cosine-curve sky light scale (1.0 at
  noon, a dim nonzero floor at midnight, never fully black). Not yet
  wired into any renderer or `engine/lighting` data - logged only for
  now. 7 new unit tests.
- `VoxelClient`: computes real per-chunk block+sky light at load time
  (`chunk_light`, a `ChunkCoord -> Light` map alongside the existing GPU
  mesh map) and keeps it correct through every break/place edit via the
  incremental propagate/unpropagate primitives plus a per-column sky
  light refresh, instead of re-flooding the whole chunk on every edit.
  Spawns 3 wandering AI entities in a ring around spawn and a
  `DayNightCycle`, both updated every frame. Also fixes a real
  off-by-one found while verifying this: `terrain_height()` returns the
  topmost *solid* block's Y (worldgen.cpp: `world_y <= height` is
  solid), so the player (and now AI) previously spawned with feet
  embedded one block into the ground instead of resting on top of it -
  spawn Y is now `terrain_height() + 1`.
- `VoxelTests` now at 187/187 passing (bgfx build) / 184/184 (non-bgfx
  build). Verified via a real headless run: "Sky light 5 blocks above
  spawn column: 15", real (deterministic, seeded) AI entity positions
  logged, "Day/night: time_of_day=0.000 sky_light_scale=0.550", and the
  existing `LCU_VERIFY_BREAK_PLACE` break-then-place round-trip still
  holds after the spawn-height fix.

### Phase 5

- `engine/items::{ItemRegistry, ItemDefinition, ItemId, kNoItemId}`:
  namespaced, datadriven item registry mirroring `engine/voxel`'s
  `BlockRegistry`/`BlockDefinition` pattern - `kNoItemId` (0) is always
  "no item", auto-registered by the constructor. 5 unit tests.
- `engine/items::{ItemStack, Inventory}`: fixed-size slot-based item
  storage. `add_item` tops up existing matching partial stacks before
  spilling into empty slots, respecting each item's
  `ItemDefinition::max_stack_size`, and returns any leftover that
  didn't fit; `remove_item`/`count_item` round it out. No UI/drag-drop
  yet - no inventory screen exists to need one. 10 unit tests.
- `engine/items::{RecipeRegistry, ShapedRecipe, ShapelessRecipe}`:
  shaped recipes matched by trimming the *queried* crafting grid to its
  bounding box and comparing cell-for-cell at a single orientation (no
  mirroring - a documented simplification, no recipe has needed it
  yet); shapeless recipes matched by exact ingredient-multiset
  comparison (extra unrelated items in the grid correctly fail to
  match, same as real crafting games). No crafting-UI caller yet -
  tested standalone, same as `BlockRegistry`/`ItemRegistry` were before
  their first real callers existed. 9 unit tests.
- `VoxelClient`: block-break now has a real item consumer. Breaking
  registers and drops one `game:stone` item into a new 9-slot player
  `Inventory`; placing now consumes one stone item instead of being
  free, refunding it if the placement target's chunk turns out not to
  be loaded (a real edge case found and fixed while wiring this up).
  Verified via the existing `LCU_VERIFY_BREAK_PLACE` headless hook:
  break logs "Picked up 1 game:stone (inventory: 1)", place logs
  "... (inventory: 0)".
- 24 new unit tests across `ItemRegistry`/`Inventory`/`RecipeRegistry`.
  `VoxelTests` now at 149/149 passing (bgfx build) / 146/146 (non-bgfx
  build).

### Phase 4

- `engine/physics::raycast`: voxel DDA (Amanatides & Woo) against
  `World`, stepping one voxel boundary at a time regardless of chunk
  size, predicate-driven solidity. 9 unit tests incl. a hand-computed
  exact-distance case and the origin-starts-inside-solid edge case.
- `engine/physics::{AABB, move_and_collide, PlayerPhysicsState,
  integrate_player}`: axis-independent Y->X->Z AABB-vs-voxel collision
  resolution, gravity, jump, and single-ledge auto-stepping. Found and
  fixed a real grounding-detection bug during development (a stationary
  grounded player briefly reported ungrounded because "grounded" was
  read off whether *this frame's* downward movement collided, not
  whether the player was actually resting on something) via a dedicated
  small downward ground-probe, decoupling the two - see DECISIONS.md.
  18 unit tests, including a regression test for that exact bug and
  hand-computed auto-step clamp positions.
- `engine/player::{FirstPersonCamera, movement_direction_from_input}`:
  yaw/pitch first-person camera matching the existing `Mat4::look_at`
  -Z-forward convention (pitch clamped just under the poles), and
  WASD-relative normalized movement direction decoupled from pitch.
  12 unit tests.
- `engine/platform::Action` gained `LookUp/Down/Left/Right` (arrow keys)
  and `PlaceBlock` (`F`) - arrow-key look is a real, immediately usable
  interim control scheme standing in for mouse-look until SDL
  relative-mouse-mode plumbing exists (see DECISIONS.md).
- `VoxelClient` rewritten from Phase 2's single hardcoded placeholder
  chunk to a real vertical slice: loads a 36-chunk area of `World`-
  driven terrain around spawn, spawns a physics-driven player resting
  on the generated surface, drives the camera from arrow-key look input
  and WASD movement through `integrate_player`, raycasts from the
  camera every frame, and mutates the world on edge-detected Interact
  (break, `E`)/PlaceBlock (place, `F`) presses - remeshing and
  re-uploading not just the edited chunk but any neighbor chunk sharing
  the mutated block's boundary, so cross-chunk face culling stays
  correct after an edit at a chunk seam.
- New `LCU_VERIFY_BREAK_PLACE` env var: since this sandbox has no real
  keyboard/mouse, synthesizes an Interact press at frame 3 and a
  PlaceBlock press at frame 6, driving the exact same edge-detected
  `InputState` code path a real key press would. Verified via a real
  run: breaks a block, logs it, then the next raycast (now reaching one
  block deeper) places a new block back at the exact same world
  position - a genuine round-trip through mutate-world -> remesh ->
  re-upload, not a mock of it. This closes brief section 80's slice 1
  vertical slice (save/load already existed from Phase 3; persisting a
  live session's edits to disk still has no trigger wired up - see
  PROJECT_STATE.md "Known Limitations").
- 39 new unit tests across `Raycast`/`Collision`/`PlayerPhysics`/
  `FirstPersonCamera`/`MovementInput`. `VoxelTests` now at 125/125
  passing (bgfx build) / 122/122 (non-bgfx build).

### Phase 3

- `engine/world::World`: sparse chunk table keyed by `ChunkCoord`,
  lifecycle state machine (Unloaded -> Requested -> Generating ->
  Generated, matching ARCHITECTURE.md), distance-based streaming with
  load/unload radius hysteresis.
- `engine/world::worldgen`: deterministic seeded value-noise terrain
  height (continental+terrain pipeline stage only). Same seed+coord
  always produces the same height; different seeds differ; adjacent
  columns change smoothly.
- `engine/serialization::chunk_serializer`: versioned, zstd-compressed
  chunk save/load with corruption detection (zstd content checksum) and
  version-mismatch detection - new dependency, zstd v1.5.7 (see
  DECISIONS.md/third_party/README.md).
- `std::hash<ChunkCoord>` added so it can key `World`'s chunk table.
- 27 new unit tests across `World`/`worldgen`/`chunk_serializer`,
  including exhaustive round-trip and corruption-detection coverage for
  save/load. `VoxelTests` now at 88/88 passing (bgfx build) / 85/85
  (non-bgfx build).

### Phase 0

- Repository structure created (`engine/`, `game/`, `client/`, `server/`,
  `tools/`, `third_party/`, `mods/`, `examples/server/`, `tests/`).
- Governance docs added: `PROJECT_STATE.md`, `ROADMAP.md`,
  `TASK_QUEUE.md`, `BUILD_STATUS.md`, `BUILDING.md`, `ARCHITECTURE.md`,
  `DECISIONS.md`, `CHANGELOG.md`.
- CMake build system: root `CMakeLists.txt`, `CMakePresets.json`
  (linux/windows/macos/android-arm64/ios), `third_party/CMakeLists.txt`
  fetching fmt, SDL3, GoogleTest and bgfx.cmake via pinned FetchContent.
- `engine/core` (types, fmt-backed logging, assertions) and `engine/math`
  (Vec3/Vec4/Mat4), both unit tested.
- `VoxelServer`: headless dedicated server entry point, verified via
  `ldd` to carry zero SDL/bgfx dependency.
- `VoxelTests`: 12 GoogleTest cases (Log/Vec3/Mat4), all passing.

### Phase 1 (in progress)

- `engine/platform::Window`: SDL3-backed window + event pump. Verified
  headlessly (`SDL_VIDEODRIVER=dummy`) via `VoxelClient`.
- `engine/platform::get_native_window_handle`: extracts the platform
  native window handle from SDL3 (X11/Wayland/Win32/Cocoa/UIKit/Android),
  returning null when unavailable (e.g. dummy driver).
- `engine/rendering::Renderer`: bgfx init/frame/shutdown wrapper. Falls
  back to bgfx's `Noop` backend when no native handle is available.
  Verified headlessly: 5-frame clear loop completes cleanly against the
  `Noop` backend. Real GPU backend rendering to an actual screen is not
  yet verified (no GPU/display in the dev sandbox).
- `engine/platform::InputState`/`KeyboardInputBackend`: action-based
  input (`MoveForward`/`Jump`/`Interact`/...) decoupled from raw SDL
  scancodes. Keyboard backend only; gamepad/touch deferred to when
  something needs them (Phase 4/10).
- `engine/debug::FrameStats`: minimal FPS/frame-time accumulator, wired
  into `VoxelClient`'s loop as a once-per-second log line. Verified with
  a real running loop, not just unit tests.
- `VoxelTests` now at 18/18 passing (added `InputState.*`,
  `FrameStats.*`).

### Phase 2 (in progress)

- `engine/voxel::ChunkStorage<EdgeLength>`/`Chunk`: flat, contiguous
  `BlockId` array, no per-block C++ instance, default 16^3, alternative
  sizes proven via `ChunkStorage<8>`.
- `engine/voxel::world_to_chunk_and_local`: correct floor-division
  world->chunk+local coordinate splitting (handles negative coordinates
  correctly, unlike naive truncating division).
- `engine/voxel::BlockRegistry`/`BlockDefinition`: namespaced, datadriven
  block definitions (`game:stone`, `example_mod:magic_stone`); air always
  id 0.
- `VoxelTests` now at 39/39 passing (added `Chunk.*`, `ChunkStorage.*`,
  `ChunkCoord.*`, `BlockRegistry.*` - 27 new cases, including an
  exhaustive chunk-volume injectivity sweep and a coordinate-math
  round-trip sweep).
- `engine/jobs::JobSystem`: worker-thread pool with priority scheduling,
  dependency graphs (with cascading cancellation), and cancellation of
  not-yet-started jobs. Required before greedy meshing can run off the
  main thread. 12 new unit tests; `VoxelTests` now at 51/51 passing.
  Additionally verified via 200 repeated test-suite runs and 50 runs
  under ThreadSanitizer, zero failures/data races either way.
- `engine/voxel::mesh_chunk_greedy`: axis-sweep greedy meshing producing
  a renderer-agnostic `ChunkMesh` (opaque layer; transparent/water
  layers exist structurally, populated once a transparent block exists
  to motivate their face rules). Registry-driven opacity. Triangle
  winding verified via a geometric cross-product check against each
  triangle's stored normal. 8 new unit tests; `VoxelTests` now at 59/59
  passing.
- `engine/rendering::upload_chunk_mesh_layer`/`destroy_gpu_chunk_mesh`:
  real bgfx `VertexBuffer`/`IndexBuffer` creation from a `ChunkMeshLayer`
  (`Lcu::Rendering` now links bgfx `PUBLIC` instead of `PRIVATE`, since
  this header exposes bgfx types). No shader/draw-call yet - bgfx needs
  a compiled shader program to draw anything, and no shader compiler is
  built in this repo; documented as the explicit next step. 3 new unit
  tests.
- `VoxelClient` now exercises the full Phase 2 pipeline end-to-end:
  registers a placeholder `"game:stone"` block, builds a flat ground
  slab `Chunk`, dispatches `mesh_chunk_greedy` through
  `engine/jobs::JobSystem` (its first real caller), and uploads the
  result to GPU buffers. Verified via a real headless run: "Meshed
  placeholder chunk: opaque 24 vertices / 36 indices" then "Uploaded
  chunk mesh to GPU buffers: valid=true index_count=36". `VoxelTests`
  now at 62/62 passing (bgfx build) / 59/59 (non-bgfx build).
- `LCU_BUILD_SHADER_TOOLS` (opt-in, default OFF): builds bgfx's
  `shaderc` and compiles `client/shaders/{vs_chunk,fs_chunk}.sc` (a
  minimal directional+ambient lit shader, no texturing yet) into
  spirv/glsl/essl binaries. `engine/rendering::load_chunk_program` loads
  them at runtime; `Renderer` gained `begin_frame`/`submit_chunk_mesh`/
  `end_frame` (replacing `render_clear_frame`) so a real
  `bgfx::submit()` draw call happens between clear and frame advance.
  Verified via a real `VoxelClient` run: "Chunk shader program
  valid=true" followed by 3 clean frames with the draw call executing
  under bgfx's `Noop` backend - the full chunk -> mesh -> GPU buffers ->
  shader -> draw call pipeline now runs end to end. What it looks like
  on a real GPU/display remains unverified (no display in this sandbox).
