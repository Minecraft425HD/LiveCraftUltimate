# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, in that
order, before touching code. The repository is the source of truth, not
this file's prose if the two disagree — if in doubt, run the build and
tests and trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

Phase 0 complete. Phase 1 functionally complete for what this headless
sandbox can verify. Phase 2 (voxel storage + chunk + meshing +
rendering) functionally complete for what this sandbox can verify: chunk
storage, coordinate math, BlockRegistry, job system, greedy meshing, GPU
buffer upload, shader compilation, and a real `bgfx::submit()` draw call
are all done, tested, and wired together end-to-end in `VoxelClient`.
What's *not* verified is what any of it looks like - no GPU/display
exists in this sandbox.

## Current Task

None in flight. Phase 2 is functionally done for this environment; next
up per `TASK_QUEUE.md` is choosing between Phase 3 world
storage/streaming or Phase 4 physics/camera as the next foundational
piece (leaning toward world storage first, since physics/raycast need
more than one hardcoded chunk to be meaningful) - record the choice in
`DECISIONS.md` when starting.

## Last Completed Task

Got a real bgfx draw call working, opt-in via `LCU_BUILD_SHADER_TOOLS`
(builds bgfx's `shaderc` - confirmed feasible first via a scratch-build
probe before committing to it, ~700 extra build steps pulling in
glslang/SPIRV-Tools/SPIRV-Cross/Dawn-Tint). Added minimal
`client/shaders/{vs_chunk,fs_chunk}.sc` (directional+ambient lighting,
no texturing - no atlas yet), compiled via bgfx.cmake's
`bgfx_compile_shaders()` into spirv/glsl/essl binaries, loaded at
runtime by the new `engine/rendering::load_chunk_program` (profile
selected by active bgfx renderer type). Split `Renderer::render_clear_frame`
into `begin_frame`/`submit_chunk_mesh`/`end_frame` so a draw call can be
submitted between clear and frame advance.

The first attempt at this was committed as WIP (per repo policy on
uncommitted changes, while a long background build was still running)
and had two real bugs the actual build then caught: a missing
`lcu/core/types.h` include (`u8`/`u32`/`usize` undeclared), and a
`VARYING_DEF` path passed relative to `client/` when
`bgfx_compile_shaders()`'s generated custom command actually runs with
the *build* directory as its working directory (silently produced
confusing HLSL-parser errors, not a "file not found"). Both fixed in a
follow-up commit, then verified for real: full rebuild produces all 6
shader binaries, `ctest` 62/62 passing, and a real `VoxelClient` run
logs `"Chunk shader program valid=true"` followed by 3 clean frames with
the draw call actually executing via `bgfx::submit()`, under the `Noop`
backend. Non-bgfx build (59/59 ctest) and `VoxelServer` (still
SDL/bgfx-free per `ldd`) both confirmed unaffected.

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 62/62 passing (this build dir is now
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 59/59 passing
(`ChunkMeshUpload.*` only exists in the bgfx build, since it needs a
real bgfx context). Covers Log, Vec3, Mat4,
FrameStats, InputState, Chunk, ChunkStorage, ChunkCoord, BlockRegistry,
GreedyMesher, JobSystem, ChunkMeshUpload. JobSystem additionally verified
via 200 repeated `ctest`-suite runs and 50 runs under ThreadSanitizer,
zero failures/races - see `BUILDING.md` "Testing under ThreadSanitizer"
for the exact commands. GreedyMesher's triangle winding is verified via
a geometric cross-product check, not just vertex counts. No integration
tests yet (no networking/save system exists yet to integration-test).

## Known Bugs

None currently tracked.

## Known Limitations

- `VoxelClient` draws its one hardcoded placeholder chunk with a fixed
  camera — no player/camera control, no visual verification possible
  (no GPU/display here). Real content and an actual camera come with
  Phase 3/4.
- No world, no gameplay of any kind yet — intentionally still
  pre-vertical-slice (see `ROADMAP.md` "Vertical slice targets"). Zero
  blocks are registered anywhere outside unit tests.
- `VoxelServer`'s tick loop is a placeholder (sleeps at 20 TPS, no actual
  simulation) until Phase 7.
- bgfx's real GPU backend (Vulkan/GL/Metal/D3D) selection is untested —
  only the `Noop` headless fallback has been exercised, since this sandbox
  has no GPU/display.
- Mobile/Windows/macOS builds are untested from this Linux-only sandbox;
  `CMakePresets.json` presets exist for them but have not been exercised
  on their native toolchains.
- Input abstraction covers keyboard only (`KeyboardInputBackend`); no
  mouse-look, gamepad or touch backend yet — none has a consumer to drive
  until a camera/player exists (Phase 4) or mobile work starts (Phase 10).
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
- No texture atlas/UV mapping validation — `MeshVertex.u`/`.v` are
  populated (quad-local, in block units) but nothing downstream
  consumes or checks them yet, since there's no atlas (Phase 12).
- Shader compilation (`LCU_BUILD_SHADER_TOOLS`) is opt-in and OFF by
  default — most builds/CI runs won't have a real draw call unless this
  is explicitly turned on, since it adds real build time (shaderc +
  glslang/SPIRV-Tools/SPIRV-Cross/Dawn-Tint).
- Chunk shaders have no texturing — flat lit color only. `MeshVertex.u`/
  `.v` are populated but unused downstream until a texture atlas exists
  (Phase 12).
- No visual verification of any rendering exists or can exist in this
  sandbox — every claim above about the draw call is about the API
  calls succeeding (valid handles, no crash, bgfx accepts the shader
  binaries), not about correct-looking output on a screen.

## Next Task

1. Phase 3 or Phase 4 (pick one, record the choice and why in
   `DECISIONS.md`): either `engine/world` (a `World` owning multiple
   chunks by `ChunkCoord`, load/unload by distance, the chunk lifecycle
   state machine from `ARCHITECTURE.md`), or `engine/physics` (AABB
   collision, voxel DDA raycaster, first-person camera). Leaning toward
   world first since physics/raycast want more than one hardcoded chunk
   to be meaningful against.
2. Update state docs and commit after each step, same as every prior one.

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
