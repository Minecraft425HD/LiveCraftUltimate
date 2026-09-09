# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, and
`NETWORKING.md` once networking is relevant, in that order, before
touching code. The repository is the source of truth, not this file's
prose if the two disagree — if in doubt, run the build and tests and
trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

Phase 0, 1, 2, 3, 4, 5, 6 complete/functionally complete for what this
headless sandbox can verify. Phase 7 (networking + dedicated server) is
also functionally complete: `engine/network`'s four-channel transport
and `VoxelServer`'s real world/AI simulation + network handshake are
done and tested.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: **Phase 8 — Replication +
prediction + interpolation.** Client-side prediction/reconciliation,
remote entity interpolation, interest management, chunk network
streaming/compression - the phase that finally connects `VoxelClient` to
`VoxelServer` over the Phase 7 transport (`VoxelClient` still has no
network code of its own today).

## Last Completed Task

Phase 7: added `engine/network` - a UDP transport (`UdpSocket`,
cross-platform POSIX/Winsock, non-blocking) with a hand-rolled
ack/retransmit protocol (`Connection`) implementing all four channel
semantics `ARCHITECTURE.md` commits to (`UnreliableUnordered`,
`UnreliableSequenced`, `ReliableUnordered`, `ReliableOrdered`) - see the
new `NETWORKING.md` for the wire format. `Connection` never touches a
socket directly (it only produces/consumes raw byte packets), so the
protocol logic - ordering, deduplication, retransmission timing - is
fully unit-tested with zero real I/O, on top of real loopback-socket
integration tests, including one that deliberately drops the first real
UDP datagram sent and confirms retransmission recovers it.

`VoxelServer` was rewritten from the Phase 0 sleep-only placeholder to a
real simulation loop: generates/loads a real `World`, runs the same
wandering-AI simulation as `VoxelClient` (`engine/ecs` +
`game::systems::update_ai_wander`), and listens for real UDP
connections - a peer is "connected" the moment the server receives any
datagram from its address, receives a real `ReliableOrdered` Welcome
message (world seed + tick rate), and gets a per-tick
`UnreliableSequenced` Heartbeat (tick number + live entity count) from
then on. Verified via a real two-process test: a standalone Python UDP
client connects to a running `VoxelServer` and receives a genuine
Welcome (`world_seed=1337 tick_rate=20`) followed by live heartbeats
(`tick=8 entity_count=3`) - the server's own log corroborates. `ldd`
reconfirmed `VoxelServer` still carries zero SDL/bgfx dependency.

46 new unit/integration tests. `ctest` 221/221 passing (bgfx build) /
218/218 (non-bgfx build).

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 221/221 passing (this build dir is
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 218/218 passing
(`ChunkMeshUpload.*` only exists in the bgfx build, since it needs a
real bgfx context). Covers Log, Vec3, Mat4, FrameStats, InputState,
Chunk, ChunkStorage, ChunkCoord, BlockRegistry, GreedyMesher, JobSystem,
ChunkMeshUpload, World, Worldgen, ChunkSerializer, Raycast,
PlayerPhysics/AABB, FirstPersonCamera, MovementInput, ItemRegistry,
Inventory, RecipeRegistry, ecs::Registry, block/sky light propagation,
AIWanderSystem, DayNightCycle, Sequence, PacketHeader, Connection,
UdpSocket, Address, LoopbackIntegration. JobSystem
additionally verified via 200 repeated `ctest`-suite runs and 50 runs
under ThreadSanitizer, zero failures/races - see `BUILDING.md` "Testing
under ThreadSanitizer" for the exact commands. GreedyMesher's triangle
winding is verified via a geometric cross-product check, not just
vertex counts. Real integration tests now exist for networking
(`LoopbackIntegration.*`: two `Connection`s over real loopback UDP
sockets, one deliberately dropping the first real datagram sent);
save/load is still unit-tested only, not yet exercised through a full
server-save/client-load cycle since there's no server-side world-save
trigger yet, and `VoxelClient`/`VoxelServer` don't call it either - see
Known Limitations.

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
- `VoxelClient` has no network code at all - it still runs entirely
  single-player/local, never connecting to a `VoxelServer`. Replication,
  client-side prediction, and interpolation are Phase 8's job.
- `engine/network::Connection`'s reliable channels have no RTT
  estimation, congestion control, or max-resend cutoff - a fixed
  retransmit interval, forever. Correct (tested, including under real
  simulated loss) but not tuned for real-world network conditions - see
  NETWORKING.md/DECISIONS.md.
- `VoxelServer`'s connection model has no authentication - any UDP
  datagram from a new address is treated as a new client connection, no
  questions asked. Fine for this vertical slice, not for any real
  deployment - see NETWORKING.md.
- `Connection`'s reorder buffer and duplicate-detection set for the two
  reliable channels use raw `u16` sequence values in
  ordered/hash containers, whose numeric ordering doesn't account for
  wraparound the way `sequence_greater_than` does - only matters past
  65536 messages on a single channel of one connection, far beyond this
  vertical slice's traffic volume. See NETWORKING.md.
- `VoxelServer`'s AI/world simulation runs identically whether or not
  any client is connected, but nothing is actually replicated to a
  connected client beyond the Welcome/Heartbeat handshake messages -
  entity positions, block edits, etc. aren't sent anywhere yet (Phase 8).
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

1. Phase 8: give `VoxelClient` real network code for the first time -
   connect to a `VoxelServer` over `engine/network`, parse the Welcome
   message it already sends (world seed, tick rate - currently only
   `VoxelServer` speaks this protocol; nothing on the client side reads
   it back yet).
2. Server-authoritative state replication: decide (and record in
   DECISIONS.md) what the server sends each tick and on what channel -
   likely entity positions on `UnreliableSequenced` (matching the
   existing Heartbeat's channel choice) and block edits on
   `ReliableOrdered` (an edit can't be allowed to just go missing the
   way a stale position update can).
3. Client-side prediction + reconciliation for local player movement
   (predict locally, correct against the server's authoritative state
   when it disagrees) and interpolation for remote entities (smooth
   between received position updates rather than snapping).
4. Interest management (brief section 22/64) and chunk network
   streaming/compression - only sending a client the chunks/entities
   actually near it, not the whole loaded world.
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
