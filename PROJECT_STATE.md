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
rendering) in progress: chunk storage, coordinate math, BlockRegistry,
job system, and greedy meshing are all done and tested. Meshing isn't
dispatched through the job system yet (still called synchronously), and
nothing uploads a `ChunkMesh` to the GPU yet (`engine/rendering` clears
a frame but draws no geometry).

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: dispatch
`mesh_chunk_greedy` through `engine/jobs::JobSystem` instead of calling
it synchronously, then upload the resulting `ChunkMesh` into
`engine/rendering`/bgfx as real vertex/index buffers for a
textured-cube-on-screen milestone.

## Last Completed Task

Added `engine/voxel::mesh_chunk_greedy<EdgeLength>()`: axis-sweep greedy
meshing producing a renderer-agnostic `ChunkMesh` (opaque layer only for
now - transparent/water layers exist structurally but stay empty until
a transparent block exists to motivate their face rules, see
DECISIONS.md). Registry-driven opacity, not a hardcoded air check.

The trickiest part to get right without a display to check visually -
triangle winding - is verified structurally: every emitted triangle's
`cross(edge1, edge2)` is asserted to match its stored vertex normal. 8
new unit tests (empty chunk, isolated block producing 6 unmerged faces,
same-type blocks merging into fewer/larger quads, different-type blocks
NOT merging across the boundary, a transparent neighbor culling
identically to air, two transparent blocks producing no face, a
boundary block, and a full 16x16 slab collapsing to exactly 6 quads).
`ctest` 59/59 passing in both build configs; `VoxelServer` still
SDL/bgfx-free per `ldd`.

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx` (or `build/dev-nobgfx`): 59/59 passing
(Log, Vec3, Mat4, FrameStats, InputState, Chunk, ChunkStorage, ChunkCoord,
BlockRegistry, GreedyMesher, JobSystem unit tests). JobSystem additionally
verified via 200 repeated `ctest`-suite runs and 50 runs under
ThreadSanitizer, zero failures/races - see `BUILDING.md` "Testing under
ThreadSanitizer" for the exact commands. GreedyMesher's triangle winding
is verified via a geometric cross-product check, not just vertex counts.
No integration tests yet (no networking/save system exists yet to
integration-test).

## Known Bugs

None currently tracked.

## Known Limitations

- `VoxelClient` opens a window and clears a frame via bgfx but draws no
  geometry yet — `mesh_chunk_greedy` produces a `ChunkMesh` in plain CPU
  memory, but nothing uploads it into bgfx vertex/index buffers yet.
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
- `engine/jobs::JobSystem` still has no real consumer wired in —
  `mesh_chunk_greedy` is currently called directly/synchronously, not
  dispatched as a job. It's exercised by its own unit tests plus now
  `GreedyMesher`'s tests call it directly, but nothing submits it to the
  scheduler yet.
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

## Next Task

1. Phase 2: dispatch `mesh_chunk_greedy` through
   `engine/jobs::JobSystem` (its first real consumer) instead of calling
   it synchronously.
2. Upload the resulting `ChunkMesh` into `engine/rendering`/bgfx as real
   vertex/index GPU buffers, for a textured-cube-on-screen milestone.
   Needs at least one real `BlockDefinition` registered somewhere for
   `VoxelClient` to have something to mesh (a trivial "game:stone"
   placeholder is enough — real content isn't the point yet).
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
