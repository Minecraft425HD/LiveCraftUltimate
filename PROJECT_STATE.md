# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, in that
order, before touching code. The repository is the source of truth, not
this file's prose if the two disagree — if in doubt, run the build and
tests and trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

Phase 0, 1, 2, 3, 4, 5 complete/functionally complete for what this
headless sandbox can verify. Phase 6 (entities + AI + lighting +
day/night) is also functionally complete: `engine/ecs`,
`engine/lighting`, wandering AI, a day/night cycle, and `VoxelClient`
running all of it together (real per-chunk lighting kept correct
through every block edit, 3 AI entities, a ticking day/night cycle) are
all done and tested.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: **Phase 7 — Networking +
dedicated server.** `engine/network` transport, and `VoxelServer`'s real
simulation loop (replacing the Phase 0 placeholder that just sleeps at
20 TPS).

## Last Completed Task

Phase 6: added `engine/ecs::Registry` (generation-checked `EntityId`
handles - a stale handle from a destroyed entity never aliases whatever
later reuses its slot; sparse-set `ComponentPool<T>` per component type
for cache-friendly dense iteration; `create`/`destroy_entity`,
`add`/`get`/`has`/`remove_component`, `pool_for<T>()`), and
`engine/lighting::{LightStorage, compute_block_light, compute_sky_light,
propagate_added_block_light, unpropagate_block_light}` (packed 4-bit
sky + 4-bit block light per voxel; the last two are true incremental
local updates for a single block add/remove - the standard two-phase
BFS removal algorithm - not a full per-edit recompute, satisfying brief
section 24's "local updates, not full recompute" directly). Single-chunk
scope for now (no cross-chunk light bleed) - see DECISIONS.md.

Added `game::components::{Position, AIWander}` and
`game::systems::update_ai_wander` (idle-then-walk-to-target loop, new
target picked on arrival, explicit `std::mt19937` for determinism - no
hidden global RNG state) and `game::systems::DayNightCycle` (cosine
sky-light-scale curve: full brightness at noon, a dim nonzero floor at
midnight). `VoxelClient` now spawns 3 wandering AI entities and a
`DayNightCycle`, updates both every frame, computes real per-chunk
block+sky light at load time, and keeps it correct through every
break/place edit via the incremental primitives instead of re-flooding
the whole chunk. Also fixed a real off-by-one surfaced while verifying
this: `terrain_height()` returns the topmost *solid* block's Y, so the
player (and now AI) previously spawned with feet embedded one block
into the ground rather than resting on the surface as documented -
spawn Y is now `terrain_height() + 1`.

38 new unit tests (`Registry`, block/sky light propagation,
`AIWanderSystem`, `DayNightCycle`). `ctest` 187/187 passing (bgfx
build) / 184/184 (non-bgfx build). Verified via a real headless run:
"Sky light 5 blocks above spawn column: 15", real AI entity positions
logged, "Day/night: time_of_day=0.000 sky_light_scale=0.550", and the
`LCU_VERIFY_BREAK_PLACE` break-then-place round-trip still holds after
the spawn-height fix.

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 187/187 passing (this build dir is
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 184/184 passing
(`ChunkMeshUpload.*` only exists in the bgfx build, since it needs a
real bgfx context). Covers Log, Vec3, Mat4, FrameStats, InputState,
Chunk, ChunkStorage, ChunkCoord, BlockRegistry, GreedyMesher, JobSystem,
ChunkMeshUpload, World, Worldgen, ChunkSerializer, Raycast,
PlayerPhysics/AABB, FirstPersonCamera, MovementInput, ItemRegistry,
Inventory, RecipeRegistry, ecs::Registry, block/sky light propagation,
AIWanderSystem, DayNightCycle. JobSystem
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
- Lighting (`engine/lighting`) is single-chunk scoped - no light bleeds
  across a chunk boundary yet (a bright torch one block from a chunk
  edge won't light the neighboring chunk's cells, and sky light doesn't
  know whether the chunk above it is open sky or a solid roof). Sky
  light also doesn't spread laterally under overhangs (straight
  top-down column fill only). See DECISIONS.md.
- Computed light (`engine/lighting::Light`, held per-chunk in
  `VoxelClient`'s `chunk_light`) isn't consumed by anything visual yet -
  the chunk shader is still flat directional+ambient lit with no
  per-voxel light sampling. Only observed via a log line
  ("Sky light 5 blocks above spawn column: ...").
- `DayNightCycle` ticks and its `sky_light_scale()` is correct and
  tested, but nothing scales the actual rendered scene or
  `engine/lighting` data by it yet - also log-line-only for now.
- AI (`game::systems::update_ai_wander`) is wander-only: no player
  awareness, no pathfinding/obstacle avoidance (a wandering entity can
  walk into a wall and just stops making progress until its next
  target pick), no combat/interaction. The 3 spawned entities in
  `VoxelClient` have no visual representation (no mesh/model system for
  entities yet) - only logged positions.
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

1. Phase 7: `engine/network` transport - decide and record in
   DECISIONS.md whether this is a chosen library (e.g. ENet, GameNetworkingSockets)
   or a hand-rolled UDP protocol, with reliable and unreliable channels
   (brief section 63's requirement - not everything needs guaranteed
   delivery/ordering, e.g. frequent position updates).
2. `VoxelServer`'s real simulation loop - replacing the Phase 0
   placeholder (sleeps at 20 TPS, no actual simulation) with one that
   actually ticks `World`/physics/entities, now that `engine/ecs`,
   `engine/physics`, and `engine/world` all exist and are usable from
   the headless `Lcu::EngineCore` `VoxelServer` already links.
3. Server-authoritative state: decide what the server owns vs. what
   clients predict (brief section 63/64) - this phase doesn't need to
   solve replication/prediction (that's Phase 8), just get real client-
   server messages flowing over the transport from (1).
4. Update state docs and commit after each step, same as every prior
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
