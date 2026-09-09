# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, and
`NETWORKING.md` once networking is relevant, in that order, before
touching code. The repository is the source of truth, not this file's
prose if the two disagree — if in doubt, run the build and tests and
trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

Phase 0 through 9 complete/functionally complete for what this headless
sandbox can verify. Phase 10 (mobile + touch + Android + iOS) is also
functionally complete for what this sandbox can verify:
`engine/platform::TouchInputBackend` and `lcu::core::QualityProfile` are
real, tested code; real Android/iOS project generation stays BLOCKED
here for lack of a toolchain.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: **Phase 11 — Optimization +
profiling.** Benchmarks for voxel access, chunk gen, meshing, lighting,
physics, serialization, compression, network, entity sim.

## Last Completed Task

Phase 10: `engine/platform::TouchInputBackend` maps a frame's active
finger touches onto the same `Action`/`InputState` abstraction
`KeyboardInputBackend` already drives (brief section 28) - a
twin-virtual-stick layout (movement drag on the screen's left half,
look drag on the right, both dead-zone-thresholded into the existing
discrete Move*/Look* actions) plus fixed button rects for
Jump/Interact/PlaceBlock/Sprint/Crouch/Inventory. `MovementInput`/
`FirstPersonCamera`/the break-place loop need zero changes to work with
it - they only ever read `InputState`. Pure logic with no SDL
dependency (there's no real touchscreen in this sandbox to wire a real
`SDL_EVENT_FINGER_*` pump against, so that plumbing is honestly
deferred - see Known Limitations), fully exercised by 13 unit tests
against synthetic `TouchPoint` lists (single-finger drag, dead zone,
two simultaneous drags, button-vs-drag disambiguation, release
clearing state).

Added `lcu::core::QualityProfile`/`chunk_load_settings_for` (brief
section 60's MOBILE_LOW/MEDIUM/HIGH, plus `Desktop`) - deliberately
placed in `engine/core`, not `engine/platform`, since `VoxelServer`
needs it too and `engine/platform` is gated behind `LCU_BUILD_CLIENT`
and off-limits to the server (see ARCHITECTURE.md "Server has zero
GPU/window dependency"). `Desktop` is numerically identical to this
project's pre-existing hardcoded chunk-load radius/vertical-range
(`kLoadRadiusXZ`/`kMinChunkY`/`kMaxChunkY`, now removed in favor of
this), so nothing changes by default; each Mobile tier trims both,
down to a single loaded chunk for `MobileLow`. Wired into both
`VoxelClient` and `VoxelServer` via a new `LCU_QUALITY_PROFILE` env
var (an unrecognized value falls back to `Desktop`, not a crash). 4
unit tests plus real-run verification: default and an invalid env var
value both still log "Loaded 36 chunks" exactly as before this phase;
`LCU_QUALITY_PROFILE=mobile_low`/`mobile_high` log "Loaded 1 chunks"/
"Loaded 27 chunks" respectively - a real behavioral change, not just a
label, confirmed by an actual run.

Re-verified `CMakePresets.json`'s `android-arm64`/`ios` presets:
`cmake --preset android-arm64` correctly reaches and fails only at
Android's own NDK-detection step, confirming the preset itself parses
and is structurally correct - not broken CMake, just genuinely blocked
on a missing toolchain in this environment. A full Android Gradle
project / iOS Xcode project wrapper around these presets is
deliberately not written this phase - unverifiable native-mobile
project boilerplate this sandbox can't build or run is exactly what
the project's "never claim done beyond what's verified" discipline
argues against (see DECISIONS.md); it's real work for whoever has the
actual NDK/Xcode toolchain to exercise it against.

17 new unit tests (`TouchInputBackend`, `QualityProfile`/
`parse_quality_profile`). `ctest` 291/291 passing (bgfx build) /
288/288 (non-bgfx build).

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx`: 291/291 passing (this build dir is
configured with `LCU_BUILD_SHADER_TOOLS=ON` too, so it also produces
compiled chunk shaders - `ctest` itself doesn't test shader compilation
directly, that's verified by actually running `VoxelClient`, see
`BUILD_STATUS.md`). `ctest --test-dir build/dev-nobgfx`: 288/288 passing
(`ChunkMeshUpload.*` only exists in the bgfx build, since it needs a
real bgfx context). Covers Log, QualityProfile, Vec3, Mat4, FrameStats,
InputState, TouchInputBackend, Chunk, ChunkStorage, ChunkCoord,
BlockRegistry, GreedyMesher, JobSystem,
ChunkMeshUpload, World, Worldgen, ChunkSerializer, Raycast,
PlayerPhysics/AABB, FirstPersonCamera, MovementInput, ItemRegistry,
Inventory, RecipeRegistry, ecs::Registry, block/sky light propagation,
AIWanderSystem, DayNightCycle, Sequence, PacketHeader, Connection,
UdpSocket, Address, LoopbackIntegration, PositionInterpolator,
PredictionBuffer, ReplicationProtocol, LuaState, EventBus,
RegistryBindings, ModLoader. JobSystem
additionally verified via 200 repeated `ctest`-suite runs and 50 runs
under ThreadSanitizer, zero failures/races - see `BUILDING.md` "Testing
under ThreadSanitizer" for the exact commands. GreedyMesher's triangle
winding is verified via a geometric cross-product check, not just
vertex counts. Real integration tests exist for networking
(`LoopbackIntegration.*`: two `Connection`s over real loopback UDP
sockets, one deliberately dropping the first real datagram sent) and,
outside the automated suite, a real two-process `VoxelClient`<->
`VoxelServer` multiplayer run (see BUILD_STATUS.md); save/load is still
unit-tested only, not yet exercised through a full server-save/
client-load cycle since there's no server-side world-save trigger yet,
and `VoxelClient`/`VoxelServer` don't call it either - see Known
Limitations.

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
- `VoxelClient`'s networked mode (`LCU_CONNECT_PORT`) only connects to
  `127.0.0.1` - there is no hostname/IP-string parser anywhere yet, and
  no in-game "connect to a server" UI. A real "join by address" flow is
  later work.
- Chunk data and block edits aren't replicated over the network at all.
  Both a connected client and the server generate their own independent
  copy of the world from the same hardcoded seed; break/place still only
  mutates the client's own local `World`, invisible to the server or any
  other client. Chunk streaming specifically needs message
  fragmentation `engine/network::Connection` doesn't implement yet (a
  compressed chunk doesn't fit in one UDP datagram) - see NETWORKING.md.
- `VoxelClient`'s networked mode logs the `Welcome` message's
  `world_seed` but doesn't actually use it for world generation - it
  still calls its own compile-time `kWorldSeed`, which by construction
  runs before any network round-trip could complete. Both are hardcoded
  to 1337 today so this isn't currently observable as a mismatch - see
  NETWORKING.md.
- `engine/network::Connection`'s reliable channels have no RTT
  estimation, congestion control, or max-resend cutoff - a fixed
  retransmit interval, forever. Correct (tested, including under real
  simulated loss) but not tuned for real-world network conditions - see
  NETWORKING.md/DECISIONS.md.
- `VoxelServer`'s connection model has no authentication - any UDP
  datagram from a new address is treated as a new client connection, no
  questions asked. A `PlayerInput`'s reported `dt` is trusted (clamped to
  a ceiling, but not otherwise validated) - a real anti-cheat concern for
  any public deployment, fine for this vertical slice. See NETWORKING.md.
- `Connection`'s reorder buffer and duplicate-detection set for the two
  reliable channels use raw `u16` sequence values in
  ordered/hash containers, whose numeric ordering doesn't account for
  wraparound the way `sequence_greater_than` does - only matters past
  65536 messages on a single channel of one connection, far beyond this
  vertical slice's traffic volume. See NETWORKING.md.
- Interest management (`VoxelServer`'s per-client `EntityState`
  distance filter) is real logic but never actually exercised excluding
  an entity in any verification run so far - this vertical slice's
  world and AI wander radius are small enough that everything stays
  within `kInterestRadius` of any client near spawn. See NETWORKING.md.
- Every multiplayer verification run so far has used exactly one
  connected client - multiple simultaneous clients are structurally
  supported (`VoxelServer` already keys everything by `Address` in a
  map) but untested together.
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
- Registries beyond `BlockRegistry`/`ItemRegistry` (`EntityRegistry`,
  `BiomeRegistry`, `StructureRegistry`, `SoundRegistry`,
  `CommandRegistry`) don't exist yet — Lua bindings only cover the two
  registries that were already real before Phase 9; the others are
  added when something actually needs them (brief section 98).
- `EventBus` only has one real event (`block_broken`) — no entity-
  spawned/player-joined/tick/... events exist since nothing in the
  engine fires them yet; add another `emit_<event>()` the same way once
  a second real event exists (see DECISIONS.md).
- `ModLoader` has no manifest/dependency/version format — a mod is just
  a directory name plus a fixed `init.lua` entry point. No load-order
  guarantees between mods beyond directory iteration order, no way for
  one mod to depend on another.
- Mod-registered block/item ids aren't synced over the network at all —
  `VoxelClient` and `VoxelServer` each load `mods/` independently and
  must agree by construction (same mods directory, same registration
  order) for ids to match; nothing detects or reports a mismatch. Same
  underlying gap as the pre-existing "block edits aren't replicated"
  limitation above.
- Lua sandboxing is standard-library-only (no `io`/`os`/`package`) — a
  mod script still runs with no CPU/memory/time limits (a mod with an
  infinite loop hangs the host process); resource-limiting a Lua VM is
  deferred until a real need (untrusted third-party mods, not just this
  repo's own `example_mod`) exists.
- `TouchInputBackend` has no real input source wired up — nothing in
  `VoxelClient` calls it yet, and there's no `SDL_EVENT_FINGER_*` pump
  in `engine/platform::Window` to feed it real touches from (this
  sandbox has no touchscreen to test that against anyway). The
  touch-to-`Action` mapping logic itself is real and unit tested; only
  the "read real hardware touches and call `update()`" wiring is
  missing.
- Button/drag-region layout in `TouchInputBackend` is a fixed set of
  normalized-screen-space rectangles, not configurable/skinnable, and
  has no on-screen visual representation (no `engine/ui` yet to draw
  the virtual joystick/buttons a player would actually see) — a player
  would currently be dragging/tapping blind.
- `QualityProfile` only controls chunk-load radius/vertical range so
  far — no render-distance-vs-loaded-distance split (both are the same
  number today), no texture/shadow/particle quality tiers, since none
  of those systems have more than one quality level to choose between
  yet (no texture atlas, no shadows, no particles).
- Android/iOS: only `CMakePresets.json` entries exist and were
  re-verified structurally reachable (`android-arm64` fails only at
  NDK detection, as expected without one installed). No Gradle project,
  no `AndroidManifest.xml`, no Xcode project/Info.plist, no mobile
  entry point wiring `SDL_main`/touch events on either platform — all
  deferred until there's an actual NDK/Xcode toolchain in the
  environment to build and run against (this sandbox is Linux-only, no
  GPU/display either way).

## Next Task

1. Phase 11: optimization + profiling. Benchmarks (`tools/benchmark`)
   for voxel access, chunk gen, meshing, lighting, physics,
   serialization, compression, network, entity sim — real measurements
   from this sandbox's CPU.
2. Phase 12: UI + audio + content + polish.
3. Update state docs and commit after each step, same as every prior
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
