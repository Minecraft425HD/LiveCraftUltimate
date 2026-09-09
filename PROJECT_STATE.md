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
rendering) in progress: chunk storage, chunk/local coordinate math,
BlockRegistry, and the job system are done and tested; greedy meshing
(the thing that actually needs the job system) is not started.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: **greedy meshing** —
`engine/jobs::JobSystem`'s first real consumer, dispatched as jobs,
consuming `Chunk` + `BlockRegistry`, producing vertex/index buffers fed
into `engine/rendering`.

## Last Completed Task

Added `engine/jobs::JobSystem`: fixed worker-thread pool, priority
scheduling, dependency graphs with cascading cancellation, cancellation
of not-yet-started jobs. One mutex + condition variable guards
scheduling state; job bodies run unlocked (see DECISIONS.md —
correctness-first design, not yet optimized). Required before meshing
can run off the main thread (brief section 18). Also fixed a latent
circular-dependency smell: `engine/voxel` and `engine/jobs` now link
`Lcu::Core` directly instead of the aggregate `Lcu::EngineCore` (which
itself includes them).

Verified with extra rigor given this is genuinely tricky concurrent
code: 12 new unit tests (cross-thread execution, linear/diamond
dependency ordering, pending-job cancellation with cascade, priority
ordering with a single worker for determinism), `ctest` 51/51 passing
in both build configs, PLUS 200 repeated runs of the job-system suite
with no failures and 50 runs under GCC ThreadSanitizer with zero
data-race reports.

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx` (or `build/dev-nobgfx`): 51/51 passing
(Log, Vec3, Mat4, FrameStats, InputState, Chunk, ChunkStorage, ChunkCoord,
BlockRegistry, JobSystem unit tests). JobSystem additionally verified via
200 repeated `ctest`-suite runs and 50 runs under ThreadSanitizer, zero
failures/races - see `BUILDING.md` "Testing under ThreadSanitizer" for
the exact commands. No integration tests yet (no networking/save system
exists yet to integration-test).

## Known Bugs

None currently tracked.

## Known Limitations

- `VoxelClient` opens a window and clears a frame via bgfx but draws no
  geometry yet — chunk storage and the job system exist but nothing
  meshes a chunk yet (greedy meshing not started).
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
- `engine/jobs::JobSystem` has no real consumer yet — nothing submits
  chunk generation, meshing, lighting, serialization, compression, or
  asset-loading jobs to it, since none of those systems exist yet. It's
  exercised only by its own unit tests so far.
- `JobSystem` scheduling is a single mutex + condition variable, not
  lock-free or work-stealing — correctness-first, unoptimized (see
  DECISIONS.md). Fine at today's job volumes (its own tests); revisit
  only if Phase 11 profiling shows contention actually matters once real
  workloads (chunk gen/meshing) exist.
- `JobSystem::cancel()` only prevents not-yet-started jobs from running;
  it cannot preempt a job already `Running`. No use case has needed
  preemption yet.

## Next Task

1. Phase 2: greedy meshing (opaque/transparent/water layers, hidden-face
   removal) consuming `Chunk` + `BlockRegistry`, dispatched through
   `engine/jobs::JobSystem` as its first real consumer, producing
   vertex/index buffers fed into `engine/rendering`. Needs at least one
   real `BlockDefinition` registered somewhere to mesh against (a
   trivial "game:stone" placeholder is enough — real content isn't the
   point yet).
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
