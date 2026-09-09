# Changelog

All notable changes to this project are recorded here, newest first.

## Unreleased — Phase 0 / Phase 1

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
