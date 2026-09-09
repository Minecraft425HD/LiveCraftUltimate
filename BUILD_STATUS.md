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
| `VoxelClient` | **TESTED** (headless) / **UNTESTED** (visual) | Loads a real 36-chunk `World` around spawn, spawns a physics-driven player, and runs the full camera/movement/raycast/break-place loop under dummy SDL driver. `LCU_MAX_FRAMES` bounds the loop for CI; `LCU_VERIFY_BREAK_PLACE` synthesizes a break-then-place input sequence for headless verification (no real keyboard in this sandbox) - confirmed via a real run: breaks a block, then places a new one back at the exact same world position on the next frame's raycast. **Not** visually verified (no GPU/display here). |
| `VoxelServer` | **TESTED** | Runs standalone, parses `--world`/`--port`, ticks at 20 TPS, exits cleanly via `LCU_MAX_TICKS`. `ldd` confirms **zero** SDL/bgfx link dependency (only libc/libstdc++/libm/libgcc_s). |
| `VoxelTests` (GoogleTest) | **TESTED** | 125/125 tests pass (bgfx build) / 122/122 (non-bgfx build, `ChunkMeshUpload.*` excluded): `Log.*`, `Vec3.*`, `Mat4.*`, `FrameStats.*`, `InputState.*`, `Chunk.*`, `ChunkStorage.*`, `ChunkCoord.*`, `BlockRegistry.*`, `GreedyMesher.*`, `JobSystem.*`, `ChunkMeshUpload.*`, `World.*`, `Worldgen.*`, `ChunkSerializer.*`, `Raycast.*`, `AABB.*`/`Collision.*`/`PlayerPhysics.*`, `FirstPersonCamera.*`, `MovementInput.*`. |
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
