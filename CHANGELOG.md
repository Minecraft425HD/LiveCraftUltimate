# Changelog

All notable changes to this project are recorded here, newest first.

## Unreleased — Phase 0 / Phase 1 / Phase 2 / Phase 3 / Phase 4

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
