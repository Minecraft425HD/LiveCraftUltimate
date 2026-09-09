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
job system, greedy meshing, and GPU buffer upload are all done, tested,
and wired together end-to-end in `VoxelClient`. What's left before an
actual on-screen cube: a compiled bgfx shader program and a real
`bgfx::submit()` draw call.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: get a real draw call
working. Needs a compiled bgfx shader program - most likely means
enabling `BGFX_BUILD_TOOLS=ON` to build bgfx's `shaderc` and writing
minimal `.sc` shaders; evaluate alternatives and record the choice in
`DECISIONS.md` when this starts.

## Last Completed Task

Wired the full Phase 2 pipeline together in `VoxelClient`: registers a
placeholder `"game:stone"` block, builds a flat ground-slab `Chunk`,
dispatches `mesh_chunk_greedy` through `engine/jobs::JobSystem` (its
first real caller outside its own tests), and uploads the result into
real bgfx GPU buffers via the new
`engine/rendering::upload_chunk_mesh_layer`/`destroy_gpu_chunk_mesh`
(bgfx is now a PUBLIC link dependency of `Lcu::Rendering`, since the new
header exposes bgfx handle types).

Verified via an actual headless run, not just unit tests: `"Meshed
placeholder chunk: opaque 24 vertices / 36 indices"` then `"Uploaded
chunk mesh to GPU buffers: valid=true index_count=36"` - real numbers
from a real 256-block slab greedy-meshed to 6 quads. 3 new unit tests
(empty layer, a manually-built quad, full chunk-to-GPU pipeline), each
exercising its own headless bgfx init/shutdown cycle within one test
binary (confirms bgfx supports that cleanly). `ctest` 62/62 passing
(bgfx build) / 59/59 (non-bgfx build); `VoxelServer` still SDL/bgfx-free
per `ldd`. No shader/draw-call yet - documented as the explicit next
step, not skipped silently.

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 62/62 passing. `ctest --test-dir
build/dev-nobgfx`: 59/59 passing (`ChunkMeshUpload.*` only exists in the
bgfx build, since it needs a real bgfx context). Covers Log, Vec3, Mat4,
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

- `VoxelClient` uploads a real chunk mesh into real bgfx GPU buffers but
  never draws them — no shader program exists (no shader compiler
  built), so there's no `bgfx::submit()` call yet. The buffers just sit
  there, created and eventually destroyed, unused this frame.
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
- No shader program or draw call exists — `GpuChunkMesh` buffers are
  created and destroyed but never submitted for rendering. bgfx's
  shader compiler (shaderc) isn't built (`BGFX_BUILD_TOOLS=OFF`); no
  `.sc` shader source exists anywhere in the repo yet.

## Next Task

1. Phase 2: get a real draw call working. Requires a compiled bgfx
   shader program - most likely path is enabling `BGFX_BUILD_TOOLS=ON`
   to build `shaderc` and writing minimal `.sc` vertex/fragment shaders;
   evaluate alternatives and record the choice in `DECISIONS.md` when
   this starts.
2. Wire `bgfx::submit()` into `VoxelClient`'s render loop for the
   already-uploaded `GpuChunkMesh`.
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
