# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, in that
order, before touching code. The repository is the source of truth, not
this file's prose if the two disagree — if in doubt, run the build and
tests and trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

Phase 0, 1, 2 complete/functionally complete for what this headless
sandbox can verify. Phase 3 (world generation + streaming + save) is
also functionally complete: multi-chunk `World` with a lifecycle state
machine and distance-based streaming, deterministic terrain generation,
and versioned/corruption-checked chunk save-load are all done and
tested.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: **Phase 4 — Player +
physics + interaction.** Start with the voxel DDA raycaster and AABB
collision (both pure logic, testable against a hand-built `World`),
then a first-person camera + block break/place wiring the raycaster's
hit into `World::chunk_at_mutable()`.

## Last Completed Task

Phase 3: added `engine/world::World` (sparse chunk table, lifecycle
state machine matching ARCHITECTURE.md, distance-based streaming with
load/unload hysteresis), `engine/world::worldgen` (deterministic seeded
value-noise terrain height, continental+terrain pipeline stage only),
and `engine/serialization::chunk_serializer` (zstd-compressed, versioned
chunk save/load with corruption and version-mismatch detection - a new
dependency, see DECISIONS.md/third_party/README.md). Added
`std::hash<ChunkCoord>` so it can key `World`'s chunk table.

27 new unit tests covering: chunk lifecycle transitions and their
assertions, streaming load/unload/hysteresis behavior, terrain
determinism/variation/smoothness, and - most rigorously - save/load
correctness: a full 4096-cell round-trip, an empty chunk, a missing
file, a garbage file, a version byte flipped after the fact, and a
corrupted compressed payload, each asserted to produce its own distinct
correct error code, plus confirmation that a failed load never touches
the caller's chunk data. `ctest` 88/88 passing (bgfx build) / 85/85
(non-bgfx build); `VoxelServer` confirmed still SDL/bgfx-free via `ldd`.

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 88/88 passing (this build dir is
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 85/85 passing
(`ChunkMeshUpload.*` only exists in the bgfx build, since it needs a
real bgfx context). Covers Log, Vec3, Mat4, FrameStats, InputState,
Chunk, ChunkStorage, ChunkCoord, BlockRegistry, GreedyMesher, JobSystem,
ChunkMeshUpload, World, Worldgen, ChunkSerializer. JobSystem additionally
verified via 200 repeated `ctest`-suite runs and 50 runs under
ThreadSanitizer, zero failures/races - see `BUILDING.md` "Testing under
ThreadSanitizer" for the exact commands. GreedyMesher's triangle winding
is verified via a geometric cross-product check, not just vertex counts.
No integration tests yet (no networking exists to integration-test;
save/load is unit-tested but not yet exercised through a full
server-save/client-load cycle since there's no server-side world-save
trigger yet either).

## Known Bugs

None currently tracked.

## Known Limitations

- `VoxelClient` still draws only its one hardcoded placeholder chunk
  with a fixed camera, not a `World`-driven scene — `World` exists and
  is tested, but nothing has wired `VoxelClient` to build/stream one yet
  (that's naturally Phase 4's job, alongside the camera that would move
  through it).
- No player, physics, or interaction yet — intentionally still
  pre-vertical-slice (see `ROADMAP.md` "Vertical slice targets"). Zero
  gameplay blocks are registered anywhere outside unit tests/the
  placeholder "game:stone".
- `World::update_streaming` streams a 3D cube by Chebyshev distance, not
  the horizontal-disc-plus-bounded-vertical shape real worlds want -
  no camera/player yet to define "horizontal" against (see
  DECISIONS.md).
- Worldgen only implements continental+terrain (brief section 21's first
  two pipeline stages) - no climate/biome/caves/ores/structures/
  vegetation/decoration, and no surface/subsurface block variation
  (dirt/grass over stone) - single block type fills everything below
  the height.
- Chunk save/load is unit-tested in isolation but not yet wired to any
  actual save/load trigger (no "save world" command, no server
  persistence loop yet - Phase 7+).
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

1. Phase 4: `engine/physics` — voxel DDA raycaster (brief section 25)
   against `World`, then AABB collision (moving AABB vs. block data),
   gravity/jump/crouch/swim/step. Both independently testable against a
   hand-built `World` with no display needed.
2. First-person camera + block break/place, wiring the raycaster's hit
   result into `World::chunk_at_mutable()` to actually remove/place a
   block - the last piece of brief section 80's vertical slice (window
   -> renderer -> voxel chunk -> world -> player -> camera -> raycast ->
   break block -> place block -> save -> load; save/load already done).
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
