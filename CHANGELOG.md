# Changelog

All notable changes to this project are recorded here, newest first.

## Unreleased — Phase 0 / Phase 1 / Phase 2 / Phase 3 / Phase 4 / Phase 5 / Phase 6 / Phase 7 / Phase 8 / Phase 9 / Phase 10 / Phase 11 / Phase 12 / Phase 13 / Phase 14 / Phase 15 / Phase 16 / Phase 17 / Phase 18 / Phase 19 / Phase 20 / Phase 21 / Phase 22 / Phase 23 / Phase 24 / Phase 25 / Phase 26 / Phase 27 / Phase 28 / Phase 29 / Phase 30 / Phase 31 / Phase 33 / Phase 34 / Phase 35 / Phase 36 / Phase 37 / Phase 38

### Phase 38

- **Two genuinely separate worldgen noise stages**, matching brief
  section 21's own pipeline naming ("kontinental -> terrain") for real
  instead of as a comment on one combined noise sample:
  - **Continental**: a new, much-lower-frequency noise layer
    (`kContinentalNoiseScale`, ~666-block wavelength vs. the terrain
    layer's ~100-block one) producing a broad "how much landmass"
    value per column. Alone decides two things: the column's base
    elevation before any local detail (`kDeepOceanBase` for
    continental=0 up to `kHighlandBase` for continental=1), and how
    much amplitude the terrain-detail layer below gets to work with
    (`kMinMountainAmplitude`..`kMaxMountainAmplitude`) - a coastal/
    oceanic column is capped to gentle relief regardless of what the
    detail layer samples there, a highland column can swing into
    real mountain-sized peaks and valleys.
  - **Terrain (detail)**: the original Phase 3 4-octave fractal noise,
    frequency unchanged, now scaled by the continental-driven
    amplitude above instead of one fixed `kHeightVariation` everywhere
    - this is what actually produces mountain-shaped relief inland and
    keeps ocean/coastal regions comparatively flat, rather than the
    uniform bumpiness every earlier phase generated regardless of
    location.
  - A separate seed offset (`kContinentalSeedOffset`) keeps the two
    noise fields statistically independent, so "how mountainous" and
    "the mountain shape itself" don't visibly correlate through the
    same lattice.
- **A real, pre-existing-pattern bug caught and fixed in the same
  phase**: `find_dry_spawn_column`'s search radius (`kMaxRadius=64`,
  Phase 37) was sized for the old single-frequency noise, where dry/
  wet transitions happened every ~100 blocks. Continental noise's
  much larger ~666-block wavelength means a 64-block search can now
  legitimately stay inside one giant ocean basin the entire time and
  never find land - confirmed for real: seed 1337's spawn search
  needed radius 84 to find any dry land at all, so the old radius
  would have silently fallen back to (0,0), which is itself
  underwater for this seed. Fixed by raising `kMaxRadius` to 1024 (on
  both `VoxelClient` and `VoxelServer`) and rewriting the ring search
  from an O(ring-area) re-scanned square (skipping most cells via a
  `continue`) to an O(ring-perimeter) walk of only the new ring's
  boundary cells - keeps even the worst case a fast, one-time startup
  cost (confirmed via a real run: the full search, chunk load, and
  spawn completed in 0.23s wall-clock).
- 1 worldgen test's height-range bounds widened to the new
  continental-modulated range (measured empirically via a real 20-seed
  sweep: [-15, 20], test bounds set to [-20, 30] for real headroom); 1
  new test (`LocalRoughnessVariesAcrossRegions`) - the real, directly
  observable new behavior: local terrain roughness (height range
  within a small neighborhood) now varies meaningfully from region to
  region, unlike the old uniform-amplitude model.
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn column
  (-84,-84) found for seed 1337, dry land, full sky light 5 blocks
  above), real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
  `LCU_VERIFY_CRAFT` runs at the new location, and a real two-process
  networked run where client and server independently compute the
  identical spawn column and the server-reconciled player position
  lands on dry land, zero warnings/errors.
- `ctest` 394/394 (bgfx, up from 393) / 391/391 (non-bgfx, up from
  390).
- Honestly scoped: **what the new mountain/continental terrain shape
  actually looks like on a real GPU/display is still NOT VERIFIED —
  ENVIRONMENT LIMITATION**; still no ridged-multifractal or erosion-
  style mountain shaping (a straightforward amplitude-modulated two-
  stage composition, not a full geological simulation - a real,
  scoped choice, not a shortcut hiding a gap); still no climate/biome/
  caves/ores/structures/vegetation stages (brief section 21's later
  pipeline items, Phases 39-41).

### Phase 37

- **Real sea level at world Y=0**: `worldgen::kSeaLevel` (new, exported
  constant) - `terrain_height()` is now centered on it (`kBaseHeight`
  changed from a fixed positive 32 to `kSeaLevel`, `kHeightVariation`
  20) instead of the old always-positive [8,56] range, so roughly half
  of all columns now land above sea level (dry land/hills) and half
  below it (real lake/ocean basins) - not just a comment, an actual
  change in what terrain generates.
- **`game:water`**: `generate_terrain_chunk` gained a `water_block`
  parameter - any cell above a column's terrain but at or below sea
  level is filled with it (dry-land columns are completely unaffected,
  a real behavior no-op). Deliberately `is_transparent = false` (same
  "no transparent-layer meshing exists yet" reasoning Phase 34's torch
  already established - `true` would make water correctly placed but
  invisible) and `has_collision = false` (real, honest difference from
  every other block registered so far - a player walks/swims straight
  through it via the same `has_collision`-driven `is_solid` predicate
  every other block's collision already goes through, no new physics
  special case). No buoyancy/drag/swim mechanics - an honest, scoped
  gap, not claimed as done.
- **Quality profiles re-centered on sea level**: `ChunkLoadSettings`
  for all four tiers shifted their `min_chunk_y`/`max_chunk_y` down to
  straddle Y=0 instead of sitting entirely above it (Desktop: was
  1/0/3, now 1/-1/2). Every tier's *total* chunk count is unchanged
  (1/18/27/36) - a deliberate design choice so every prior phase's
  "Loaded N chunks" claims stay numerically true even though the
  loaded volume moved.
- **Real dry spawn placement**: a fixed world (0,0) spawn column can
  now legitimately land underwater by pure chance (no swim mechanics
  exist), so `VoxelClient`/`VoxelServer` each gained an identical,
  deterministic `find_dry_spawn_column` (a small square-ring search
  outward from the origin for the nearest column at or above sea
  level) - same seed, same search, so both sides agree on where
  "spawn" is without sending it over the wire. The entire spawn-area
  load region, the player, and the AI ring all recentre on this real
  column instead of always (0,0).
- 5 worldgen unit tests updated for the new `water_block` parameter
  and recentered height range; 1 new test
  (`BelowSeaLevelColumnIsFilledWithWaterUpToSeaLevel`, spanning
  multiple chunks where a deep column's water range crosses a chunk
  boundary). 2 quality-profile tests updated (the old "must stay
  1/0/3" locked test now asserts 1/-1/2 with updated reasoning).
- Verified via real runs: a real `LCU_BUILD_SHADER_TOOLS=ON` run
  showing the new spawn-column search finding column (-19,18) for
  seed 1337 (dry land, `Player position: (-19.00, 1.90, 18.00)`),
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` all
  byte-identical in behavior at the new spawn location, and a real
  two-process networked run where client and server independently
  compute the identical spawn column ((-19,18) on both sides) and the
  server-reconciled player position lands on dry land, zero
  warnings/errors.
- `ctest` 393/393 (bgfx, up from 392) / 390/390 (non-bgfx, up from
  389).
- Honestly scoped: **what water actually looks like on a real
  GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; no
  transparent/translucent rendering (water is a solid-looking blue
  block, same trade-off the torch already made); no waves/current/
  buoyancy/swimming physics; no beach/sand transition at the shoreline
  (the same grass/dirt/stone convention continues right up to and
  under the waterline); sky light still stops entirely at water's
  surface (treated as opaque for light purposes, like any other solid
  block - unrealistic but an honest consequence of the existing
  binary open/blocked light model, not a new fake feature).

### Phase 36

- **Entity debug boxes**: new `Renderer::submit_wireframe_box` draws an
  axis-aligned box as 12 line-list edges, reusing the exact minimal
  position+color vertex format/shader `submit_billboard` (Phase 27)
  already established (`vs_sky.sc`/`fs_sky.sc` - unlit, no lighting
  concept needed) rather than adding a third shader pair. Drawn into
  the terrain view with real depth testing (occluded correctly behind
  solid terrain), depth write off (a debug aid shouldn't leave a mark
  other draws test against). Wired into `VoxelClient`: one box per
  local AI entity (single-player) or per remote interpolated entity
  (networked), reusing `make_player_aabb` - the exact box shape the
  player's own collision already uses, since `Position` has always
  been a feet position for both the player and AI/remote entities.
- **Extended debug overlay** (brief section 60's "CPU/GPU/RAM/chunks/
  entities/ping/bandwidth/draw-calls/jobs" line): a new
  `DebugOverlayStats` struct carries chunks loaded, entity count, real
  draw-call count, and unfinished job count into `draw_debug_overlay`,
  rendered as a second real on-screen text line. New
  `JobSystem::unfinished_job_count()` accessor (lock-guarded, 3 new
  unit tests) supplies the jobs number. CPU/GPU/RAM and ping/bandwidth
  are deliberately NOT added - this codebase has no real per-platform
  CPU/RAM reader or per-connection RTT/byte-counter yet, and a fake
  placeholder number would violate this project's own "never claim
  more than what's verified" discipline (see DECISIONS.md).
- Draw-call counting reflects what actually reached
  `bgfx::submit()`, not merely what was attempted: every
  `submit_*()` call silently no-ops on an invalid program (e.g.
  `LCU_BUILD_SHADER_TOOLS` off), so each counter increment mirrors
  that same no-op condition rather than over-reporting a call that
  produced nothing.
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run showing
  `"Chunk shader program valid=true"`/`"Sky shader program
  valid=true"` (confirming the new wireframe-box draw call executes
  against real compiled shaders, not just the `Noop` backend), a real
  `LCU_VERIFY_BREAK_PLACE` run (zero regressions), and a real
  two-process networked run (100 frames, 3 remote AI entities
  interpolated and boxed every frame, zero warnings/errors).
- `ctest` 392/392 (bgfx, up from 389) / 389/389 (non-bgfx, up from
  386).
- Honestly scoped: **what the wireframe boxes or overlay text actually
  look like on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
  LIMITATION**; CPU/GPU/RAM/ping/bandwidth remain deliberately absent
  from the overlay until this codebase has a real source for them.

### Phase 35

- **`reseed_light_for_newly_loaded_chunk`** (`engine/lighting/propagation.h`):
  closes the two "arrived too late" cross-chunk lighting gaps every
  prior phase honestly documented rather than guessed at. Call once,
  right after a chunk's own initial light is computed:
  - Block light (Phase 31's gap): an already-loaded neighbor's
    existing light never crossed into a chunk that loaded afterward,
    and symmetrically a newly-loaded chunk's own near-boundary emitter
    never reached an already-loaded neighbor either. Fixed by
    reseeding - walking every already-loaded neighbor's shared
    boundary face once, collecting every currently-lit cell on *both*
    sides into one queue, then re-flooding via the existing
    `flood_block_light_cross_chunk` (which only ever raises a value,
    never lowers one - safe and idempotent; an already-correct cell
    contributes nothing).
  - Sky light (Phase 30's gap): a chunk loading *above* an
    already-loaded, already-lit chunk below left that neighbor stale
    (computed assuming open sky, since nothing was there yet). Fixed
    by recomputing every already-loaded chunk below, cascading through
    however many happen to be stacked.
  - Returns every chunk this call's light actually touched, for the
    caller to remesh.
- 4 new unit tests (`ReseedLightForNewlyLoadedChunk.*`): late-arriving
  block light in both directions, sky-light cascade through two
  stacked already-lit chunks (hand-verified), and the no-already-
  loaded-neighbors no-op case.
- **VoxelClient wiring**: called after every chunk load (the initial
  spawn-area load, per-movement streaming, and the networked
  `ChunkDataFragment` receipt path - closing the exact gap that
  handler's own code comment named this phase for), remeshing whatever
  it reports touched.
- **Real client-side chunk unloading** (the literal "chunk unload"
  half of this phase's name): before this phase the client's `World`
  only ever grew for the process's whole lifetime, even as the player
  walked away, holding every chunk's mesh/GPU buffers/light data
  forever (`WorldLight::remove_chunk_light` has named this phase as
  its real caller since Phase 29). Now a distance-gated unload sweep
  (the client's own counterpart to `VoxelServer`'s Phase 20
  interest-scoped unloading) runs on every streaming-center change:
  saves the chunk to a new `client_world/chunks/` directory first
  (mirroring `VoxelServer`'s own save-before-unload exactly, so a
  single-player edit doesn't silently revert to regenerated terrain
  the moment the player wanders back), destroys its GPU mesh, removes
  its light data, then unloads it from `World`. Loading (both the
  initial spawn area and per-movement streaming) now checks that same
  directory before falling back to regenerating.
- Two new log lines (`"Streaming center moved to ..."`,
  `"Unloaded N chunk(s) beyond streaming range ..."`) plus a
  single-player `"Player position: ..."` shutdown line for parity with
  the existing networked-mode one - all real diagnostics that proved
  useful while verifying this phase's own behavior, not left in as
  debug noise.
- **A real, pre-existing (not introduced by this phase) finding made
  while verifying it**: `LCU_VERIFY_MOVE_SECONDS` moves in a straight
  line and never jumps, so it can get legitimately blocked by terrain
  taller than the player's auto-step height - confirmed byte-identical
  against the pre-Phase-35 build under the same test, so this is a
  limitation of that one verification hook's simplicity, not a physics
  bug. Documented in DECISIONS.md; not fixed here (out of this phase's
  scope) - unblocked for this phase's own real-run verification by
  temporarily also holding Jump.
- Verified via a real, longer-distance single-player run (Jump added
  temporarily to clear the terrain obstacle above): repeated
  `"Streaming center moved to ..."`/`"Unloaded 12 chunk(s) ..."` pairs
  firing correctly as the player crossed multiple chunk boundaries,
  loaded chunk count staying steady at 48 (load/unload balance
  correct), and 60 real `.chunk` save files written to
  `client_world/chunks/`. Also verified via the full existing
  regression set with zero changes needed: `LCU_VERIFY_TORCH`,
  `LCU_VERIFY_BREAK_PLACE`, `LCU_VERIFY_CRAFT` (byte-identical to
  Phase 34) and a real two-process networked run (`VoxelServer` +
  `VoxelClient`, loopback UDP, 600 server ticks, 36 chunks streamed,
  zero warnings/errors).
- `ctest` 389/389 (bgfx, up from 385) / 386/386 (non-bgfx, up from
  382).
- Honestly scoped: **what any of this looks like on a real GPU/display
  is still NOT VERIFIED — ENVIRONMENT LIMITATION**; the reload-from-
  disk half of client-side persistence wasn't separately re-verified
  end-to-end in this run (the verify hook only moves one direction) -
  the save-on-unload half is proven by 60 real files written this run,
  and the load-from-disk half reuses the exact same `chunk_serializer`
  API `VoxelServer` already proves correct (round-trip/corruption
  tests since Phase 3); lateral sky light bleed under overhangs
  remains unmodeled (documented since Phase 6).

### Phase 34

- **New content**: `game:torch` (light_emission=14, a solid glowing
  cube - deliberately `is_transparent = false`, since `mesh_chunk_
  greedy` only ever meshes the opaque layer; a transparent torch would
  be correctly lit and completely invisible, a real fake-feature trap
  avoided before it became a bug, see DECISIONS.md). Registered
  identically on `VoxelClient`/`VoxelServer`, added to `placeable_
  items`/`block_item_mapping`/`tracked_items`. New headless hook
  `LCU_VERIFY_TORCH`: breaks the spawn block, grants one torch
  directly (nothing drops one yet), cycles the hotbar to it, places it
  into the hole, and logs the real post-place `block_light` value read
  back through `WorldLight::block_light_at` - proving the full
  place -> propagate -> query pipeline end to end. Verified via a real
  run: `"Placed game:torch at world (0, 28, -1): block_light=14"`.
- **Lighting benchmarks** (`tools/benchmark`, gated `LCU_BUILD_TOOLS`):
  3 new Google Benchmark cases against the brief's own explicit perf
  budgets (not the file's usual "numbers to profile against"):
  `BM_Lighting_ComputeChunkWithNeighbors` (< 2ms), `BM_Lighting_
  PlaceTorchAtChunkEdge`/`BM_Lighting_UnplaceTorchAtChunkEdge` (each
  < 0.5ms, torch placed at the worst-case chunk-edge position).
- **Build-configuration finding**: this project's only configured
  `CMAKE_BUILD_TYPE` is the custom string `"Development"`, which CMake
  does not recognize as a built-in type - no optimization flags are
  ever applied (effectively `-O0`). Google Benchmark itself warned
  `Library was built as DEBUG` on the first run. A dedicated `-DCMAKE_
  CXX_FLAGS_RELEASE... -DCMAKE_BUILD_TYPE=Release` build directory
  (`build/bench-release`) was created to get trustworthy numbers - see
  DECISIONS.md. This is an existing project-wide gap (not introduced
  this phase, not fixed this phase beyond this one benchmark
  directory) worth a dedicated future pass.
- **Real optimization made and verified**: under genuine `-O3`, the
  place/unplace benchmarks still exceeded budget (639,123 ns / 639us
  and 766,988 ns / 767us against a 500us budget). Root cause: the
  cross-chunk BFS (`flood_block_light_cross_chunk`/`unpropagate_
  block_light_cross_chunk`) paid up to ~13 `unordered_map` lookups
  across two separate maps per popped cell, plus floor-division
  arithmetic, for every one of 6 neighbor steps - even though the
  overwhelming majority of steps stay within the same chunk as the
  cell being processed. Added an in-bounds fast path: a plain integer
  range check against `[0, EdgeLength)` lets an in-chunk step reuse
  the already-held `LightStorage*`/`ChunkStorage*` pointers directly,
  falling back to the original (`step_cross_chunk`) logic only for
  genuine boundary crossings. Behavior-preserving by construction and
  confirmed so: full `ctest` suite unchanged at 385/385 (bgfx) /
  382/382 (non-bgfx) before and after. Re-measured after the fix:
  `BM_Lighting_ComputeChunkWithNeighbors` 16,616 ns (well under 2ms),
  `BM_Lighting_PlaceTorchAtChunkEdge` 115,649 ns (under 500us),
  `BM_Lighting_UnplaceTorchAtChunkEdge` 99,158 ns (under 500us) - both
  budgets now met, by a wide margin.
- Verified via a real two-process networked run (`VoxelServer` +
  `VoxelClient`, loopback UDP, 600 server ticks, 36 chunks streamed,
  zero warnings/errors) and real `LCU_VERIFY_TORCH`/`LCU_VERIFY_
  BREAK_PLACE`/`LCU_VERIFY_CRAFT` headless runs (bgfx build) - all
  pre-existing hooks byte-identical to Phase 33's behavior, confirming
  the BFS optimization changed nothing observable.
- `ctest` unchanged at 385/385 (bgfx) / 382/382 (non-bgfx) - no new
  unit tests this phase (the BFS change is proven behavior-preserving
  by the existing suite; the new content is proven by the real
  `LCU_VERIFY_TORCH` run and the new benchmarks, not by new GoogleTest
  cases).
- Honestly scoped: **what a placed, lit torch actually looks like on a
  real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**;
  the `CMAKE_BUILD_TYPE="Development"` no-optimization gap remains
  project-wide outside `build/bench-release`; a chunk loading after a
  nearby light source's BFS already finished still isn't retroactively
  relit/remeshed (Phase 35's job, next).

### Phase 33

- **VoxelClient integration**: closed a real remesh gap - a block edit
  whose cross-chunk block-light BFS (Phase 31) actually reaches a
  neighbor chunk wasn't necessarily one of the geometric
  `neighbors_sharing_boundary` (only exactly-at-the-edge edits counted),
  so that neighbor's newly-changed light could go un-remeshed. The
  cross-chunk propagate/unpropagate functions gained an optional
  `touched_chunks` output set (which chunk, if any besides the edited
  one, actually got a light write); `update_lighting_for_edit` now
  returns it, and a new `remesh_edit_neighbors` helper remeshes the
  union of that set with the existing geometric neighbor set.
- **Smooth lighting**: `mesh_chunk_greedy`'s merged quads are now lit
  per-vertex instead of one flat value per quad - each of a quad's 4
  corners independently averages the packed light of its up-to-4
  diagonally-adjacent mask cells (`detail::smooth_corner_light`), the
  classic vertex-light-averaging technique (without ambient occlusion).
  `ChunkMeshLayer::add_quad` now takes 4 separate per-vertex light
  bytes instead of one. Merging (`MaskCell::merges_with`) no longer
  requires equal light between cells - Phase 28's flat-shading-only
  merge restriction is superseded, since a merged quad's corners are
  now independently sampled; two differently-lit adjacent faces merge
  into one quad again and blend smoothly across it. `v_color1`'s
  existing bgfx varying (Phase 28) already linearly interpolates a
  non-`flat` float across a triangle by default, so real GPU-side
  smooth shading falls out of this change with **no shader changes
  needed** - the fragment shader was already unpacking sky/block
  nibbles from whatever value the rasterizer hands it per pixel.
- 6 new unit tests: 3 for `smooth_corner_light` directly (full 4-cell
  average, edge case with fewer in-range cells, degenerate empty-mask
  fallback), plus 2 existing Phase 28 tests rewritten for the new
  semantics (a single isolated quad's uniform-by-symmetry averaged
  value; two differently-lit adjacent faces now merging and showing a
  real per-corner gradient instead of staying separate).
- Verified via a real two-process networked run (`VoxelServer` +
  `VoxelClient`, loopback UDP) and real `LCU_BUILD_SHADER_TOOLS=ON` +
  `LCU_VERIFY_BREAK_PLACE` runs (bgfx and non-bgfx) - zero
  regressions, byte-identical log output to Phase 31.
- `ctest` 385/385 (bgfx, up from 379) / 382/382 (non-bgfx, up from
  376).
- Honestly scoped: **what smooth lighting actually looks like on a
  real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**;
  no ambient occlusion (a related but separate darkening-by-solid-
  neighbor-count effect); a chunk loading after a nearby light
  source's BFS already finished still doesn't retroactively receive
  that light or get remeshed (Phase 35's job).

### Phase 32 (skipped, optional)

- Explicitly optional in the brief ("Grenzpuffer (optional)"). Its
  purpose is a concurrency optimization (deferring cross-chunk light
  writes into a buffer so concurrent threads don't contend for the
  same neighbor chunk) - nothing in this codebase dispatches lighting
  work across multiple threads yet, so there's no actual blocking to
  buffer against. See DECISIONS.md for the full reasoning.

### Phase 31

- New `flood_block_light_cross_chunk`/`propagate_added_block_light_
  cross_chunk`/`unpropagate_block_light_cross_chunk`: a genuine BFS
  that crosses chunk boundaries, unlike sky light's seeded column scan
  (Phase 30) - block light spreads in all 6 directions, so a boundary
  crossing needs a real frontier that continues into the neighbor
  chunk's own `LightStorage` (via `WorldLight`) and keeps spreading
  from there, decrementing exactly as it would within a single chunk.
- Templated on a duck-typed `ChunkProviderT` (matches
  `lcu::world::World`'s own `chunk_at(ChunkCoord) const` exactly) rather
  than a concrete `#include` - same reasoning as Phase 28's
  `LightStorageT`, keeps the header usable from tests that only
  construct bare `ChunkStorage` instances.
- A neighbor chunk that isn't loaded is never crossed into or written
  to - honestly nothing to propagate into, not a guess (same
  "unknown -> can't say, don't guess" convention `WorldLight` already
  established); a chunk that loads *later* doesn't retroactively
  receive light from a BFS that already finished (Phase 32/35
  territory).
- Terminates in at most `kMaxLightLevel` (15) steps from any seed in
  any direction, same as the single-chunk version - satisfies "BFS
  queue only runs over the radius actually affected by a change"
  without a separate hard cap, since the existing level-decrements-to-
  zero termination already bounds it.
- `client/main.cpp`'s `update_lighting_for_edit` now calls the
  cross-chunk versions (passing `world` itself as the `ChunkProviderT`)
  instead of the single-chunk ones, and its own "let light flow back in
  from the brightest neighbor" logic now queries
  `WorldLight::block_light_at` instead of only checking neighbors
  inside the same chunk - both real correctness fixes right at a chunk
  boundary, not just new plumbing.
- 4 new unit tests: light crossing into a loaded neighbor with correct
  continued decay, not crossing into an unloaded neighbor, removing a
  cross-chunk source darkens both chunks, and - the trickiest case -
  removing one of two cross-chunk sources correctly refills the overlap
  from the remaining one without leaving a dark gap or wrongly
  darkening the surviving source.
- Verified via a real two-process networked run (`VoxelServer` +
  `VoxelClient`, loopback UDP): a real break/place round-trip through
  the new cross-chunk edit path, zero warnings/errors - plus real
  `LCU_BUILD_SHADER_TOOLS=ON` and `LCU_VERIFY_BREAK_PLACE` runs (bgfx
  and non-bgfx) with byte-identical output to Phase 30.
- `ctest` 379/379 (bgfx, up from 375) / 376/376 (non-bgfx, up from
  372).
- Honestly scoped: a chunk that loads *after* a nearby light source's
  BFS already finished doesn't retroactively receive that light (Phase
  32/35's job); what real cross-chunk torchlight actually looks like on
  a real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION.

### Phase 30

- `compute_sky_light_column` gained a `sky_open_above` parameter
  (defaulted `true`, preserving every existing caller's behavior) -
  when `false`, the whole column starts pre-blocked, so a chunk with a
  solid roof directly above it (in the *neighboring* chunk) actually
  darkens instead of showing full brightness.
- New `compute_sky_light_column_cross_chunk`/`compute_sky_light_cross_
  chunk`: query the chunk directly above via `WorldLight::sky_light_at`
  (its bottom cell) to seed `sky_open_above` for real - an unloaded or
  not-yet-lit neighbor still means "assume open" (WorldLight's own
  "unknown -> best case, not guessed dark" convention), identical to
  this function's behavior before this phase.
- `client/main.cpp`'s chunk-load loops (initial spawn-area load, and
  per-movement streaming) restructured into three passes per (x,z)
  column instead of one: block light (any order), sky light
  (**top-down**, highest `chunk_y` first - required for the cascade to
  actually work), then meshing (after both light passes). The
  networked `ChunkData` receipt path (a single chunk, arbitrary
  vertical order) uses the same split but honestly can't guarantee the
  top-down ordering - documented as a known gap closed by Phase 35's
  neighbor-dirtying.
- 4 new unit tests covering: no-neighbor-above behaves like the old
  default, a solid roof one chunk up darkens the chunk below (the real
  bug this phase fixes), open sky above leaves the chunk fully lit, and
  a chunk's own internal roof still shadows regardless of what's above.
- Verified via a real two-process networked run (`VoxelServer` +
  `VoxelClient` over loopback UDP): all 36 chunks received via
  `ChunkData` and correctly lit/meshed through the new split path,
  break/place round-trips cleanly, zero warnings/errors - plus real
  `LCU_VERIFY_BREAK_PLACE` runs (bgfx and non-bgfx) producing
  byte-identical `"Sky light 5 blocks above spawn column: 15"` output
  to Phase 29 (the topmost loaded chunk still has nothing above it to
  darken it, exactly as before).
- `ctest` 375/375 (bgfx, up from 371) / 372/372 (non-bgfx, up from
  368).
- Honestly scoped: block light still doesn't cross a chunk boundary
  (Phase 31); a chunk that loads or is edited *after* its neighbor
  below was already lit doesn't yet retroactively relight that
  neighbor (Phase 35); lateral sky light bleed under overhangs remains
  a documented simplification, unchanged from before this phase.

### Phase 29

- New `lcu::lighting::WorldLight<EdgeLength>` (`engine/lighting/include/
  lcu/lighting/world_light.h`): an owning `ChunkCoord -> LightStorage`
  map plus boundary-aware `sky_light_at`/`block_light_at` queries that
  resolve a local coordinate outside `[0, EdgeLength)` into its real
  owning neighbor chunk (reusing `voxel::world_to_chunk_and_local`'s
  floor-division logic), returning `std::optional<u8>` - real data or
  honestly "don't know" (an unloaded/unlit neighbor), never a guess.
  This is the prerequisite data structure Phase 30 (sky) and Phase 31
  (block) cross-chunk propagation build on - it does not itself
  propagate light across chunks yet.
- `client/main.cpp`'s ad hoc `std::unordered_map<ChunkCoord, Light>
  chunk_light` (Phase 6) replaced with a real `DefaultWorldLight`; the
  "sky light 5 blocks above spawn" startup log now goes through
  `sky_light_at` instead of a hand-rolled `.find()`/`.end()` iterator
  lookup.
- 9 new unit tests, including explicit positive- and negative-direction
  cross-chunk boundary resolution and the "neighbor not loaded returns
  nullopt" case.
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` build and a real
  headless `LCU_VERIFY_BREAK_PLACE` run (both bgfx and non-bgfx
  configs) - byte-identical log output to Phase 28 (`"Sky light 5
  blocks above spawn column: 15"`), confirming this is a real,
  behavior-preserving refactor, not just new code that happens to
  compile.
- `ctest` 371/371 (bgfx, up from 362) / 368/368 (non-bgfx, up from 359).
- Honestly scoped: light still doesn't actually cross a chunk boundary
  yet - `sky_light_at`/`block_light_at` can *query* a neighbor chunk's
  light, but nothing yet *writes* light that originated in one chunk
  into another (that's Phase 30/31); `mesh_chunk_greedy`'s own
  boundary-face handling (Phase 28) is unchanged, still defaulting to
  full-bright at a chunk edge.

### Phase 28

- `MeshVertex` gained a packed `u8 light` field (low nibble sky, high
  nibble block - the exact `lcu::lighting::LightStorage` packing, one
  byte total as specified). `mesh_chunk_greedy` now reads real light
  from the air cell each face is actually exposed to (not the solid
  block's own cell, which propagation never touches) and packs it per
  vertex - light is computed once by `engine/lighting` per chunk load/
  edit, never recomputed by meshing or the shader per frame.
- `mesh_chunk_greedy` is now templated on a duck-typed `LightStorageT`
  (matches `lcu::lighting::LightStorage`'s public interface) instead of
  including a concrete lighting header directly - `engine/lighting`
  already depends on `engine/voxel`, so the reverse `#include` would
  have been a circular target dependency. A light-less two-argument
  overload (backed by an always-full-bright stand-in) keeps every
  existing call site (tests, `tools/benchmark`) unchanged; only
  `client/main.cpp`'s real remesh path passes its actual per-chunk
  `LightStorage`.
- Merging now also requires equal light, not just equal block id/
  facing: two adjacent faces that would otherwise merge but are lit
  differently stay separate quads, so per-voxel light doesn't get
  averaged/flattened away by the same optimization that reduces
  triangle count.
- `chunk_mesh_upload.cpp`'s vertex layout gained a matching `Color1`
  (Uint8 x1) attribute, plus an explicit `bgfx::VertexLayout::skip()` +
  `LCU_ASSERT` to keep bgfx's declared stride in exact sync with
  `sizeof(MeshVertex)`'s compiler-inserted trailing padding - a real,
  previously-nonexistent risk this phase's byte-sized field newly
  introduced (a 4-byte-aligned struct ending in one trailing byte).
- `client/shaders/{vs_chunk,fs_chunk}.sc` rewritten: the fixed Phase 26
  fake directional light is gone, replaced by
  `final = color * (sky * u_skyLightScale + block) / 15.0` using the
  real packed per-vertex light and a new `u_skyLightScale` uniform (set
  once per draw call from `DayNightCycle::sky_light_scale()` - the same
  real time signal Phase 27's skybox already reuses). The old fake
  light had no relationship to Phase 27's real sun/moon position and
  would have double-counted daylight and never actually darkened at
  night alongside real per-voxel light.
- `Renderer::submit_chunk_mesh` gained a `sky_light_scale` parameter and
  owns the new uniform's lifetime (`bgfx::createUniform`/`destroy`).
- 4 new unit tests: light-less overload stays full-bright, a face reads
  light from its exposed air cell (not the solid block), differently-lit
  coplanar faces don't merge, and a true chunk-boundary face (no
  cross-chunk light yet - Phase 29-31) defaults to full-bright rather
  than reading out of bounds or guessing dark.
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` build: `"Chunk shader
  program valid=true"` (the new `Color1`/`u_skyLightScale` wiring links
  correctly), plus a real headless `LCU_VERIFY_BREAK_PLACE` run under
  that build with the real 36-chunk world's real light data flowing
  through meshing with zero regressions/crashes (the new stride
  `LCU_ASSERT` didn't fire on real production data).
- `ctest` 362/362 (bgfx, up from 358) / 359/359 (non-bgfx, up from 355).
- Honestly scoped: **what a real GPU/display actually shows (whether
  torch/sky light genuinely looks right) is still NOT VERIFIED —
  ENVIRONMENT LIMITATION**; cross-chunk light doesn't exist yet (a
  chunk-boundary face is unconditionally full-bright, honestly, not
  guessed) - that's Phase 29-31's job; lighting isn't yet smoothed
  per-vertex (Phase 33).

### Phase 27

- Skybox: `Renderer::begin_frame` now takes an explicit `Vec3` clear
  color (+ alpha) instead of a pre-packed `u32`, and interpolates it
  every frame from the existing `DayNightCycle::sky_light_scale()` -
  night (0.02, 0.03, 0.08) to day (0.45, 0.65, 0.95) - reusing the one
  real time signal instead of a second animation clock.
- Sun/moon: camera-facing billboard quads via a new
  `Renderer::submit_billboard()`, built from the camera's own
  `right()`/`cross(right, forward)` basis and transient bgfx vertex/
  index buffers (a fresh tiny per-frame allocation, not a persistent
  GPU buffer for geometry that moves every frame). Position comes from
  a new pure `game::systems::sun_direction(time_of_day)` (moon is
  always exactly opposite).
- Own bgfx view (`kSkyViewId = 1`) ordered via `bgfx::setViewOrder` to
  execute *before* the terrain view (view 0): the sky clears
  color+depth, terrain then draws into the same depth buffer with its
  normal depth test and naturally occludes the sky wherever a block is
  actually in front of it - real occlusion via view ordering, not a
  depth trick on the sky quad itself (which has depth test/write off,
  as specified: "Tiefentest aus").
- Dedicated minimal `vs_sky.sc`/`fs_sky.sc` + `varying_sky.def.sc`
  (position + flat color only, no lighting/noise) - the sun/moon IS a
  light source, not something lit by one, so reusing the chunk
  shader's lighting would be wrong.
- Scope: stars at night were explicitly listed as optional in the
  brief ("Sterne bei Nacht optional") and are deliberately deferred -
  not a fake/missing feature, a scoped-out one.
- Real bugs found and fixed: (1) `bgfx::allocTransientVertexBuffer`/
  `allocTransientIndexBuffer` return `void` in this bgfx version, not
  `bool` - fixed by checking `getAvailTransientVertexBuffer`/
  `getAvailTransientIndexBuffer` first; (2) `sun_direction`'s formula
  was initially inlined directly in `client/main.cpp` (untestable) -
  extracted into `game::systems::sun_direction()` next to
  `DayNightCycle` so it's a pure, headlessly-testable function; this
  also needed `LcuGame` to gain an explicit `Lcu::Math` link (same
  transitive-include trap as Phase 26's `LcuVoxel` bug, avoided this
  time instead of hit).
- 6 new unit tests: `sun_direction` at all four phase points (dawn/
  noon/dusk/midnight), unit-length-in-the-xy-plane, and moon-always-
  opposite-sun.
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` build: "Chunk shader
  program valid=true" AND "Sky shader program valid=true", plus a real
  headless `LCU_VERIFY_BREAK_PLACE` run under that exact build showing
  zero regressions.
- `ctest` 358/358 (bgfx, up from 352).
- Honestly scoped: what a real GPU/display actually shows (sky color,
  sun/moon visibility and occlusion) is still NOT VERIFIED —
  ENVIRONMENT LIMITATION; stars deferred (see above).

### Phase 26

- `BlockDefinition` gained `color`/`side_color`/`bottom_color` - real
  per-block/per-face tint, no texture atlas needed. `mesh_chunk_greedy`
  selects the right one per face at mesh time (it already knows the
  axis/facing direction), so this is real data selection, not a
  shader-side special case for any specific block.
- `MeshVertex` gained a `color` field; the bgfx vertex layout gained a
  matching `Color0` attribute (both appended last, in lockstep, since
  the struct is `memcpy`'d straight into a GPU buffer).
- Registered real colors: `game:stone` gray, `game:grass` green top /
  brown sides, `game:dirt` brown.
- `client/shaders/{vs_chunk,fs_chunk}.sc` rewritten: real per-face
  color, a small explicit top-face light lift, and a subtle
  deterministic-per-voxel hash-noise pattern - no per-block branching
  in the shader itself.
- Real bug found and fixed: `engine/voxel` used `math::Vec3` without
  `LcuVoxel` ever linking `Lcu::Math` - previously silent, exposed once
  `BlockDefinition` gained a `Vec3` field and `block_registry.cpp`
  itself failed to compile.
- 2 new unit tests for the per-face color selection.
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` build (bgfx's actual
  `shaderc`, not just C++ compilation): "Chunk shader program
  valid=true", plus a real headless `LCU_VERIFY_BREAK_PLACE` run under
  that build showing zero regressions.
- `ctest` 352/352 (bgfx, up from 350) / 349/349 (non-bgfx, up from
  347).
- Honestly scoped: what a real GPU/display shows is still NOT VERIFIED
  — ENVIRONMENT LIMITATION; no texture atlas; water/sand colors
  deferred to Phase 37/39.

### Phase 25

- macOS build audit (user-directed, start of Phases 25-42: visible
  game, global lighting, procedural terrain): read every CMake/
  FetchContent path and every macOS-specific branch already in the
  codebase, rather than assuming. No Linux-only assumptions found
  anywhere in `third_party/CMakeLists.txt`; `engine/network`'s socket
  code already branches correctly for POSIX (macOS's path);
  `engine/platform`'s native-window-handle code already had a correct
  macOS Cocoa branch; bgfx.cmake's macOS linking needs zero Homebrew
  packages beyond `cmake`/`ninja` (Xcode CLT ships the rest);
  `bgfx_compile_shaders()` already auto-compiles a `metal` profile on
  an `APPLE` host with no code change needed.
- **Real bug found and fixed**: `engine/rendering::
  active_shader_profile_dir()` had no case for `bgfx::RendererType::
  Metal` and would have loaded the wrong (glsl) shader binary format
  into bgfx's macOS-preferred Metal renderer. Fixed with one added
  `case`.
- New "macOS" section in `BUILDING.md`: exact prerequisites, configure/
  build/run commands, what a real run should show.
- `ctest` unchanged at 350/350 (bgfx) / 347/347 (non-bgfx) - a real
  headless Linux run confirms the fix doesn't regress the existing
  Noop/glsl fallback path.
- Honestly scoped: this is a code audit, not a real build - actually
  running `cmake --build` against a macOS toolchain has not happened
  from this Linux-only sandbox and is marked **NOT VERIFIED —
  ENVIRONMENT LIMITATION**, not TESTED, until someone with a real Mac
  runs the documented commands.

### Phase 24

- New `EventBus::emit_item_crafted(item_id, count)` - `EventBus`'s
  second real event, closing a gap flagged since Phase 9 ("add another
  emit_<event>() the same way once a second real event exists").
- `VoxelClient`'s quick-craft handler (Phase 23) calls it right after
  a successful `find_match` + item grant. Purely client-side, like
  crafting itself - `VoxelServer` never calls it, but still exposes
  `lcu.subscribe("item_crafted", ...)` since mod scripts are shared
  between both hosts.
- `example_mod/init.lua` now subscribes to both `block_broken` and
  `item_crafted`, proving the real register -> load -> subscribe ->
  emit loop generalizes, not just that a second typed method compiles.
- Fixed a stale comment in `server/main.cpp` claiming "the server never
  calls emit_block_broken() itself" - false since Phase 13 made block
  edits server-authoritative.
- 3 new unit tests (`EventBus.EmitItemCrafted*`,
  `EventBus.BlockBrokenAndItemCraftedSubscribersAreTrackedIndependently`).
- Verified via a real single-player run: `[example_mod] item_crafted
  #1: 1 x item id 4` fires at the exact craft moment; a real server run
  confirms the mod still loads cleanly there.
- `ctest` 350/350 (bgfx, up from 347) / 347/347 (non-bgfx, up from
  344).
- Honestly scoped: both real events are still client-triggered content
  moments; nothing server-side fires an event yet.

### Phase 23

- New `Action::Craft` (`engine/platform::Action`), bound to `C` on
  keyboard and a new "CRAFT" touch button - `RecipeRegistry`'s first
  real caller, closing a gap honestly flagged since Phase 5 ("no
  crafting-grid caller exists yet").
- First crafted-only item: `game:compost` (no corresponding block).
  One real shapeless recipe on a new `lcu::items::RecipeRegistry`
  instance in `VoxelClient`: `1x game:grass + 1x game:dirt -> 1x
  game:compost`.
- Quick-craft: on an edge-detected Craft press, builds a query grid
  from one of each distinct item type currently held (dedup by
  inventory-slot scan), calls `RecipeRegistry::find_match` for real,
  consumes exactly the grid's contents on a match and grants the
  result, logs "No recipe matches your held items" on no match.
- Purely client-side (single-player and networked alike) - crafting
  never touches the `World` or needs server validation, same
  client-authoritative precedent as item pickup. No protocol/server
  changes needed.
- New standalone headless hook `LCU_VERIFY_CRAFT`. Caught and fixed a
  real bug along the way: its first, frame-count-gated version broke
  under real network latency (the unthrottled client loop outran the
  server round trip by hundreds of frames, causing a double-break/
  double-grant race) - fixed by switching to the same wall-clock-gated
  pattern `LCU_VERIFY_MOVE_SECONDS` (Phase 16) already established for
  this exact class of problem. Confirmed fixed via a second real
  networked run.
- No new unit tests - pure orchestration of already-tested
  `RecipeRegistry`/`Inventory`/`ItemRegistry` primitives. `ctest`
  unchanged at 347/347 (bgfx) / 344/344 (non-bgfx).
- Honestly scoped: quick-craft's auto-built grid only correctly
  represents a recipe needing exactly one of each distinct ingredient
  type; no graphical crafting-grid UI; shaped-recipe matching still has
  zero real caller.

### Phase 22

- New `game::items::BlockItemMapping` (`game/items/`) - a real
  `register_pair(block_id, item_id)`/`item_for_block(block_id)` table,
  closing Phase 19's remaining honest gap: `item_for_block` (server)
  and `grant_item_for_broken_block` (client) were both still three
  explicit `if (block_id == X)` checks, one per block, hand-duplicated
  between the two files.
- `VoxelClient`/`VoxelServer` both now populate the same table shape
  (three `register_pair` calls right after each block/item pair is
  registered) and do a single lookup instead of their own hardcoded
  chain - adding a fourth item-backed block is now one call per side.
- 4 new unit tests (`BlockItemMapping.*`): unmapped block returns
  `kNoItemId`, a registered pair round-trips, re-registering a block id
  overwrites its previous mapping, multiple blocks can map to the same
  item.
- Verified via a real single-player run and a real two-process
  networked run reproducing Phase 21's exact same log lines - a true
  refactor, zero behavior change.
- `ctest` now 344/344 (non-bgfx, up from 340) / 347/347 (bgfx, up from
  343).
- Honestly scoped: client and server still each maintain their own
  separate table populated independently (not synced across the
  network); still not loaded from an external data file - a real
  runtime table populated by code, not a JSON/config-file content
  pipeline.

### Phase 21

- New `Action::CycleHotbar` (`engine/platform::Action`), bound to `R`
  on keyboard and a new "ITEM" touch button - closes Phase 18/19's
  remaining honest gap: `PlaceBlock` only ever requested `game:stone`
  since there was no way to choose otherwise.
- `VoxelClient` gained a `placeable_items` list (stone/grass/dirt) and
  a plain `selected_placeable_index`, cycled on an edge-detected
  `CycleHotbar` press. `PlaceBlock`'s handling (both single-player and
  networked branches) now reads the selected entry instead of the
  hardcoded `stone_id`/`stone_item_id`.
- No protocol change needed - `BlockAction::block_id` was already a
  plain field, and the server's Phase 19 `item_for_block`/place-
  validity gate already generalized to any item-backed block.
- Extended `LCU_VERIFY_BREAK_PLACE` with `kVerifyCycleHotbarFrame`
  (between break and place) so the existing headless hook now
  exercises break -> cycle -> place end to end.
- Verified via a real single-player run: "Selected placeable item:
  game:grass" then "Placing game:grass at world (0, 28, -1) (inventory:
  0)" - the exact position the grass block was broken from. Verified
  via a real two-process networked run: server logs "Applied
  BlockAction from <addr>: (0,29,-1) 0 -> 2" (block id 2 = game:grass,
  not the old hardcoded stone id 1), client logs "Requesting place
  game:grass..." then "Applied server BlockChange at world (0, 29,
  -1): block_id=2".
- No new unit tests - existing `Action::Count`-driven tests
  (`touch_input_test.cpp` and others) generalize to the new enumerator
  automatically. `ctest` unchanged at 343/343 (bgfx) / 340/340
  (non-bgfx).
- Honestly scoped: still no graphical hotbar (log-line-only selection
  feedback); selection is a plain fixed-list cycle, not driven by what
  the player's inventory actually holds.

### Phase 20

- Real disconnect detection: every `ClientState` now tracks
  `last_packet_time` (updated on every received packet); a new
  per-tick sweep erases (and logs) any client idle past a new
  `kClientTimeoutSeconds = 5.0f` constant, since UDP has no connection
  concept to detect a departure from otherwise.
- Interest-scoped chunk unloading: a new `compute_interest_set` lambda
  gives each `ClientState` a real `interest_set` (every chunk coord
  within load radius of its last streamed center); after any tick where
  a client moved, connected, or was pruned, the server unions every
  remaining client's interest set and unloads any currently-loaded
  chunk nobody needs - closes Phase 16's "the shared `World` only ever
  grows" gap.
- Real chunk persistence wired to a real trigger for the first time:
  before unloading, the chunk is saved via the already-existing,
  already-tested `lcu::serialization::save_chunk_to_file`; if a
  client's interest later returns to that coord, `load_chunk_from_file`
  is tried before falling back to regenerating it - regenerating an
  edited-then-evicted chunk would have silently reverted the edit.
- Fixed a design-time bug caught before building: `ClientState::
  last_streamed_center` changed from a plain `ChunkCoord` pre-set to
  the client's own spawn center, to `std::optional<ChunkCoord>`
  (default unset) - the old pre-set made the movement-triggered
  streaming loop treat a freshly-connected client as "no change since
  last tick, skip", silently skipping the real load-or-reload path for
  that client's own spawn-adjacent chunks whenever a previous client's
  departure had evicted them.
- Verified via real multi-process runs: a disconnect-timeout run
  ("Client 127.0.0.1:42566 timed out after 5.0s of silence,
  disconnecting" at ~5s); an isolated unload run ("Saved chunk
  (x,y,1) to disk before unloading" x12, "Unloaded 12 chunk(s) no
  connected client still needs (total 36 loaded)"); a chained run where
  a second, freshly-connecting client receives `ChunkData` for the
  exact same 12 coordinates the first run evicted, proving reload-from-
  disk rather than silent data loss.
- No new unit tests - composes already-tested primitives (`World`,
  `lcu::serialization::{save,load}_chunk_to_file`) under new
  server-side orchestration, exercised by the real runs above. `ctest`
  unchanged at 343/343 (bgfx) / 340/340 (non-bgfx).
- Honestly scoped: persistence is session-scoped (`<world>/chunks/`,
  not separately verified as surviving a deliberate server restart as
  a product feature); `kClientTimeoutSeconds` is an untuned placeholder;
  discovered but not fixed while stress-testing - a suspected
  `UnreliableSequenced` `u16` sequence-wraparound issue under extreme
  sustained packet volume, documented in NETWORKING.md/PROJECT_STATE.md
  rather than guessed at a fix.

### Phase 19

- `VoxelServer` registers `game:grass`/`game:dirt` items (capturing
  their `ItemId`s, previously discarded in Phase 18) - closes Phase
  15's remaining honest gap: server-side inventory only ever tracked
  `game:stone`, so Phase 18's new grass/dirt pickup was entirely
  client-optimistic with nothing server-side to correct it.
- New `item_for_block` lookup (the same direct 1:1 mapping
  `VoxelClient`'s `grant_item_for_broken_block` already used) replaces
  the single hardcoded `stone_id` check in `handle_block_action`'s
  break/place bookkeeping and place-validity gate - covers all three
  tracked items identically, not a stone-only special case.
- `send_inventory_update` renamed `send_inventory_updates`: sends one
  `InventoryUpdate` per tracked item (`{stone, grass, dirt}`) after
  every `BlockAction`, not just whichever the request happened to
  touch, so a stale guess for an unrelated tracked item also eventually
  corrects.
- `VoxelClient` needed no changes - its `InventoryUpdate` handler was
  already generic (keyed by whatever `item_id` arrives).
- Verified via a real two-process run (`LCU_VERIFY_BREAK_PLACE`): the
  player spawns on a grass block (Phase 17's layering), and the round
  trip converges cleanly - server logs "Applied BlockAction ...:
  (0,28,-1) 2 -> 0", client logs "Requesting break", "Picked up 1
  game:grass (inventory: 1)", "Applied server BlockChange ...
  block_id=0", zero warnings/errors.
- No new unit tests - pure generalization of already-tested
  `ItemRegistry`/`Inventory` orchestration. `ctest` unchanged at
  343/343 (bgfx) / 340/340 (non-bgfx).
- Honestly scoped: still only stone/grass/dirt are inventory-backed (no
  general, data-driven block-id-to-item-id mapping); no persistence
  across a disconnect/reconnect; placing still only ever places
  `game:stone`.

### Phase 18

- `VoxelClient` registers `game:grass`/`game:dirt` items (1:1 mapping
  to their block counterparts, matching `game:stone`'s own convention -
  not a shared loot-table drop). A new `grant_item_for_broken_block`
  helper replaces two previously-duplicated stone-only checks
  (networked and single-player break paths) with one lookup covering
  all three blocks - closes Phase 17's immediate follow-up gap (both
  new terrain blocks were real content, but breaking either granted no
  item).
- `VoxelServer` registers the same two items, same order, purely to
  keep both sides' `ItemId` spaces aligned - doesn't track either in a
  per-client `Inventory` yet.
- Verified via a real single-player run (`LCU_VERIFY_BREAK_PLACE`): the
  player spawns standing on a grass surface block (Phase 17's layering
  means the straight-down raycast now hits grass, not stone) - log
  shows "Breaking block at world (0, 28, -1)" then "Picked up 1
  game:grass (inventory: 1)", an unforced real exercise of the new
  path.
- Verified via a real two-process networked run: server logs "Applied
  BlockAction from <addr>: (0,28,-1) 2 -> 0" (block id 2 = game:grass),
  client logs "Requesting break", "Picked up 1 game:grass (inventory:
  1)", then "Applied server BlockChange ... block_id=0" - confirming
  the mapping works under server-authoritative editing too.
- No new unit tests - pure orchestration logic reusing already-tested
  `ItemRegistry`/`Inventory` primitives. `ctest` unchanged at 343/343
  (bgfx) / 340/340 (non-bgfx).
- Honestly scoped: placing grass/dirt isn't wired up (no hotbar/item-
  selection UI - `PlaceBlock` always places `game:stone`), and
  server-side authoritative tracking still only covers `game:stone` -
  grass/dirt pickup is client-authoritative and optimistic.

### Phase 17

- `lcu::world::worldgen::generate_terrain_chunk`'s signature changed
  from a single `solid_block` parameter to `(surface_block,
  subsurface_block, stone_block)` - the topmost solid layer is now
  `surface_block`, the next `kSubsurfaceDepth` (3) layers are
  `subsurface_block`, everything deeper is `stone_block`, closing a
  content gap flagged since Phase 3 ("single block type fills
  everything below the height").
- `VoxelClient`/`VoxelServer` both register `game:grass` and
  `game:dirt` block definitions - identical fields, identical
  registration order right after `game:stone` on both sides, so their
  `BlockId`s coincide by construction - and pass them into
  `generate_terrain_chunk`.
- Both new blocks are fully real content, not placeholders: real
  collision/meshing (entirely data-driven off `BlockRegistry`, never
  hardcoded by block id - no changes needed anywhere in
  physics/meshing/lighting), real network replication (a `ChunkData`
  snapshot's compressed bytes are whatever block ids the chunk actually
  holds), real break/place through the existing generic edit paths.
- Updated 4 existing unit tests and added 2 new ones
  (`SurfaceLayerIsExactlyOneBlockThickAtTheHeight`; renamed
  `ChunkFarBelowTerrainIsEntirelySolid` to `...IsEntirelyStone`) for the
  new layering behavior.
- Verified via a real single-player run (36-chunk world generates and
  loads with no crash, `LCU_VERIFY_BREAK_PLACE` round-trips cleanly)
  and a real two-process networked run (server logs `Sent 1 chunk(s) (1
  fragment(s))`, client logs `Applied server ChunkData for chunk (0, 1,
  0)` for a chunk now containing the layered grass/dirt/stone content,
  zero warnings/errors) - confirming the new content flows through the
  *existing* pipeline unmodified.
- `ctest` 343/343 passing (bgfx build) / 340/340 (non-bgfx build), up
  from 342/342 / 339/339.
- Honestly scoped: only `game:stone` has an item mapping (Phase 5), so
  breaking grass or dirt currently removes the block without granting
  an item. No climate/biome/caves/ores/structures/vegetation (brief
  section 21's later pipeline stages).

### Phase 16

- `VoxelServer` now re-checks every connected client's loaded-chunk
  range every tick (only when that client's current chunk coordinate
  has changed since last checked) and loads any not-yet-loaded chunk in
  range using the same logic the startup area already uses - closes
  Phase 14's honestly-flagged "connect-time-only sync" gap.
- Every newly-loaded chunk is broadcast as `ChunkData` to every
  connected client, not just whoever's movement triggered it. The
  server's shared `World` is deliberately append-only - it never
  unloads a chunk, since it's one instance shared across every
  connected client and unloading based on one client's position could
  break a different client still standing in that chunk.
- `VoxelClient` runs the mirror-image local half unconditionally: the
  same load-then-light-then-mesh sequence the initial spawn-area load
  already runs, triggered only when the player's own chunk coordinate
  changes.
- Fixed a real gap in Phase 14's `ChunkDataFragment` handler that this
  phase's dynamics exercise for the first time: a `ChunkData` for a
  coordinate the client hasn't locally streamed to yet used to be
  silently dropped ("isn't loaded locally, ignoring") - now the client
  creates a real chunk slot via `world.load_chunk` before overwriting
  it.
- Added a new headless verification hook, `LCU_VERIFY_MOVE_SECONDS` -
  holds `MoveForward` for that many real (wall-clock) seconds, since a
  frame-count-indexed hook (like `LCU_VERIFY_BREAK_PLACE`) doesn't work
  against the client's unthrottled main loop.
- Verified via two real multi-process runs: a two-process run where a
  client holds `MoveForward` for 6 real seconds (crossing the 16-block
  chunk boundary) shows the server logging `Streamed 1 newly-loaded
  chunk(s) into range (total 2 loaded)` and the client logging `Applied
  server ChunkData for chunk (0, 1, -1)`, zero warnings/errors. A
  three-process run adds a second, entirely stationary client that
  independently logs the identical line, proving the broadcast reaches
  every connected client, not just the one whose movement triggered it.
- No new unit tests - orchestration logic in the two executables built
  entirely on already-unit-tested primitives, verified via the real
  runs above. `ctest` unchanged at 342/342 (bgfx) / 339/339 (non-bgfx).
- Honestly scoped: still no interest-managed unloading; a client's own
  local streaming trigger and the server's are independent and only
  usually agree, not literally synchronized.

### Phase 15

- `VoxelServer` now registers the same `game:stone` item `VoxelClient`
  does and gives each connected client a real, authoritative 9-slot
  `lcu::items::Inventory` (`ClientState::inventory`) - closes Phase
  13's honestly-flagged gap: item pickup/placement-cost was entirely
  client-local and optimistic, with no server-side accounting and no
  refund on a rejected `BlockAction`.
- `handle_block_action`: placing `game:stone` is now rejected unless
  the requester actually holds one server-side (a new validity
  condition alongside the existing chunk-loaded/target-state checks); a
  successful break/place of it adds/removes one from that client's
  server-side inventory.
- Added `InventoryUpdate` (server->one client, `ReliableOrdered`) to
  `game::systems::protocol` - sent after every `BlockAction`, accepted
  or rejected, carrying that client's current authoritative
  `game:stone` count. 6 new unit tests.
- `VoxelClient` keeps its existing optimistic pickup/consumption
  (fires at request-send time, unchanged from Phase 13) but now
  reconciles it against every `InventoryUpdate`, the same pattern
  `PlayerCorrection` already uses for predicted movement.
- Verified via a real two-process run (`LCU_VERIFY_BREAK_PLACE`): the
  client's log shows the optimistic guess and the server's
  authoritative count actually disagree then converge in both
  directions - `Reconciled inventory item 1 to authoritative count 1
  (was 0)` right after the break, `... count 0 (was 1)` right after the
  place, each immediately followed by the matching `Applied server
  BlockChange` - not just that a message decoded.
- `ctest` 342/342 passing (bgfx build) / 339/339 (non-bgfx build), up
  from 337/337 / 334/334.
- Honestly scoped: only `game:stone` is inventory-gated (no general
  block-id-to-item-id mapping exists yet), and there's no persistence
  across a disconnect/reconnect.

### Phase 14

- Added `lcu::network::fragment_payload`/`FragmentReassembler`
  (`engine/network/fragmentation.h`/`.cpp`) - a generic, caller-side
  message split/rejoin layer for payloads too large for one UDP
  datagram, deliberately kept out of `Connection`/`PacketHeader` itself
  so existing small messages pay nothing for it. 11 new unit tests
  (in-order, out-of-order, duplicate, interleaved-concurrent, and
  malformed-too-short fragment delivery).
- Extracted `lcu::serialization::serialize_chunk_to_bytes`/
  `deserialize_chunk_from_bytes` as the real zstd-compression
  primitives; `save_chunk_to_file`/`load_chunk_from_file` are now thin
  wrappers around them - lets network chunk streaming reuse the exact
  same, already-tested compression/versioning/corruption logic instead
  of a parallel copy. 4 new unit tests, incl.
  `InMemoryBytesMatchFileBytes` pinning byte-for-byte equivalence with
  the pre-existing file-based path.
- Added `ChunkData` (server->client, logical - a full chunk snapshot,
  too large for one datagram) and `ChunkDataFragment` (server->client,
  `ReliableOrdered` - the actual wire message, one fragment of a
  fragmented `ChunkData`) to `game::systems::protocol`. 6 new unit
  tests.
- Added `World::loaded_chunk_coords()`. `VoxelServer` now sends a
  newly-connecting client a full `ChunkData` snapshot of every chunk it
  has loaded, right after `Welcome` and the `block_change_history`
  replay - fragmented via `fragment_payload` and sent
  `ReliableOrdered`.
- `VoxelClient` reassembles `ChunkDataFragment`s via a per-connection
  `FragmentReassembler`; once a `ChunkData` is complete, it fully
  overwrites the client's own (independently, deterministically
  generated - previously only ever *assumed* to match) local chunk with
  the server's authoritative one, then fully relights and remeshes it
  plus its six axis-adjacent neighbors.
- Closes the Reality Audit's other confirmed gap alongside Phase 13:
  chunk *data*, not just block *edits*, is now actually replicated -
  the client's world is received from the server, not merely
  coincidentally identical to it.
- Verified via two real two-process runs: a `mobile_low`-profile
  (1-chunk world) run logs `Sent 1 chunk(s) (1 fragment(s))`
  server-side and `Applied server ChunkData for chunk (0, 1, 0)`
  client-side; a `desktop`-profile (36-chunk world) run logs `Sent 36
  chunk(s) (36 fragment(s))` and exactly 36 matching `Applied server
  ChunkData` lines client-side, zero warnings/errors either run.
- 21 new unit tests total (11 fragmentation + 4 in-memory serialization
  + 6 `ChunkData`/`ChunkDataFragment` protocol). `ctest` 337/337
  passing (bgfx build) / 334/334 (non-bgfx build), up from 316/316 /
  313/313.
- Honestly scoped: a one-shot full sync sent once on connect, not
  interest-managed by distance and not re-streamed as either side's
  loaded-chunk set changes afterward (see NETWORKING.md "Chunk network
  streaming").

### Phase 13

- Added `BlockAction` (client->server, `ReliableOrdered`) and
  `BlockChange` (server->all-clients broadcast, `ReliableOrdered`) to
  `game::systems::protocol` - block edits are now replicated and
  server-authoritative, closing the single most consequential gap a
  Reality Audit of the existing codebase found (block edits previously
  only ever mutated a client's own local `World`, invisible to the
  server or any other client).
- `VoxelServer::handle_block_action` validates every request (target
  chunk loaded; break targets a non-air block; place targets an air
  block with a registered `block_id`; target within
  `kMaxBlockActionRange` of the requester's own server-known position -
  brief section 20's "never trust client data") before applying it to
  the server's `World` and broadcasting the result to every connected
  client, including the requester itself - no client mutates its own
  `World` speculatively for a block edit (see DECISIONS.md).
- `VoxelServer` now keeps every applied edit in order
  (`block_change_history`) and replays it in full to a newly connecting
  client right after its `Welcome`, so a late joiner catches up on
  edits that happened before it connected instead of silently
  disagreeing with everyone else's world forever.
- `VoxelClient`'s item pickup/consumption stays client-local and
  optimistic (fires at request-send time, not at `BlockChange`-received
  time - every client receives every broadcast and can't tell whose
  edit it was from the message alone) - a real, honestly-scoped
  simplification: no server-side inventory yet, so a rejected request
  currently isn't refunded (see NETWORKING.md "What's deferred").
- Found and fixed two real bugs while verifying this feature by
  actually running it, not just by inspection: an initial
  implementation used `continue` inside the place-block branch that
  would have skipped the rest of that frame's loop body (rendering,
  network flush, frame counting); and networked-mode breaking initially
  gave the player no item at all (the pickup logic only existed in
  single-player's code path), which would have made placing impossible
  in multiplayer since it requires an item.
- Verified via a real three-process run (one `VoxelServer`, two
  independent `VoxelClient`s): the server logs `Applied BlockAction`
  for both a break and a place; the acting client logs the item pickup/
  consumption and `Applied server BlockChange` for both edits; a
  second, purely observing client - which never touched either block
  itself - independently logs the identical `Applied server
  BlockChange` lines, confirming its `World` genuinely converged with
  the other two processes. A separate run confirms the late-joiner
  catch-up: a client connecting only after both edits already happened
  still receives and applies both via the replayed history.
- 10 new unit tests for `BlockAction`/`BlockChange` encode/decode
  (round-trip, rejection of truncated/wrong-type/invalid-enum
  payloads). `ctest` 316/316 passing (bgfx build) / 313/313 (non-bgfx
  build), up from 308/308 / 305/305.

### Phase 12

- `engine/audio::AudioEngine`: RAII wrapper around one `SDL_AudioStream`
  (`SDL_OpenAudioDeviceStream`, 44.1kHz stereo float). Only
  `audio_engine.cpp` includes `<SDL3/SDL_audio.h>`, mirroring
  `engine/scripting`'s Lua-header confinement. Initializes its own
  `SDL_INIT_AUDIO` subsystem (reference-counted by SDL, same pattern
  `engine/platform::Window` uses for `SDL_INIT_VIDEO`). A failed
  `init()` (no device - most CI, this sandbox without
  `SDL_AUDIODRIVER=dummy`) is logged and non-fatal; `play()` becomes a
  silent no-op.
- `engine/audio::generate_sine_wave`: real, own-created procedural PCM
  tone content - no WAV/asset-loading pipeline exists yet, and any
  checked-in audio asset would need to be this project's own work
  anyway (GPL-3.0/own-IP-only, brief section 12).
- `engine/audio::{compute_stereo_pan, distance_attenuation}`: pure-math
  positional audio - pan by lateral angle to the listener, linear
  distance falloff. No SDL dependency, fully unit tested.
- `VoxelClient`: breaking/placing a block now plays a real synthesized,
  positionally-panned/attenuated tone through `AudioEngine`.
- `engine/ui::draw_debug_overlay`: a real on-screen HUD via bgfx's
  built-in VGA-style debug-text buffer - `Renderer` gained
  `draw_debug_text`/`clear_debug_text` (wrapping
  `bgfx::dbgTextPrintf`/`dbgTextClear`, `BGFX_DEBUG_TEXT` enabled in
  `Renderer::init`), keeping bgfx access confined to
  `engine/rendering` per ARCHITECTURE.md. Draws live FPS and a legend
  for every mobile touch-control button, at the exact positions
  `TouchInputBackend` hit-tests against.
- Promoted the touch-control button layout out of `touch_input.cpp`'s
  private `constexpr` array into `lcu::platform::kTouchButtonLayout`
  (`touch_control_layout.h`), a shared source of truth both hit-testing
  and on-screen drawing read from - closes the Phase 10 "a player would
  currently be dragging/tapping blind" limitation and the long-standing
  "debug overlay is a log line, not on-screen" limitation, both for
  real.
- Verified via real runs: `AudioEngine initialized: 44100 Hz, stereo
  float` under `SDL_AUDIODRIVER=dummy` with the break/place round trip
  completing with no crash; a full bgfx (`Noop` backend) `VoxelClient`
  run from startup to shutdown with `draw_debug_overlay` executing
  every frame, no assert/crash.
- `ctest` 308/308 passing (bgfx build) / 305/305 (non-bgfx build), up
  from 291/291 / 288/288 - 15 new tests (`GenerateSineWave`,
  `ComputeStereoPan`, `DistanceAttenuation`).
- This closes the entire 12-phase queue from the project brief.

### Phase 11

- Added Google Benchmark as a build dependency (FetchContent, pinned to
  v1.9.1, opt-in alongside the rest of `tools/` via `LCU_BUILD_TOOLS`) -
  same vendor/ecosystem as GoogleTest, no new justification needed for
  exactly this job.
- `tools/benchmark::VoxelBenchmarks`: 15 benchmark cases against real
  engine code (not synthetic stand-ins) covering every area this
  phase's task list named - voxel access (`ChunkStorage::set_block`/
  `block_at`), chunk gen (`worldgen::generate_terrain_chunk`), meshing
  (`mesh_chunk_greedy` on both a fully-solid and a checkerboard chunk),
  lighting (`compute_block_light`/`compute_sky_light`), physics
  (`raycast`/`move_and_collide`), serialization+compression
  (`save_chunk_to_file`/`load_chunk_from_file` - zstd runs inside
  these), network (a `Connection` `ReliableOrdered` send+deliver round
  trip), and entity sim (`update_ai_wander` at 10/100/1000 entities).
- Actually run in this sandbox, in both a `Development` build (flagged
  "Library was built as DEBUG" by Google Benchmark itself, since this
  project's `Development` build type applies no optimization flags) and
  a `Release` build (clean run, real optimized numbers - see
  `BUILD_STATUS.md`). One concrete finding: greedy-meshing a
  checkerboard chunk (no face-merging possible) takes ~15x longer than
  a fully-solid chunk of the same size (1.45ms vs. 94us) - real evidence
  the algorithm's merging step does substantial work. No code changed
  based on these numbers yet - nothing has shown a need to.

### Phase 10

- `engine/platform::TouchInputBackend`: maps a frame's active finger
  touches onto the existing `Action`/`InputState` abstraction
  `KeyboardInputBackend` already drives - a twin-virtual-stick layout
  (movement drag on the screen's left half, look drag on the right,
  both dead-zone-thresholded into the existing discrete Move*/Look*
  actions) plus fixed button rects for Jump/Interact/PlaceBlock/Sprint/
  Crouch/Inventory. `MovementInput`/`FirstPersonCamera`/the break-place
  loop need zero changes - they only ever read `InputState`. Pure
  logic, no SDL dependency. 13 new unit tests.
- `lcu::core::{QualityProfile, chunk_load_settings_for,
  parse_quality_profile}`: device performance tiers (MobileLow/
  MobileMedium/MobileHigh/Desktop). Lives in `engine/core`, not
  `engine/platform`, since `VoxelServer` needs it too and must stay
  SDL/bgfx-free. `Desktop` matches this project's pre-existing
  hardcoded chunk-load radius/vertical range exactly - `kLoadRadiusXZ`/
  `kMinChunkY`/`kMaxChunkY` in `VoxelClient`/`VoxelServer` are now
  computed from this instead. 4 new unit tests.
- `VoxelClient`/`VoxelServer`: new `LCU_QUALITY_PROFILE` env var
  selects a quality profile (falls back to `Desktop` for an
  unrecognized value). Verified via real runs: default and an invalid
  value both still log "Loaded 36 chunks" exactly as before this
  phase; `mobile_low`/`mobile_high` log "Loaded 1 chunks"/"Loaded 27
  chunks".
- Re-verified `CMakePresets.json`'s `android-arm64`/`ios` presets:
  `cmake --preset android-arm64` reaches and fails only at Android's
  own NDK-detection step, confirming the preset is structurally sound.
  No Gradle/Xcode project generated - deliberately deferred until an
  actual toolchain exists to build/run one against (see DECISIONS.md).
- `ctest` 291/291 passing (bgfx build) / 288/288 (non-bgfx build), up
  from 274/274 / 271/271 - 17 new tests across `TouchInputBackend` and
  `QualityProfile`/`parse_quality_profile`.

### Phase 9

- Added Lua 5.4.7 as a build dependency: official upstream
  (`github.com/lua/lua`, which ships no CMake support) fetched via
  `FetchContent_Populate`, built from its own `onelua.c` amalgamation
  with `-DMAKE_LIB` to produce just the embeddable library (no `main()`).
  Root `CMakeLists.txt` now declares `LANGUAGES CXX C` for it.
- `engine/scripting::LuaState`: RAII wrapper around one Lua VM. Opens
  only the base/table/string/math standard libraries - not `io`/`os`/
  `package` - so a mod script has no filesystem/process access by
  default. Forward-declares `lua_State` so `<lua.h>` is only ever
  `#include`d inside `engine/scripting`'s and `engine/modding`'s own
  `.cpp` files (mirrors the existing bgfx-header-confinement rule).
  `register_function` exposes a native C function to Lua's global
  namespace with a stateful `void*` upvalue, the standard technique for
  binding a C++ object to Lua's C-style callback ABI. 7 new unit tests.
- `engine/modding::EventBus`: a named pub/sub bus - mod scripts call
  `lcu.subscribe(event_name, fn)`, the engine calls a typed
  `emit_<event>()` method (currently just `emit_block_broken`) at the
  real moment that event happens. An erroring handler is logged and
  skipped without blocking the remaining subscribers. 7 new unit tests.
- `engine/modding::{bind_block_registry, bind_item_registry}`: expose
  `register_block(namespaced_id, display_name, is_transparent,
  has_collision)`/`register_item(namespaced_id, display_name,
  max_stack_size)` to Lua, writing directly into the given
  `BlockRegistry`/`ItemRegistry` - mod content and base game content are
  otherwise indistinguishable. 6 new unit tests.
- `engine/modding::ModLoader`: enumerates immediate subdirectories of a
  mods directory, running each one's fixed `<mod>/init.lua` entry point
  against one shared `LuaState`. A mod without an `init.lua`, or whose
  script errors, is logged and skipped - not fatal to the others.
  Deliberately no manifest/dependency/version format yet. 6 new unit
  tests.
- `mods/example_mod/init.lua`: a real, working demonstration mod -
  registers `example_mod:magic_stone`/`example_mod:magic_wand`,
  subscribes to `block_broken`, and logs every block it sees broken.
- `VoxelClient`/`VoxelServer`: both now construct their own `LuaState`,
  bind their own block/item registries, and call
  `ModLoader::load_all("mods")` at startup (guarded by the new
  `LCU_ENABLE_SCRIPTING` compile definition, on by default via
  `LCU_BUILD_SCRIPTING`). `VoxelClient` fires a real
  `EventBus::emit_block_broken()` at the exact point in the existing
  break-handling code where a block actually becomes air.
  `VoxelServer` also constructs an `EventBus`/`ItemRegistry` purely so a
  mod script shared between both hosts has a uniform Lua API surface,
  even though the server never itself calls `emit_block_broken` (block
  edits aren't replicated yet).
- Verified via real runs, not just unit tests: `VoxelServer` logs its
  mod's registrations and `Loaded 1 mod(s) from 'mods'`; `VoxelClient`
  under `LCU_VERIFY_BREAK_PLACE` additionally logs `[example_mod]
  block_broken #1: block id 1 broken at (0, 28, -1)` immediately after
  breaking that exact block.
- `ctest` 274/274 passing (bgfx build) / 271/271 (non-bgfx build), up
  from 247/247 / 244/244 - 27 new tests across `LuaState`, `EventBus`,
  `RegistryBindings`, `ModLoader`.

### Phase 8

- `engine/replication::PositionInterpolator`: buffers timestamped
  position samples and linearly interpolates between them for a
  slightly-delayed render time, clamping (never extrapolating) past
  either end of the buffer. 9 new unit tests.
- `engine/replication::PredictionBuffer<State, Input>`: generic
  client-side prediction + server reconciliation - `predict_and_record`
  applies an input immediately and remembers it; `reconcile` discards
  acknowledged history and replays what's left on top of an
  authoritative correction. Generic over any pure step function, not
  tied to player movement. 6 new unit tests, including one wiring the
  real `lcu::physics::PlayerPhysicsState`/`integrate_player` as the
  concrete step function with a hand-computed expected result.
- `game::systems::protocol`: the shared application-level messages
  `VoxelClient` and `VoxelServer` both use now
  (`Welcome`/`Heartbeat`/`EntityState`/`PlayerInput`/`PlayerCorrection`),
  replacing `VoxelServer`'s own local copies from Phase 7 - one
  definition instead of two that could silently drift apart. 11 new
  unit tests (round-trips, negative floats, malformed-payload
  rejection).
- `VoxelServer`: tracks one real `lcu::physics::PlayerPhysicsState` per
  connected client now, driven by received `PlayerInput` messages
  through the same physics `VoxelClient` runs (server-authoritative
  movement, not an echo), with a `dt` ceiling clamp as a light
  anti-cheat measure. Broadcasts a per-client `EntityState` filtered by
  a real interest-management distance check (`kInterestRadius`), and a
  periodic `PlayerCorrection`.
- `VoxelClient`: new `LCU_CONNECT_PORT` env var enables a real networked
  mode (loopback IPv4 only) alongside the existing single-player path,
  which is completely unaffected when unset. When networked: connects,
  logs the real `Welcome`; predicts local player movement immediately
  via `PredictionBuffer` and reconciles against `PlayerCorrection`;
  stops simulating AI locally and instead renders each remote entity's
  `EntityState` samples through its own `PositionInterpolator`.
- Verified via a real two-process run: an actual `VoxelClient` connected
  to an actual `VoxelServer` over real loopback UDP, received a genuine
  Welcome (`world_seed=1337 tick_rate=20`), rendered all 3 remote AI
  entities' interpolated positions matching the server's live
  simulation, and had its player position predicted, sent, and
  reconciled - the full loop exercised end to end, not simulated.
  Single-player mode reverified byte-for-byte unchanged in both build
  configs.
- New `NETWORKING.md` sections documenting the replication protocol,
  prediction/reconciliation flow, interest management, and what's
  deferred (chunk streaming - needs message fragmentation `Connection`
  doesn't have; block-edit replication).
- 26 new unit tests. `VoxelTests` now at 247/247 passing (bgfx build) /
  244/244 (non-bgfx build).

### Phase 7

- `engine/network::UdpSocket`: cross-platform (POSIX/Winsock, selected
  at compile time) non-blocking IPv4 UDP socket wrapper - `bind`/
  `send_to`/`try_receive`. Winsock startup/cleanup is reference-counted
  so callers never have to think about it. 9 new unit tests incl. a real
  loopback send/receive round-trip.
- `engine/network::{Channel, PacketHeader, sequence_greater_than}`: the
  pure, independently-tested building blocks. `Channel` names all four
  semantics `ARCHITECTURE.md` commits to
  (`UnreliableUnordered`/`UnreliableSequenced`/`ReliableUnordered`/
  `ReliableOrdered`); `PacketHeader` is a 4-byte wire header
  (type/sequence/channel) with round-trip serialization;
  `sequence_greater_than` is the standard wraparound-correct `u16`
  sequence comparison (the same technique TCP uses for its own
  sequence numbers).
- `engine/network::Connection`: implements all four channel semantics
  over an abstract byte-packet transport - it never touches a socket
  itself, only produces/consumes raw packets (the same
  dependency-injection shape as `physics::raycast`'s `is_solid`
  predicate), so the protocol logic (ordering, deduplication,
  retransmission timing) is fully unit-tested with zero real I/O.
  Reliable channels use per-channel sequence counters and ack-based
  retransmission (fixed interval - see DECISIONS.md); `ReliableOrdered`
  additionally buffers out-of-order arrivals and drains them in
  sequence. See the new `NETWORKING.md` for the full wire format.
- 46 new unit/integration tests, including two full loopback integration
  tests running real `Connection` pairs over real `UdpSocket`s on
  `127.0.0.1` - one deliberately drops the first real UDP datagram sent
  and confirms retransmission recovers it, not a hypothetical case a
  mock would assume away. `VoxelTests` now at 221/221 passing (bgfx
  build) / 218/218 (non-bgfx build).
- `VoxelServer`: replaced the Phase 0 sleep-only placeholder tick loop
  with a real one. Generates/loads a real 36-chunk `World`, runs the
  same wandering-AI simulation as `VoxelClient` (`engine/ecs` +
  `game::systems::update_ai_wander`) every tick regardless of whether
  any client is connected, and listens for real UDP connections - a
  peer is "connected" the moment the server sees any datagram from its
  address, and gets a real `ReliableOrdered` Welcome message (world
  seed + tick rate) plus a per-tick `UnreliableSequenced` Heartbeat
  (tick number + live entity count) from then on. Verified via a real
  two-process test: a standalone Python UDP client connects to a
  running `VoxelServer` and receives the genuine handshake and live
  heartbeats (see `NETWORKING.md`/`BUILD_STATUS.md` for the exact
  reproduce steps). `ldd` reconfirmed zero SDL/bgfx dependency.
- New `NETWORKING.md`: wire format, channel semantics, ack/retransmit
  behavior, the application-level Welcome/Heartbeat message format, and
  what's verified vs. deferred.

### Phase 6

- `engine/ecs::Registry`: generation-checked `EntityId` handles (a stale
  handle from a destroyed entity never aliases whatever later reuses its
  slot) and sparse-set `ComponentPool<T>` per component type (dense
  contiguous storage for cache-friendly iteration, swap-and-pop removal
  so dense arrays never develop holes). `create`/`destroy_entity`,
  `add`/`get`/`has`/`remove_component`, `pool_for<T>()` for dense
  iteration over every live component of a type. No query DSL,
  archetypes, or multithreaded system dispatch - not needed yet. 13 new
  unit tests.
- `engine/lighting::LightStorage<EdgeLength>`: packed 4-bit sky + 4-bit
  block light per voxel, same flat-array layout as `ChunkStorage`.
  `compute_block_light`/`compute_sky_light`: the one-time initial
  per-chunk flood (from every `BlockDefinition::light_emission` source,
  and a top-down per-column sky fill). `propagate_added_block_light`/
  `unpropagate_block_light`: true incremental local updates for a single
  block add/remove - the standard two-phase BFS removal algorithm
  (darken everything strictly dimmer than the retracted source, collect
  still-validly-lit boundary cells, re-flood from them), directly
  satisfying brief section 24's "local updates, not full recompute"
  rather than re-flooding the whole chunk per edit. Header-only
  (templated on edge length, like `mesh_chunk_greedy`). Single-chunk
  scope for now (no cross-chunk light bleed) - see DECISIONS.md. 12 new
  unit tests, including an exact-match check between the incremental add
  path and a full recompute, and a two-source removal test verifying the
  refill phase correctly reproduces what a solo-source recompute would
  give.
- `game::components::{Position, AIWander}` and
  `game::systems::update_ai_wander`: a real gameplay-layer `engine/ecs`
  consumer. An entity with both components idles for a random duration,
  then walks toward `AIWander::target` at `AIWander::speed`; on arrival,
  picks a new target within a configurable radius and idles again. An
  explicit `std::mt19937` (not a hidden global RNG) keeps this
  deterministic and testable, matching `worldgen`'s "no hidden global
  state" approach. 6 new unit tests.
- `game::systems::DayNightCycle`: tracks elapsed time through a
  repeating cycle and reports a cosine-curve sky light scale (1.0 at
  noon, a dim nonzero floor at midnight, never fully black). Not yet
  wired into any renderer or `engine/lighting` data - logged only for
  now. 7 new unit tests.
- `VoxelClient`: computes real per-chunk block+sky light at load time
  (`chunk_light`, a `ChunkCoord -> Light` map alongside the existing GPU
  mesh map) and keeps it correct through every break/place edit via the
  incremental propagate/unpropagate primitives plus a per-column sky
  light refresh, instead of re-flooding the whole chunk on every edit.
  Spawns 3 wandering AI entities in a ring around spawn and a
  `DayNightCycle`, both updated every frame. Also fixes a real
  off-by-one found while verifying this: `terrain_height()` returns the
  topmost *solid* block's Y (worldgen.cpp: `world_y <= height` is
  solid), so the player (and now AI) previously spawned with feet
  embedded one block into the ground instead of resting on top of it -
  spawn Y is now `terrain_height() + 1`.
- `VoxelTests` now at 187/187 passing (bgfx build) / 184/184 (non-bgfx
  build). Verified via a real headless run: "Sky light 5 blocks above
  spawn column: 15", real (deterministic, seeded) AI entity positions
  logged, "Day/night: time_of_day=0.000 sky_light_scale=0.550", and the
  existing `LCU_VERIFY_BREAK_PLACE` break-then-place round-trip still
  holds after the spawn-height fix.

### Phase 5

- `engine/items::{ItemRegistry, ItemDefinition, ItemId, kNoItemId}`:
  namespaced, datadriven item registry mirroring `engine/voxel`'s
  `BlockRegistry`/`BlockDefinition` pattern - `kNoItemId` (0) is always
  "no item", auto-registered by the constructor. 5 unit tests.
- `engine/items::{ItemStack, Inventory}`: fixed-size slot-based item
  storage. `add_item` tops up existing matching partial stacks before
  spilling into empty slots, respecting each item's
  `ItemDefinition::max_stack_size`, and returns any leftover that
  didn't fit; `remove_item`/`count_item` round it out. No UI/drag-drop
  yet - no inventory screen exists to need one. 10 unit tests.
- `engine/items::{RecipeRegistry, ShapedRecipe, ShapelessRecipe}`:
  shaped recipes matched by trimming the *queried* crafting grid to its
  bounding box and comparing cell-for-cell at a single orientation (no
  mirroring - a documented simplification, no recipe has needed it
  yet); shapeless recipes matched by exact ingredient-multiset
  comparison (extra unrelated items in the grid correctly fail to
  match, same as real crafting games). No crafting-UI caller yet -
  tested standalone, same as `BlockRegistry`/`ItemRegistry` were before
  their first real callers existed. 9 unit tests.
- `VoxelClient`: block-break now has a real item consumer. Breaking
  registers and drops one `game:stone` item into a new 9-slot player
  `Inventory`; placing now consumes one stone item instead of being
  free, refunding it if the placement target's chunk turns out not to
  be loaded (a real edge case found and fixed while wiring this up).
  Verified via the existing `LCU_VERIFY_BREAK_PLACE` headless hook:
  break logs "Picked up 1 game:stone (inventory: 1)", place logs
  "... (inventory: 0)".
- 24 new unit tests across `ItemRegistry`/`Inventory`/`RecipeRegistry`.
  `VoxelTests` now at 149/149 passing (bgfx build) / 146/146 (non-bgfx
  build).

### Phase 4

- `engine/physics::raycast`: voxel DDA (Amanatides & Woo) against
  `World`, stepping one voxel boundary at a time regardless of chunk
  size, predicate-driven solidity. 9 unit tests incl. a hand-computed
  exact-distance case and the origin-starts-inside-solid edge case.
- `engine/physics::{AABB, move_and_collide, PlayerPhysicsState,
  integrate_player}`: axis-independent Y->X->Z AABB-vs-voxel collision
  resolution, gravity, jump, and single-ledge auto-stepping. Found and
  fixed a real grounding-detection bug during development (a stationary
  grounded player briefly reported ungrounded because "grounded" was
  read off whether *this frame's* downward movement collided, not
  whether the player was actually resting on something) via a dedicated
  small downward ground-probe, decoupling the two - see DECISIONS.md.
  18 unit tests, including a regression test for that exact bug and
  hand-computed auto-step clamp positions.
- `engine/player::{FirstPersonCamera, movement_direction_from_input}`:
  yaw/pitch first-person camera matching the existing `Mat4::look_at`
  -Z-forward convention (pitch clamped just under the poles), and
  WASD-relative normalized movement direction decoupled from pitch.
  12 unit tests.
- `engine/platform::Action` gained `LookUp/Down/Left/Right` (arrow keys)
  and `PlaceBlock` (`F`) - arrow-key look is a real, immediately usable
  interim control scheme standing in for mouse-look until SDL
  relative-mouse-mode plumbing exists (see DECISIONS.md).
- `VoxelClient` rewritten from Phase 2's single hardcoded placeholder
  chunk to a real vertical slice: loads a 36-chunk area of `World`-
  driven terrain around spawn, spawns a physics-driven player resting
  on the generated surface, drives the camera from arrow-key look input
  and WASD movement through `integrate_player`, raycasts from the
  camera every frame, and mutates the world on edge-detected Interact
  (break, `E`)/PlaceBlock (place, `F`) presses - remeshing and
  re-uploading not just the edited chunk but any neighbor chunk sharing
  the mutated block's boundary, so cross-chunk face culling stays
  correct after an edit at a chunk seam.
- New `LCU_VERIFY_BREAK_PLACE` env var: since this sandbox has no real
  keyboard/mouse, synthesizes an Interact press at frame 3 and a
  PlaceBlock press at frame 6, driving the exact same edge-detected
  `InputState` code path a real key press would. Verified via a real
  run: breaks a block, logs it, then the next raycast (now reaching one
  block deeper) places a new block back at the exact same world
  position - a genuine round-trip through mutate-world -> remesh ->
  re-upload, not a mock of it. This closes brief section 80's slice 1
  vertical slice (save/load already existed from Phase 3; persisting a
  live session's edits to disk still has no trigger wired up - see
  PROJECT_STATE.md "Known Limitations").
- 39 new unit tests across `Raycast`/`Collision`/`PlayerPhysics`/
  `FirstPersonCamera`/`MovementInput`. `VoxelTests` now at 125/125
  passing (bgfx build) / 122/122 (non-bgfx build).

### Phase 3

- `engine/world::World`: sparse chunk table keyed by `ChunkCoord`,
  lifecycle state machine (Unloaded -> Requested -> Generating ->
  Generated, matching ARCHITECTURE.md), distance-based streaming with
  load/unload radius hysteresis.
- `engine/world::worldgen`: deterministic seeded value-noise terrain
  height (continental+terrain pipeline stage only). Same seed+coord
  always produces the same height; different seeds differ; adjacent
  columns change smoothly.
- `engine/serialization::chunk_serializer`: versioned, zstd-compressed
  chunk save/load with corruption detection (zstd content checksum) and
  version-mismatch detection - new dependency, zstd v1.5.7 (see
  DECISIONS.md/third_party/README.md).
- `std::hash<ChunkCoord>` added so it can key `World`'s chunk table.
- 27 new unit tests across `World`/`worldgen`/`chunk_serializer`,
  including exhaustive round-trip and corruption-detection coverage for
  save/load. `VoxelTests` now at 88/88 passing (bgfx build) / 85/85
  (non-bgfx build).

### Phase 0

- Repository structure created (`engine/`, `game/`, `client/`, `server/`,
  `tools/`, `third_party/`, `mods/`, `examples/server/`, `tests/`).
- Governance docs added: `PROJECT_STATE.md`, `ROADMAP.md`,
  `TASK_QUEUE.md`, `BUILD_STATUS.md`, `BUILDING.md`, `ARCHITECTURE.md`,
  `DECISIONS.md`, `CHANGELOG.md`.
- CMake build system: root `CMakeLists.txt`, `CMakePresets.json`
  (linux/windows/macos/android-arm64/ios), `third_party/CMakeLists.txt`
  fetching fmt, SDL3, GoogleTest and bgfx.cmake via pinned FetchContent.
- `engine/core` (types, fmt-backed logging, assertions) and `engine/math`
  (Vec3/Vec4/Mat4), both unit tested.
- `VoxelServer`: headless dedicated server entry point, verified via
  `ldd` to carry zero SDL/bgfx dependency.
- `VoxelTests`: 12 GoogleTest cases (Log/Vec3/Mat4), all passing.

### Phase 1 (in progress)

- `engine/platform::Window`: SDL3-backed window + event pump. Verified
  headlessly (`SDL_VIDEODRIVER=dummy`) via `VoxelClient`.
- `engine/platform::get_native_window_handle`: extracts the platform
  native window handle from SDL3 (X11/Wayland/Win32/Cocoa/UIKit/Android),
  returning null when unavailable (e.g. dummy driver).
- `engine/rendering::Renderer`: bgfx init/frame/shutdown wrapper. Falls
  back to bgfx's `Noop` backend when no native handle is available.
  Verified headlessly: 5-frame clear loop completes cleanly against the
  `Noop` backend. Real GPU backend rendering to an actual screen is not
  yet verified (no GPU/display in the dev sandbox).
- `engine/platform::InputState`/`KeyboardInputBackend`: action-based
  input (`MoveForward`/`Jump`/`Interact`/...) decoupled from raw SDL
  scancodes. Keyboard backend only; gamepad/touch deferred to when
  something needs them (Phase 4/10).
- `engine/debug::FrameStats`: minimal FPS/frame-time accumulator, wired
  into `VoxelClient`'s loop as a once-per-second log line. Verified with
  a real running loop, not just unit tests.
- `VoxelTests` now at 18/18 passing (added `InputState.*`,
  `FrameStats.*`).

### Phase 2 (in progress)

- `engine/voxel::ChunkStorage<EdgeLength>`/`Chunk`: flat, contiguous
  `BlockId` array, no per-block C++ instance, default 16^3, alternative
  sizes proven via `ChunkStorage<8>`.
- `engine/voxel::world_to_chunk_and_local`: correct floor-division
  world->chunk+local coordinate splitting (handles negative coordinates
  correctly, unlike naive truncating division).
- `engine/voxel::BlockRegistry`/`BlockDefinition`: namespaced, datadriven
  block definitions (`game:stone`, `example_mod:magic_stone`); air always
  id 0.
- `VoxelTests` now at 39/39 passing (added `Chunk.*`, `ChunkStorage.*`,
  `ChunkCoord.*`, `BlockRegistry.*` - 27 new cases, including an
  exhaustive chunk-volume injectivity sweep and a coordinate-math
  round-trip sweep).
- `engine/jobs::JobSystem`: worker-thread pool with priority scheduling,
  dependency graphs (with cascading cancellation), and cancellation of
  not-yet-started jobs. Required before greedy meshing can run off the
  main thread. 12 new unit tests; `VoxelTests` now at 51/51 passing.
  Additionally verified via 200 repeated test-suite runs and 50 runs
  under ThreadSanitizer, zero failures/data races either way.
- `engine/voxel::mesh_chunk_greedy`: axis-sweep greedy meshing producing
  a renderer-agnostic `ChunkMesh` (opaque layer; transparent/water
  layers exist structurally, populated once a transparent block exists
  to motivate their face rules). Registry-driven opacity. Triangle
  winding verified via a geometric cross-product check against each
  triangle's stored normal. 8 new unit tests; `VoxelTests` now at 59/59
  passing.
- `engine/rendering::upload_chunk_mesh_layer`/`destroy_gpu_chunk_mesh`:
  real bgfx `VertexBuffer`/`IndexBuffer` creation from a `ChunkMeshLayer`
  (`Lcu::Rendering` now links bgfx `PUBLIC` instead of `PRIVATE`, since
  this header exposes bgfx types). No shader/draw-call yet - bgfx needs
  a compiled shader program to draw anything, and no shader compiler is
  built in this repo; documented as the explicit next step. 3 new unit
  tests.
- `VoxelClient` now exercises the full Phase 2 pipeline end-to-end:
  registers a placeholder `"game:stone"` block, builds a flat ground
  slab `Chunk`, dispatches `mesh_chunk_greedy` through
  `engine/jobs::JobSystem` (its first real caller), and uploads the
  result to GPU buffers. Verified via a real headless run: "Meshed
  placeholder chunk: opaque 24 vertices / 36 indices" then "Uploaded
  chunk mesh to GPU buffers: valid=true index_count=36". `VoxelTests`
  now at 62/62 passing (bgfx build) / 59/59 (non-bgfx build).
- `LCU_BUILD_SHADER_TOOLS` (opt-in, default OFF): builds bgfx's
  `shaderc` and compiles `client/shaders/{vs_chunk,fs_chunk}.sc` (a
  minimal directional+ambient lit shader, no texturing yet) into
  spirv/glsl/essl binaries. `engine/rendering::load_chunk_program` loads
  them at runtime; `Renderer` gained `begin_frame`/`submit_chunk_mesh`/
  `end_frame` (replacing `render_clear_frame`) so a real
  `bgfx::submit()` draw call happens between clear and frame advance.
  Verified via a real `VoxelClient` run: "Chunk shader program
  valid=true" followed by 3 clean frames with the draw call executing
  under bgfx's `Noop` backend - the full chunk -> mesh -> GPU buffers ->
  shader -> draw call pipeline now runs end to end. What it looks like
  on a real GPU/display remains unverified (no display in this sandbox).
