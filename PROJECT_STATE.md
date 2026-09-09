# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, in that
order, before touching code. The repository is the source of truth, not
this file's prose if the two disagree — if in doubt, run the build and
tests and trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

Phase 0, 1, 2, 3 complete/functionally complete for what this headless
sandbox can verify. Phase 4 (player + physics + interaction) is also
functionally complete: voxel DDA raycasting, AABB collision/player
physics, a first-person camera, and `VoxelClient` block break/place
wired against a real multi-chunk `World` are all done and tested.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: **Phase 5 — Items +
inventory + crafting.** `ItemRegistry`, an `Inventory` component, and
`RecipeRegistry`.

## Last Completed Task

Phase 4: added `engine/physics::raycast` (voxel DDA / Amanatides & Woo
against `World`), `engine/physics::{AABB, move_and_collide,
integrate_player}` (axis-independent Y->X->Z collision resolution,
gravity, jump, auto-step, a dedicated ground-probe fix for a real
stationary-player grounding bug found and fixed before it ever
shipped - see DECISIONS.md), and `engine/player::{FirstPersonCamera,
movement_direction_from_input}` (yaw/pitch camera matching the existing
`Mat4::look_at` convention, WASD-relative movement direction).

Rewrote `client/main.cpp` from Phase 2's single hardcoded placeholder
chunk to a real vertical slice: loads a 36-chunk area of `World`-driven
terrain around spawn, spawns a physics-driven player resting on the
generated surface, drives the camera from arrow-key look input
(`LookUp/Down/Left/Right`, an interim stand-in for mouse-look - see
DECISIONS.md) and WASD movement through `integrate_player`, raycasts
from the camera every frame, and mutates the world on edge-detected
Interact (break, `E`)/PlaceBlock (place, `F`) presses - remeshing and
re-uploading not just the edited chunk but any neighbor chunk sharing
the mutated block's boundary, so cross-chunk face culling stays
correct. Verified via a real headless run using a synthetic input hook
(`LCU_VERIFY_BREAK_PLACE`): breaks a block, logs it, then places a new
block back at the exact same world position on the next raycast - a
real round-trip through mutate-world -> remesh -> re-upload, not a
mock.

12 new unit tests for camera/movement (39 total for raycast/collision/
camera/movement across Phase 4). `ctest` 125/125 passing (bgfx build) /
122/122 (non-bgfx build).

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 125/125 passing (this build dir is
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 122/122 passing
(`ChunkMeshUpload.*` only exists in the bgfx build, since it needs a
real bgfx context). Covers Log, Vec3, Mat4, FrameStats, InputState,
Chunk, ChunkStorage, ChunkCoord, BlockRegistry, GreedyMesher, JobSystem,
ChunkMeshUpload, World, Worldgen, ChunkSerializer, Raycast,
PlayerPhysics/AABB, FirstPersonCamera, MovementInput. JobSystem
additionally verified via 200 repeated `ctest`-suite runs and 50 runs
under ThreadSanitizer, zero failures/races - see `BUILDING.md` "Testing
under ThreadSanitizer" for the exact commands. GreedyMesher's triangle
winding is verified via a geometric cross-product check, not just
vertex counts. No integration tests yet (no networking exists to
integration-test; save/load is unit-tested but not yet exercised
through a full server-save/client-load cycle since there's no
server-side world-save trigger yet either, and `VoxelClient` doesn't
call it either - see Known Limitations).

## Known Bugs

None currently tracked.

## Known Limitations

- `VoxelClient` loads a static, fixed 36-chunk area around spawn once at
  startup (a 3x3 column of chunks, 4 chunks tall) rather than calling
  `World::update_streaming` every frame from the player's actual
  position - the player can walk outside the loaded area (movement/
  physics/raycast simply stop finding chunks there; `chunk_at`/
  `chunk_at_mutable` return null and break/place silently no-ops, logged
  at debug level). Wiring `update_streaming` into the per-frame loop is
  deferred, not forgotten - see DECISIONS.md.
- `World::update_streaming` itself still streams a 3D cube by Chebyshev
  distance, not the horizontal-disc-plus-bounded-vertical shape real
  worlds want. A camera/player now exists (Phase 4) but `VoxelClient`
  doesn't call `update_streaming` yet (see above), so there's still no
  real caller to validate a disc-shaped version against.
- Worldgen only implements continental+terrain (brief section 21's first
  two pipeline stages) - no climate/biome/caves/ores/structures/
  vegetation/decoration, and no surface/subsurface block variation
  (dirt/grass over stone) - single block type fills everything below
  the height.
- Chunk save/load (`engine/serialization::chunk_serializer`) is
  unit-tested in isolation but still not wired to any actual trigger in
  `VoxelClient` or `VoxelServer` (no "save world" command, no server
  persistence loop yet - Phase 7+). Brief section 80's slice 1 is closed
  in the sense that the save/load primitive exists and break/place
  mutates real in-memory chunk data; persisting those edits to disk from
  a live client/server session is still open.
- Look input is arrow keys, not mouse-look - SDL relative-mouse-mode
  plumbing doesn't exist yet (see `engine/platform/include/lcu/platform/
  input.h` and DECISIONS.md). A real, usable interim control scheme, not
  a placeholder that does nothing.
- No item drops on block break, no hotbar/hand - breaking a block simply
  removes it; there is no item system yet to hand the player anything
  (Phase 5).
- Only one block type (`game:stone`) exists anywhere outside unit tests;
  placing a block always places stone.
- `VoxelServer`'s tick loop is a placeholder (sleeps at 20 TPS, no actual
  simulation) until Phase 7.
- bgfx's real GPU backend (Vulkan/GL/Metal/D3D) selection is untested —
  only the `Noop` headless fallback has been exercised, since this sandbox
  has no GPU/display.
- Mobile/Windows/macOS builds are untested from this Linux-only sandbox;
  `CMakePresets.json` presets exist for them but have not been exercised
  on their native toolchains.
- Input abstraction covers keyboard only (`KeyboardInputBackend`); no
  real mouse-look, gamepad or touch backend yet — camera look is driven
  by arrow keys (`LookUp/Down/Left/Right`, see `DECISIONS.md`) as an
  interim scheme until SDL relative-mouse-mode is wired up; gamepad/touch
  still have no consumer until mobile work starts (Phase 10).
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

1. Phase 5: `engine/items::ItemRegistry` — datadriven, namespaced item
   definitions (same pattern as `BlockRegistry`).
2. `engine/items::Inventory` component — slot-based storage, stack
   sizes/limits.
3. `engine/items::RecipeRegistry` — shaped/shapeless crafting matching
   against an inventory/crafting grid.
4. Decide (and record in DECISIONS.md) the first real connection between
   block-break and an item - e.g. broken blocks drop a corresponding
   item into the world/inventory - since nothing produces items yet.
5. Update state docs and commit after each step, same as every prior
   one.

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
