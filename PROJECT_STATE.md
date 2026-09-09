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
rendering) in progress: chunk storage, chunk/local coordinate math, and
BlockRegistry are done and tested; meshing and the job system it depends
on are not started.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: **job system
(`engine/jobs`)** — required before meshing can run off the main thread
(brief section 18), so it comes before greedy meshing, not after.

## Last Completed Task

Added `engine/voxel`: `ChunkStorage<EdgeLength>` (flat `BlockId` array,
default 16^3 via `Chunk`), `world_to_chunk_and_local()` (floor-division
world->chunk+local coordinate splitting), and `BlockRegistry` (namespaced,
datadriven `BlockDefinition`s, air always id 0). Zero SDL/bgfx
dependency — linked into `Lcu::EngineCore`, so both `VoxelClient` and
`VoxelServer` get it; confirmed via `ldd` that `VoxelServer` is still
SDL/bgfx-free. Verified: `ctest` 39/39 passing (27 new cases), including
an exhaustive injectivity sweep over the full chunk volume for
`index_of` and a round-trip sweep of the coordinate math across
positive/negative/boundary values, plus death tests for out-of-bounds
access and duplicate block registration.

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx` (or `build/dev-nobgfx`): 39/39 passing
(Log, Vec3, Mat4, FrameStats, InputState, Chunk, ChunkStorage, ChunkCoord,
BlockRegistry unit tests). No integration tests yet (no networking/save
system exists yet to integration-test).

## Known Bugs

None currently tracked.

## Known Limitations

- `VoxelClient` opens a window and clears a frame via bgfx but draws no
  geometry yet — chunk storage exists but nothing meshes it until the job
  system + greedy meshing land.
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
- No job system yet, so no meshing yet either — meshing must not run on
  the main thread (brief section 18), so it's blocked on the job system,
  not skipped.

## Next Task

1. Phase 2: `engine/jobs` — minimal worker-thread pool with priority and
   simple dependency support, unit tested for correctness (all jobs
   eventually complete, dependency ordering respected, cancellation
   works). Required before meshing can be considered done (brief section
   18/19).
2. Greedy meshing (opaque/transparent/water layers, hidden-face removal)
   consuming `Chunk` + `BlockRegistry`, run through the job system,
   producing vertex/index buffers fed into `engine/rendering`. Needs at
   least one real `BlockDefinition` registered somewhere to mesh against.
3. Update state docs and commit after each step, same as every prior one.

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
