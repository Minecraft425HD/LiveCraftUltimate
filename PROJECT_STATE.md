# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, in that
order, before touching code. The repository is the source of truth, not
this file's prose if the two disagree — if in doubt, run the build and
tests and trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

Phase 0, 1, 2, 3, 4 complete/functionally complete for what this
headless sandbox can verify. Phase 5 (items + inventory + crafting) is
also functionally complete: `ItemRegistry`, `Inventory`,
`RecipeRegistry`, and `VoxelClient`'s block-break/place now running on
a real item economy are all done and tested.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: **Phase 6 — Entities + AI
+ lighting + day/night.** `engine/ecs` entity/component storage,
sunlight/block light propagation, simple AI, day/night cycle.

## Last Completed Task

Phase 5: added `engine/items::ItemRegistry`/`ItemDefinition`
(namespaced, datadriven, mirrors `BlockRegistry` - `kNoItemId`
auto-registered like `kAirBlockId`), `engine/items::Inventory`
(slot-based `ItemStack` storage; `add_item` tops up existing partial
stacks before spilling into empty slots, respecting each item's
`max_stack_size`; `remove_item`/`count_item`), and
`engine/items::RecipeRegistry` (shaped recipes matched via
bounding-box-trimming the query grid then exact cell comparison;
shapeless recipes matched via ingredient-multiset comparison - no
mirrored-orientation matching, a documented simplification with no
recipe needing it yet).

Gave block-break a real item consumer: `VoxelClient` now registers a
"game:stone" item, spawns the player with a 9-slot `Inventory`, and
breaking a block adds one stone item to it; placing a block now
requires (and consumes) one stone item from the inventory instead of
placing for free, refunding it if the placement target's chunk turns
out not to be loaded (a real edge case caught and fixed while wiring
this up, not just documented away). Verified end-to-end with the
existing `LCU_VERIFY_BREAK_PLACE` headless hook: break logs "Picked up
1 game:stone (inventory: 1)", place logs "... (inventory: 0)".

24 new unit tests (`ItemRegistry`/`Inventory`/`RecipeRegistry`). `ctest`
149/149 passing (bgfx build) / 146/146 (non-bgfx build).

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 149/149 passing (this build dir is
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 146/146 passing
(`ChunkMeshUpload.*` only exists in the bgfx build, since it needs a
real bgfx context). Covers Log, Vec3, Mat4, FrameStats, InputState,
Chunk, ChunkStorage, ChunkCoord, BlockRegistry, GreedyMesher, JobSystem,
ChunkMeshUpload, World, Worldgen, ChunkSerializer, Raycast,
PlayerPhysics/AABB, FirstPersonCamera, MovementInput, ItemRegistry,
Inventory, RecipeRegistry. JobSystem
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
- Block-break's item drop is a direct, hardcoded 1:1 mapping
  (`stone block -> stone item`) written into `VoxelClient` itself, not a
  general loot-table/drop-rate system - there's only one droppable block
  type to motivate one, so a real table is deferred until more than one
  exists (see DECISIONS.md).
- `Inventory` has no UI - no hotbar rendering, no drag-drop, no way for
  a player to see or rearrange their items (needs `engine/ui`, a later
  phase). `player_inventory` in `VoxelClient` is currently only
  observable via log lines.
- `RecipeRegistry` has no crafting-grid caller anywhere - implemented
  and unit-tested standalone, same as `BlockRegistry`/`ItemRegistry`
  were before `VoxelClient` used them. No crafting table/UI exists yet
  to feed it a real grid.
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

1. Phase 6: `engine/ecs` — a minimal entity/component storage (data-
   oriented, matching the rest of the engine's style - see
   ARCHITECTURE.md). No entities exist anywhere yet outside the player,
   which today is just a `PlayerPhysicsState`/`FirstPersonCamera` pair
   in `VoxelClient`, not an ECS entity.
2. Sunlight + block light propagation/removal (local BFS-style updates
   on placement/removal, not a full per-chunk recompute every edit) -
   `BlockDefinition::light_emission` already exists (brief section 24)
   but nothing reads it yet.
3. Simple AI (a passive mob or two) and a day/night cycle driving the
   sunlight propagation's light level over time.
4. Update state docs and commit after each step, same as every prior
   one.
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
