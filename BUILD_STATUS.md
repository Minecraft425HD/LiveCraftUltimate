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
| `VoxelClient` | **TESTED** (headless) / **UNTESTED** (visual) | Runs end-to-end under dummy SDL driver, `LCU_MAX_FRAMES` env var bounds the loop for CI. No renderer wired up yet (bgfx integration in progress, see below) — window is currently blank/unrendered even when a real display is available. |
| `VoxelServer` | **TESTED** | Runs standalone, parses `--world`/`--port`, ticks at 20 TPS, exits cleanly via `LCU_MAX_TICKS`. `ldd` confirms **zero** SDL/bgfx link dependency (only libc/libstdc++/libm/libgcc_s). |
| `VoxelTests` (GoogleTest) | **TESTED** | 62/62 tests pass (bgfx build) / 59/59 (non-bgfx build, `ChunkMeshUpload.*` excluded): `Log.*`, `Vec3.*`, `Mat4.*`, `FrameStats.*`, `InputState.*`, `Chunk.*`, `ChunkStorage.*`, `ChunkCoord.*`, `BlockRegistry.*`, `GreedyMesher.*`, `JobSystem.*`, `ChunkMeshUpload.*`. |
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
