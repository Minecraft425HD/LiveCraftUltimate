# Changelog

All notable changes to this project are recorded here, newest first.

## Unreleased — Phase 0 / Phase 1 / Phase 2 / Phase 3 / Phase 4 / Phase 5 / Phase 6 / Phase 7 / Phase 8 / Phase 9 / Phase 10 / Phase 11 / Phase 12 / Phase 13 / Phase 14 / Phase 15 / Phase 16 / Phase 17 / Phase 18 / Phase 19 / Phase 20 / Phase 21 / Phase 22 / Phase 23 / Phase 24 / Phase 25 / Phase 26 / Phase 27 / Phase 28 / Phase 29 / Phase 30 / Phase 31 / Phase 33 / Phase 34 / Phase 35 / Phase 36 / Phase 37 / Phase 38 / Phase 39 / Phase 40 / Phase 41 / Phase 42 / Phase 43 / Phase 44 / Phase 45 / Phase 46 / Phase 47 / Phase 48 / Phase 49 / Phase 50 / Phase 51 / Phase 52 / Phase 53 / Phase 54 / Phase 55 / Phase 56 / Phase 57 / Phase 58 / Phase 59 / Phase 60 / Phase 61 / Phase 62 / Phase 63 / Phase 64 / Phase 65 / Phase 73

### Phase 73

- **Rollback of Phases 67-72** (backface/frustum/occlusion culling,
  LOD, live render distance, async pre-loading, and their own
  documentation/perf-report phase): on a real Mac, these performance
  changes caused FPS to drop below 30 while moving and visible sky
  artifacts at distance, on top of every pre-existing Mac bug still
  being unfixed - net negative, not net positive. `git reset --hard` to
  the commit right after Phase 66 (the last commit before Phase 67),
  removing all 6 Phase 67-72 commits from this branch's history
  entirely - `git log` now ends at Phase 66 again, and none of
  `engine/rendering/frustum.*`, `engine/rendering/occlusion_culler.*`,
  or `engine/rendering/lod_mesher.*` exist in the tree. `client/main.cpp`
  has no culling/pre-loading/render-distance code; `Options` has no
  `render_distance`/`lod_distance`/`keep_chunks_loaded` fields;
  `World` has no `adopt_generated_chunk`; `VoxelServer` has no
  `--pre-generate-radius`. The goal going forward: a clean, flat-out
  playable base game first, performance work later in small, real,
  individually-verified steps - not another all-at-once batch.
- **Real bug fixed while verifying the remaining Phase 66 baseline is
  actually intact** (this phase's own 73.4 step): a submerged column
  (terrain height at or below sea level) rendered its normal biome
  surface/subsurface (grass-over-dirt, snow-over-dirt) straight through
  real water above it - grass growing underwater, a real, visible seam
  at every below-sea-level column regardless of biome. This gap existed
  in every phase back to Phase 37 (its own DECISIONS.md entry already
  named it - "no sand shoreline transition" - as a known, deferred
  item) and was never actually fixed by any prior phase, including the
  now-reverted 67-72 - not a regression introduced by the rollback.
  Fixed in `generate_terrain_chunk`: a column whose own height is
  `<= kSeaLevel` now gets real sand (reusing `desert_surface`/
  `desert_subsurface`, the same real sand block every biome's own
  desert already uses) for both its surface and subsurface, regardless
  of biome. New `Worldgen.BelowSeaLevelColumnGetsRealSandInsteadOf
  ItsBiomesNormalSurfaceSubsurface` unit test; the two existing tests
  that assert a column's surface/subsurface block (`SurfaceLayerIs
  ExactlyOneBlockThickAtTheHeight`, `GenerateTerrainChunkMatchesTerrain
  HeightColumnByColumn`) were updated to account for the real
  underwater case too, since one of their own real test columns turned
  out to already be submerged.
- Verified the other three items on this phase's own checklist are
  genuinely intact, not just assumed: real grass top/side/bottom
  textures (`grass_def.top_texture`/`side_texture`/`bottom_texture`,
  the underside using the real dirt tile), real per-vertex UV tiling on
  greedy-merged quads (`add_quad`'s own `{0,0}..{width,height}` vertex
  UVs), and real single-column tree crowns (`vegetation_at`'s own
  trunk-then-leaf-cap placement) - all three present and unmodified.
  No other Mac bugs (mouse, click, water sorting, NPC collision) were
  touched - those are explicitly out of scope for this phase.
- `ctest` 647/647 (bgfx) / 639/639 (non-bgfx), both up by 1 from Phase
  65's own counts (the one new `Worldgen` test). Full clean rebuild in
  both configs plus a real headless `LCU_MAX_FRAMES=60` run, both
  confirmed clean.

### Phase 65

- **Real farming-processing recipes**: two new `ShapelessRecipe`s in the
  existing `RecipeRegistry` - 3x `game:wheat` -> 1x `game:bread` (the
  brief's own literal "3 Weizen -> 1 Brot"; `game:bread` has existed
  since Phase 51 with no real survival obtain path until now) and 2x
  `game:planks` -> 1x `game:wooden_hoe`. The hoe recipe is a real,
  documented simplification of Minecraft's own 2 sticks + 2 planks: no
  `game:stick` item exists anywhere in this project, and inventing one
  solely for this single recipe was rejected as disproportionate new
  scope (see DECISIONS.md) - 2 planks alone is an honest substitute
  using only already-existing items. No stone-hoe recipe either (the
  brief's own "optional", and there is no real tool-tier concept yet for
  a stone vs. wood hoe to meaningfully differ by).
- **Real, documented limitation discovered while wiring this up**: the
  Phase 23 "quick-craft" shortcut (`Action::Craft`) can never match
  either new recipe, no matter how much wheat/planks are held - it
  dedupes held items down to one of each *distinct* type before querying
  `RecipeRegistry`, so it structurally cannot represent "needs 3 of the
  same item". Both new recipes work correctly through the real,
  already-existing 2x2 inventory-screen grid and 3x3 workbench grid
  instead (Phase 49/50): those query actual per-cell contents, so
  placing each wheat/plank into its own cell (exactly how a real player
  would drag them) produces a correct multiset match, and the existing
  take-result handler's "consume 1 per non-empty ingredient cell" logic
  is already exactly correct for this case - it just had a stale doc
  comment (now corrected) claiming this only worked for 1-of-each
  recipes.
- New `LCU_VERIFY_FARMING_CRAFT` headless hook: grants 3 wheat + 2
  planks directly, drives the real 2x2 grid through real mouse-click
  simulation (left-click pickup, right-click "place 1 per cell" x3 then
  x2, take result, stow) for both recipes back to back in one run, then
  logs the final bread/wooden_hoe counts - confirmed via a real headless
  run: `bread=1 wooden_hoe=1` on both dev-bgfx and dev-nobgfx builds
  (needs `LCU_MAX_FRAMES>=1000000` in this sandbox to reliably cover its
  ~2.8 real elapsed seconds - see DECISIONS.md's Phase 64 entry on this
  convention's own real timing variance run to run).
  Full regression sweep (all prior `LCU_VERIFY_*` hooks, including
  `LCU_VERIFY_FARMING`/`LCU_VERIFY_INVENTORY`/`LCU_VERIFY_WORKBENCH`/
  `LCU_VERIFY_CRAFT`) stays clean. `ctest` 646/646 (bgfx) / 638/638
  (non-bgfx) - no new tests needed since `RecipeRegistry` and the real
  click-handling code were already fully unit/integration tested; this
  phase only adds data (recipe registrations) exercised by the new
  headless hook.

### Phase 64

- **Real farming**: `game:farmland` (tilled from grass/dirt, right-
  click with a hoe on the top face) and `game:wheat` (8 real growth
  states, planted via seeds on farmland's top face). New procedural
  `generate_wheat_stage` textures (8 real tiles, sparse pale-green at
  stage 0 to dense golden-brown at stage 7, same real "one noise field,
  rising threshold" technique as the Phase 60 crack overlay) mapped
  onto wheat's own real state via `BlockDefinition::texture_index_
  offset_by_state` (Phase 63).
- **Real, documented simplification**: wheat is registered with
  `has_collision=true` - this project's own raycast targeting is
  gated entirely on that flag (see `game:water`'s own doc comment), so
  a non-collidable wheat block would be real-honestly untargetable and
  unharvestable; the same trade-off `game:torch` already accepts.
  Rendering stays a full alpha-cutout CUBE (routed into the real
  alpha-blended `mesh.water` layer via `is_transparent=true`), not real
  cross/X-shaped crop geometry - the brief's own "cross_block" category
  is **PARTIAL**, deferred: `mesh_chunk_greedy` has no non-cube
  rendering path at all today, and building one is real, separate
  architectural work this phase's own scope doesn't require.
- **Real growth**: new `game::systems::update_crop_growth` - a real
  per-random-tick scan (once per real elapsed second) of every loaded
  wheat block below max growth, gated on real sky-OR-block light >= 9,
  independently rolling each eligible block against a real chance
  calibrated so the EXPECTED growth rate is +1 stage per real in-game
  day (`kCropRandomTickIntervalSeconds / day_length_seconds`). New
  `LCU_FAST_FARMING=1` dev toggle scales the real elapsed time fed into
  the growth accumulator (not the probability math itself) so a
  headless run can observe full 0-7 growth within seconds.
  Single-player only for now (client-authoritative, matching local
  `AIWander`'s own real scope split - see DECISIONS.md).
- **Real harvest**: breaking OR right-clicking mature wheat (state 7)
  drops a real, uniformly random 1-3 wheat + 1-3 seeds (via new
  `game::systems::harvest_wheat`); an immature crop drops only 1 real
  seed. Right-click harvest replants a fresh state-0 wheat immediately.
  Farmland underneath is never touched by either path, so it honestly
  stays farmland. Optional real 5%-seed-from-grass bonus drop on top of
  grass's own normal dirt-item drop.
- **New items**: `game:wheat_seeds`, `game:wheat`, and a minimal
  `game:wooden_hoe` (no durability/tool-tier concept - `ItemDefinition`
  has none; tilling behavior lives in a real side table, same pattern
  `edible_hunger_restore` already established).
- New unit tests: `CropGrowthSystem.*` (growth gating, light thresholds,
  max-state clamp, safe no-op on misconfiguration), `HarvestWheat.*`
  (drop-count ranges and real randomness), `GenerateWheatStage.*`
  (coverage growth/superset/maturity-color checks). New `LCU_VERIFY_
  FARMING` headless hook drives a real till -> plant -> (fast-forwarded)
  grow -> harvest round trip end-to-end - confirmed via a real, longer
  headless run (not part of the standard 60-frame regression sweep,
  which only checks for a clean crash-free shutdown - see DECISIONS.md
  for a real, newly-discovered timing caveat about that convention).
  Full regression sweep and a real two-process networked run (farming
  correctly inert there) both clean. `ctest` 646/646 (bgfx, up from
  629) / 638/638 (non-bgfx, up from 621).

### Phase 63

- **Real block-state system (farming foundation)**: `lcu::voxel::
  ChunkStorage` gains a parallel `std::array<u8, kVolume>` state array
  (~4KB/16³ chunk, a real, accepted memory cost). New `state_at`/
  `set_block_with_state`/`set_state`/`states`/`set_states` API sits
  alongside the unchanged `block_at`/`set_block` pair - `set_block`
  itself now also resets state to 0 (a fresh placement has no state
  history), so every pre-Phase-63 caller keeps behaving identically.
- **Real chunk-format v2**: `serialize_chunk_to_bytes`/`deserialize_
  chunk_from_bytes` now compress the state array alongside the block-id
  array in one payload. A real v1 file (blocks only) still loads
  cleanly - every v1 chunk's state reads back as a real, honest 0 (via
  the same `set_block`-always-resets-state behavior above), no separate
  migration code needed.
- **Real network wire-format extension, with zero protocol changes**:
  `game::systems::protocol::ChunkData::compressed_bytes` is - and
  always was - exactly `serialize_chunk_to_bytes`'s own output (see
  `server/main.cpp`'s two real call sites), so upgrading that function
  already extends the real wire format to carry states; `ChunkData`/
  `encode_chunk_data`/`decode_chunk_data` needed no changes at all.
  Verified via a real two-process server/client run.
- **Real meshing awareness**: `mesh_chunk_greedy`'s `MaskCell` gains a
  real `state` field, and `merges_with` now also requires equal state -
  two otherwise-identical neighboring blocks in different real states
  no longer merge into one quad. New opt-in `BlockDefinition::
  texture_index_offset_by_state` (default false, no existing block
  affected) adds the voxel's own real state to its resolved per-face
  texture index - the real mechanism Phase 64's 8-stage wheat growth
  will use, with no BlockDefinition-per-stage needed.
- New unit tests for state persistence (in-memory + file round trip,
  legacy-v1 compatibility, corrupt-size rejection), a real network
  round trip (`ChunkDataCarriesRealBlockStatesEndToEnd`), and meshing
  with states (merge-blocking, texture-offset opt-in and its no-op
  default). Full regression sweep (`HEALTH`/`MENU`/`INVENTORY`/
  `WORKBENCH`/`CRAFT`/`TORCH`/`HUD`/`BREAK_PLACE`/`SKIN`) plus a real
  two-process networked run all complete cleanly. `ctest` 629/629
  (bgfx, up from 614) / 621/621 (non-bgfx, up from 606).
- Honestly scoped: no real block actually sets `texture_index_offset_
  by_state` yet (Phase 64's wheat is the first real consumer) - this
  phase is the foundation only, per its own name.

### Phase 62

- **Real skin system (full version)**: 5 procedural skins
  (`lcu::assets::SkinPreset::{Steve,Alex,Red,Cyan,Ninja}`) - each a
  flat 4-material palette (hair/skin-tone/shirt/pants) painted across
  the exact same real MC region layout Phase 58's `Steve` default
  already used; `Steve` matches the old `generate_default_skin_pixels()`
  byte-for-byte.
- **Real `lcu::assets::SkinCatalog`**: the 5 builtins plus every real
  `*.png` file sitting in `assets/skins` (same CWD-relative,
  real-directory-scan convention as `mods`), discovered via a real
  `std::filesystem` scan. `add_from_file()` validates a real file via
  stb_image (must decode, must be 64x64 or the legacy 64x32 format),
  copies it into `assets/skins/<stem>.png`, and adds/updates its real
  catalog entry - never crashes on an invalid file (unreadable, wrong
  size, or a name colliding with a builtin), always returns a real
  `{ok, error}` result instead. A legacy 64x32 upload's missing real
  left-arm/left-leg regions are synthesized by copying the same file's
  own already-decoded right-arm/right-leg pixels (unmirrored - a real,
  documented simplification, see DECISIONS.md).
- **Real new dependency**: stb_image (decode) + stb_image_write
  (test/verify-fixture encode only), fetched via `FetchContent`
  (no version tags exist upstream, pinned to a real commit), each
  compiled in its own dedicated, unstrict-warnings target
  (`StbImageImpl`/`StbImageWriteImpl`) to keep this project's own
  zero-warning `-Werror` build clean of third-party warning noise.
- **Real "Skins" menu screen**: reachable from the pause menu, one row
  per real catalog entry (marked `AUSGEWAEHLT` when selected) plus a
  real "Eigenen Skin laden..." row and "Zurueck" - built with the same
  `MenuStack`/`MenuScreen` framework Phase 46's Options/Controls
  screens already use.
- **Real "Load own skin..." file picker**: new `lcu::platform::
  request_open_png_file_dialog`/`poll_open_png_file_dialog_result`
  wrap the real, async, platform-native `SDL_ShowOpenFileDialog` (its
  own callback may run on a different thread - handled via a real
  thread-safe, module-local mailbox in `window.cpp`), polled once per
  frame; a successful pick runs through the same real
  `SkinCatalog::add_from_file` + live-reload path as everything else.
- **Real persistence**: `Options` gains `skin_name` (persisted as
  `skin=<name>` in options.txt, default `"Steve"`), resolved against
  the real `SkinCatalog` at startup with a logged fallback to Steve if
  the saved name isn't found (a deleted custom skin file, a stale/
  corrupt value).
- **Real live-reload**: `apply_skin(index)` destroys the old GPU
  texture and uploads the new skin's real pixels immediately - the
  player's own third-person model and first-person arm both pick it up
  next frame with zero extra plumbing, since they already read the one
  `skin_texture` handle fresh every frame.
- **Real per-NPC fixed skins**: new `game::components::NpcAppearance`
  (`skin_preset_index`) assigned once at spawn, cycling through the 5
  builtin presets - the 3 real `AIWander` NPCs no longer share the
  player's own selectable skin, and never change even if the player
  later changes or uploads their own (a new, independent
  `npc_skin_textures` array of 5 real GPU textures, created once at
  startup).
- New `LCU_VERIFY_SKIN` headless hook: exercises `apply_skin`, a real
  `SkinCatalog::add_from_file` round trip (via a real temp PNG written
  through stb_image_write, standing in for a real user-picked file -
  `SDL_ShowOpenFileDialog` itself has no real backend in this headless
  sandbox and can't be scripted), and real `build_skins_screen()`
  construction. Full regression sweep (`HEALTH`/`MENU`/`INVENTORY`/
  `WORKBENCH`/`CRAFT`/`TORCH`/`HUD`/`BREAK_PLACE`/`SKIN`) all complete
  cleanly. `ctest` 614/614 (bgfx, up from 591) / 606/606 (non-bgfx, up
  from 586).
- Honestly scoped: the real native OS file-open dialog itself is
  **NOT VERIFIED — ENVIRONMENT LIMITATION** (no desktop/portal service
  in this headless sandbox); everything downstream of "a real file path
  was chosen" is exercised for real via `LCU_VERIFY_SKIN`. A legacy
  64x32 upload's synthesized left-limb pixels are unmirrored (see
  DECISIONS.md).

### Phase 61

- **Real transparent water rendering**: the `ChunkMesh::water` layer
  (structurally present since early phases, never populated) is now
  real. `mesh_chunk_greedy`'s face-visibility test gains a second real
  branch - beyond the existing opaque-vs-transparent XOR - for "two
  different non-opaque substances touching" (today: water next to
  air), so a transparent block is no longer silently invisible where
  it borders another non-opaque material. Quads route to `mesh.water`
  vs `mesh.opaque` by reusing the existing `BlockDefinition::
  is_transparent` flag (no new field needed) - water is now the one
  real block with that flag flipped true.
- **Real alpha-blended chunk draw**: `Renderer::submit_chunk_mesh`
  gains an `alpha_blend` parameter (default false, every existing
  opaque call site unaffected) - when true, real
  `BGFX_STATE_BLEND_ALPHA` replaces the opaque state, keeping depth
  test but dropping depth write. `fs_chunk.sc` now samples and outputs
  the atlas's real per-texel alpha (previously hardcoded `1.0`) -
  harmless for every opaque draw (alpha is only ever consumed when
  blend state is enabled) and lets water's real semi-transparent
  texture (alpha ~180/255, Phase 54) genuinely composite see-through.
- **Real client wiring**: a second `gpu_water_meshes` map mirrors
  `gpu_meshes` through every lifecycle point (remesh/upload, far-chunk
  unload, shutdown); the render loop draws the opaque layer fully
  first, then a second pass over `gpu_water_meshes` with
  `alpha_blend=true`, reusing view 0 (no new bgfx view) - opaque-then-
  transparent with no back-to-front sort between water chunks, a real,
  documented limitation.
- Real, accepted side effect: `is_transparent` also drives light
  propagation (`engine/lighting/propagation.h`), so light now passes
  through water too - not separately fixed, see DECISIONS.md.
- Leaves deliberately stay opaque (`is_transparent = false`) - out of
  scope for this phase, which is titled and scoped to water only.
- 3 new `GreedyMesher` unit tests for the new visibility branch and
  layer routing; 1 existing test's stale assertion/comment fixed (a
  2-block transparent pair's 5 real air-facing faces are NOT empty,
  only the shared internal boundary between them stays face-less).
- Verified via real headless runs (default + `LCU_VERIFY_BREAK_PLACE`,
  60 frames, clean shutdown) and the full regression sweep (`HEALTH`/
  `MENU`/`INVENTORY`/`WORKBENCH`/`CRAFT`/`TORCH`/`HUD`, all complete
  cleanly), plus a real `LCU_BUILD_SHADER_TOOLS=ON` build confirming
  `fs_chunk.sc` recompiles cleanly to all 3 profiles (spirv/glsl/
  essl). `ctest` 591/591 (bgfx, up from 587) / 586/586 (non-bgfx, up
  from 583).
- Honestly scoped: no back-to-front sorting between separate water
  chunks (a real, low-risk limitation - single water bodies render
  correctly either way); what real transparency looks like on a real
  GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**.

### Phase 60

- **Real crack textures**: 10 new procedural atlas tiles
  (`TileId::Crack0`..`Crack9`, in the SAME block/item atlas - a real,
  spare-capacity "own atlas area", not a whole new texture/sampler).
  `generate_crack(stage)` samples one deterministic per-pixel noise
  field with a threshold that grows with `stage`, so every higher
  stage's real cracked pixels are a real superset of the stage below -
  the same real growing-damage look Minecraft's own break overlay has,
  without 10 independently hand-authored crack patterns.
- **Real alpha-blended rendering**: `Renderer::submit_textured_box`
  gains an `alpha_blend` parameter (defaulted false, every Phase 58/59
  caller unaffected) - when true, real `BGFX_STATE_BLEND_ALPHA`
  replaces the opaque write, so the crack texture's real transparent
  "uncracked" pixels actually composite see-through instead of
  rendering solid black (closing, for this one real caller, the same
  alpha-blending gap documented since Phase 56).
- **Real rendering**: the Phase 48.2 flat-darkening break-progress box
  is replaced by a real alpha-blended, crack-textured box (all 6 faces
  share the same crack UV - a real, simpler reading than raycasting the
  exact hit face for one oriented quad, see DECISIONS.md) - `render_
  break_fraction` (0..1) maps onto the real 10 crack stages.
- 4 new unit tests (`GenerateCrack.*`, incl. a real superset-growth
  check across all 10 stages) plus the existing `ProceduralTextures.*`/
  `BuildBlockAtlasPixels.*` suites now also iterate the 10 new tiles.
- Verified via real headless runs, real `LCU_VERIFY_BREAK_PLACE`
  (exercises the real crack overlay across multiple frames of held
  break progress) and the full regression sweep (`HEALTH`/`MENU`/
  `INVENTORY`/`WORKBENCH`/`CRAFT`/`TORCH`/`HUD`, all complete cleanly),
  and a real `LCU_BUILD_SHADER_TOOLS=ON` build (no shader files
  touched - the new `alpha_blend` flag is a pure bgfx render-state
  change, `vs_sky.sc`/`fs_sky.sc` are unchanged). `ctest` 591/591
  (bgfx, up from 587) / 583/583 (non-bgfx, up from 579).
- Honestly scoped: what the real crack texture looks like on a real
  GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**; the
  crack overlay covers all 6 faces of the targeted block uniformly
  rather than only the specific face being broken (a real, documented
  simplification, see DECISIONS.md).

### Phase 59

- **Real visible NPCs**: `submit_character_model` (Phase 58's own
  third-person body rendering) extracted into a real, reusable
  function - the local player's own third-person body and every
  wandering AI entity now go through the exact same function, just
  fed different position/yaw/pitch/walk-phase. The 3 AI entities
  spawned since Phase 6 now render as real Steve-like figures, not
  invisible logic-only points.
- **Real NPC facing + animation**: each NPC's own yaw is derived from
  its real `AIWander::target` direction (faces where it's walking, not
  a fixed default); a real walk-cycle limb swing plays while
  `wait_seconds <= 0` (moving), a real small head-wobble idle animation
  plays while `wait_seconds > 0` (waiting) - both driven by a new
  `npc_animation_time` real elapsed-time clock (advances only while
  unpaused), since NPC movement (unlike the player's own) has no
  per-frame distance delta exposed back to the renderer to drive an
  exact walk-cycle from.
- **Debug wireframe boxes are now a real toggle, default off**: bundled
  into the existing `options.debug_overlay_enabled` (F3) flag rather
  than a new dedicated keybind - it was already a real "show debug
  visualization" preference with exactly the right default. Real
  remote-player avatars (networked mode) stay out of this phase's own
  scope (the brief names AI wander entities specifically); remote
  entities keep their previous debug-box-only representation, now
  gated behind the same toggle instead of always-on.
- Verified via real headless runs (default run logs "Spawned 3
  wandering AI entities" as before, now rendered as real models every
  frame), real `LCU_VERIFY_HUD`/`BREAK_PLACE`/`HEALTH`/`MENU`/
  `INVENTORY`/`WORKBENCH`/`CRAFT`/`TORCH` regression runs (all still
  complete their full frame counts cleanly), and a real
  `LCU_BUILD_SHADER_TOOLS=ON` build (no shader files touched -
  `submit_character_model` reuses Phase 58's own `submit_textured_box`
  unchanged). `ctest` 587/587 (bgfx) / 579/579 (non-bgfx) - unchanged
  counts, real-rendering wiring on top of Phase 58's already-tested
  math/primitives, not new pure-logic surface.
- Honestly scoped: what real NPCs look like walking around on a real
  GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**; NPC
  walk-cycle speed is a fixed real constant (`kNpcWalkCycleFrequency`),
  not derived from each NPC's own real `AIWander::speed` (a real,
  documented simplification, see DECISIONS.md); no player-vs-NPC
  collision (unchanged, pre-existing "collision with player still
  inactive" behavior per the brief's own note).

### Phase 58

- **Real character model**: `engine/assets::skin_texture.{h,cpp}` - a
  procedurally-generated 64x64 player skin in the real Minecraft
  "modern" (dual-arm/dual-leg) UV layout, 36 named per-face regions
  (`SkinRegion`), own texture/sampler slot. `Renderer` gains
  `submit_textured_box` (draws an arbitrary, not-necessarily-axis-
  aligned box from 8 caller-supplied world-space corners, independent
  UV rect per face) plus real `rotate_yaw`/`rotate_pitch`/
  `character_part_corners` math in `client/main.cpp` (yaw/pitch derived
  to reproduce `FirstPersonCamera::forward()` bit-for-bit).
- **First-person arm**: replaces the flat 2D hand icon (Phase 48) with
  a small 3D box held in view-space, textured with the current hotbar
  item's own atlas UV (not the skin) - the same swing animation as
  before, now a real position/orientation offset instead of a 2D quad
  offset.
- **Third-person body**: a real 6-box Steve-like model (head, torso, 2
  arms, 2 legs) - legs/arms swing in a real, frame-rate-independent
  walk cycle (`walk_cycle_phase` advances by distance travelled, not
  wall-clock time); head follows real camera pitch, body yaw follows
  camera yaw (a documented simplification of MC's own head/body-yaw-lag
  system - see DECISIONS.md). Every part's own real dimensions are
  Minecraft's own per-part pixel sizes, uniformly scaled so the whole
  stack fits exactly inside `kPlayerHeight` (1.8 blocks).
- **Real 3-way F5 perspective cycle**: First-Person -> Third-Person-
  Behind -> Third-Person-Front -> First-Person, closing the previously
  documented "no third-person-front, no player model to look at" PARTIAL.
- **AABB verified, not changed**: `kPlayerHalfWidth`/`kPlayerHeight`/
  `kEyeHeight` already held the exact real Minecraft values (0.3/1.8/
  1.62) from earlier phases - Phase 58.1 needed no numeric change, only
  confirmation and the real work built on top of them above.
- Verified via real headless runs (`Player skin: skin_texture_valid=
  true`), a real extended `LCU_VERIFY_HUD` run (now presses F5 three
  times, cycling through all three perspectives and exercising every
  one of `submit_textured_box`'s 7 real per-frame call sites - the arm
  box plus all 6 body-part boxes), real `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_HEALTH`/`LCU_VERIFY_MENU`/`LCU_VERIFY_INVENTORY`/
  `LCU_VERIFY_WORKBENCH`/`LCU_VERIFY_CRAFT`/`LCU_VERIFY_TORCH`
  regression runs (all still complete their full frame counts cleanly),
  and a real `LCU_BUILD_SHADER_TOOLS=ON` build (no shader files were
  touched this phase - `submit_textured_box` reuses `vs_sky.sc`/
  `fs_sky.sc` unchanged - so this just confirms the existing pipeline
  still compiles/links against the new caller). 12 new unit tests
  (`SkinTextureConstants`, `SkinUvRange.*` incl. 5 exact-coordinate
  checks against the brief's own example regions, `GenerateDefaultSkin
  Pixels.*`). `ctest` 587/587 (bgfx, up from 575) / 579/579 (non-bgfx,
  up from 567).
- Honestly scoped: what the real character model/skin actually looks
  like on a real GPU/display is still **NOT VERIFIED — ENVIRONMENT
  LIMITATION**; body yaw follows camera yaw directly rather than a real
  independent, lagging body-facing direction (see DECISIONS.md); no
  idle/breathing animation for the player (that's Phase 59's own NPC
  job per the brief's own phasing); no third-person camera collision
  (the existing `kThirdPersonDistance` gap noted since Phase 47 is
  unchanged).

### Phase 57

- **Real bitmap-font atlas**: new `engine/assets::font_atlas.{h,cpp}` -
  a real, own-design procedurally-generated monospace font covering
  ASCII 32-126 (95 characters), one glyph per 6x8-pixel cell (a 5x7
  glyph plus 1px right/bottom spacing), packed into its own separate
  96x48 RGBA8 atlas (deliberately NOT merged into the Phase 53 block
  atlas - see DECISIONS.md). Each glyph is real hand-authored 5x7 dot-
  matrix art (95 characters, not a transcription of any existing font
  file), rasterized to opaque-white/transparent pixels by a real,
  deterministic `generate_glyph_pixels(char)` so one texture can be
  tinted to any text color at draw time. Same half-texel UV-inset
  anti-bleed technique as `texture_atlas.h`. 12 new unit tests
  (`FontAtlasConstants`, `GlyphUvRange.*` incl. an out-of-range->'?'
  fallback check, `GenerateGlyphPixels.*` incl. a determinism check,
  `BuildFontAtlasPixels.*` incl. a full atlas-vs-generator byte match).
- **Real text renderer**: new `engine::ui::TextRenderer` - stateless,
  `draw_text(renderer, text, x, y, color, scale)` queues one real
  textured quad per character via the new `Renderer::
  submit_text_glyph_quad`, and `measure_text_width` for right-
  alignment. `UiVertex2D`'s per-vertex sample flag becomes a real
  tri-state (0 flat color / 1 item-atlas RGB-as-is / 2 font-atlas RGB
  tinted by vertex color); `fs_ui2d.sc` gains a second real sampler
  (`s_font`, its own texture slot) and a chained-`mix()` selector
  picking the right one of the three per pixel, all within the SAME
  single UI draw call/batch a frame already had - no extra draw calls
  for text.
- **HUD/menu/inventory/workbench now render real text**: `draw_debug_
  overlay`/`draw_hud_labels`/`draw_menu_labels`/`draw_inventory_screen_
  labels`/`draw_crafting_table_screen_labels` all gain a real
  `legacy_debug_text` parameter - `false` (the new default) draws
  through `TextRenderer`'s real bitmap-font atlas at real pixel
  positions (no more character-cell rounding for e.g. hotbar item
  counts); `true` keeps every one of those functions' exact previous
  `bgfx::dbgTextPrintf`-based behavior, unchanged, as a real working
  fallback - `LCU_LEGACY_DEBUG_TEXT=1` (`client/main.cpp`) switches all
  five over live.
- Verified via real headless runs (`Font atlas: font_atlas_texture_
  valid=true`, clean 30-frame runs under both the new and legacy text
  paths), real `LCU_VERIFY_MENU`/`LCU_VERIFY_INVENTORY`/`LCU_VERIFY_
  WORKBENCH` runs (the three real UI screens whose labels now go
  through `TextRenderer`, all complete their full 60 frames cleanly),
  real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_HEALTH` regression runs
  (still byte-identical gameplay-logic output), and a real
  `LCU_BUILD_SHADER_TOOLS=ON` build (`fs_ui2d.sc`/`vs_ui2d.sc` compile
  cleanly to spirv/glsl/essl with the new sampler/mix logic). `ctest`
  575/575 (bgfx, up from 563) / 567/567 (non-bgfx, up from 555).
- Honestly scoped: what the real bitmap font actually looks like
  rendered on a real GPU/display is still **NOT VERIFIED — ENVIRONMENT
  LIMITATION**; the hand-authored 5x7 glyph shapes are plain geometric
  block letters, not aiming for real typographic refinement; text has
  no kerning (fixed-width monospace advance only, by design); this
  closes out the Phase 53-57 program - see PROJECT_STATE.md.

### Phase 56

- **Items now use the real texture atlas**: `ItemDefinition` gains
  `texture_index` (`std::optional<u32>`, same layering reason as
  `BlockDefinition`'s own texture fields) - block-as-items (stone,
  grass, dirt, torch, wood, crafting table, compost, planks) reuse the
  exact same atlas slot as their block. Apple/bread deliberately keep no
  `texture_index` (no Phase 54 texture exists for either) and fall back
  to `icon_color` everywhere, same as before.
- **One real helper, one real call site pattern**: `client/main.cpp`
  gained a single `resolve_item_display` lambda that every one of the
  13 places that used to read `.icon_color` off the item registry now
  routes through - it fills `icon_color` (always, unchanged) and
  `texture_uv` (only when textures are on *and* the item has a real
  `texture_index`), so inventory slots, hotbar, the crafting grid, and
  the drag cursor all pick up real textures from one place, not 13
  separate lookups.
- **Real textured-quad rendering plumbed through the whole UI stack**:
  `UiVertex2D` gains a per-vertex `use_texture` flag (not a per-draw
  uniform - a single UI batch legitimately mixes textured item icons
  with flat-color borders/backgrounds/health bars in the same draw
  call); `Renderer::submit_textured_ui_quad` is the new real entry
  point; `vs_ui2d.sc`/`fs_ui2d.sc` sample `s_atlas` and `mix()` against
  the flat color per-vertex. `HotbarItem`/`InventorySlotDisplay` both
  gain `std::optional<math::Vec4> texture_uv`; `hud_renderer.cpp`,
  `inventory_screen_renderer.cpp`, and `crafting_table_screen_renderer.cpp`
  all branch on it the same way.
- **Hand icon and dropped items textured too**: the Phase 48 hand icon
  now goes through `resolve_item_display` + `submit_textured_ui_quad`.
  `submit_world_billboard` (Phase 50 dropped items) gains real
  atlas-texture + UV-rect parameters, reusing the same `vs_sky.sc`/
  `fs_sky.sc` program already shared by the skybox/wireframe/solid-box
  billboard family - the other 3 callers of that program
  (`submit_billboard`, `submit_wireframe_box`, `submit_solid_box`) get
  their vertex structs mechanically extended with always-zero UV/flag
  fields, keeping their own output byte-identical.
- Verified via real headless runs (`atlas_texture_valid=true`/`false`
  under both `LCU_USE_TEXTURES=1`/`0`, unchanged from Phase 55), real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_HEALTH` regression runs (still
  byte-identical gameplay-logic output), and a real
  `LCU_BUILD_SHADER_TOOLS=ON` build compiling every touched shader
  (`vs_ui2d.sc`/`fs_ui2d.sc`/`vs_sky.sc`/`fs_sky.sc`) to spirv/glsl/essl.
  `ctest` 563/563 (bgfx) / 555/555 (non-bgfx) - unchanged counts, since
  Phase 56 is real-rendering wiring with fallback-chain coverage already
  proven by Phase 55's tests, not new pure-logic surface.
- Honestly scoped: what any of this actually looks like textured on a
  real GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**;
  the `vs_sky.sc`/`fs_sky.sc` family never had alpha blending enabled
  and still doesn't, so a dropped torch's transparent background pixels
  render solid black on its billboard rather than see-through - a real,
  accepted visual limitation, not a bug, documented in DECISIONS.md.

### Phase 55

- **Blocks now use the real texture atlas**: `BlockDefinition` gains
  `top_texture`/`side_texture`/`bottom_texture` (`u32`, mirroring
  `color`/`side_color`/`bottom_color`'s own fallback chain exactly) -
  `mesh_chunk_greedy` resolves the real per-face atlas tile at the same
  place/time it already resolves `quad_color`, and passes it through to
  `ChunkMeshLayer::add_quad`'s new `texture_index` parameter. Every
  real block registered in `client/main.cpp` (stone, grass, dirt, sand,
  snow, torch, water, coal/iron ore, wood, leaves, cactus, crafting
  table) now points at its own real Phase-54 texture(s) - grass's real
  underside is the dedicated dirt tile (not a reuse of the green-capped
  side texture the color-only fallback used), wood's top and bottom
  both show real growth rings (only the sides show bark), matching real
  Minecraft's own per-face convention more closely than color alone
  could.
- **Real texture-atlas tests**: `BlockDefinition` is `EngineCore`-level
  (built for `VoxelServer` too), so `top_texture`/etc. stay a plain
  `u32`, not `lcu::assets::TileId` directly - engine/voxel can't depend
  on `engine/assets` (`LCU_BUILD_CLIENT`-only). Two new
  `GreedyMesher.*` tests prove the same real top/side/bottom fallback
  chain `PerFaceColorUsesTopSideBottomFallbackChain` already proves for
  color, now for `texture_index`.
- Verified via a real headless run (`atlas_texture_valid=true` with
  real per-block textures now resolved through meshing), a real
  `LCU_VERIFY_BREAK_PLACE` regression run (still passes byte-identical
  - breaking/placing carries real texture indices through the whole
  pipeline with no behavior change to the logged gameplay), and a real
  `LCU_BUILD_SHADER_TOOLS=ON` build. `ctest` 563/563 (bgfx, up from
  561) / 555/555 (non-bgfx, up from 553).
- Honestly scoped: what any real block actually looks like textured on
  a real GPU/display is still **NOT VERIFIED — ENVIRONMENT LIMITATION**;
  `game:crafting_table`'s sides reuse the plain `Planks` tile (no
  dedicated crafted-table-side texture exists - Phase 54's own texture
  list names only a table-top pattern); `game:torch`/`game:cactus`
  render one real texture on every face (no per-face variation exists
  for either in Phase 54's own texture list either).

### Phase 54

- **Real procedurally-generated MC-style textures**: new
  `engine/assets::procedural_textures.{h,cpp}` - 17 real 16x16 RGBA
  generators (`generate_grass_top`/`_side`, `generate_dirt`,
  `generate_stone`, `generate_sand`, `generate_snow`, `generate_water`,
  `generate_wood_side`/`_top`, `generate_leaves`, `generate_coal_ore`/
  `iron_ore`, `generate_torch`, `generate_crafting_table_top`,
  `generate_cactus`, `generate_compost`, `generate_planks`), each real
  and deterministic (a pure hash function of a fixed per-texture seed +
  pixel position - same call, same bytes, every time, no RNG-engine
  state to carry around). New `TileId` enum fixes each texture's own
  real atlas slot (0-16) - grass's real jagged green/dirt boundary
  (a deterministic per-column row jitter, not a flat line), wood's real
  concentric growth rings, ore's real 2x2 pixel-blob clusters over a
  stone base, leaves' real alpha-0 holes, and torch's real transparent
  background + stem + flame are all genuinely computed, not flat fills
  with a label.
- **New `build_block_atlas_pixels()`** packs all 17 real textures into
  one real 256x256 RGBA8 buffer at their own fixed `TileId` slots -
  `client/main.cpp` now uploads this (via Phase 53's own `Renderer::
  create_texture_from_pixels`) instead of Phase 53's flat-white
  placeholder whenever `LCU_USE_TEXTURES` is on.
- 8 new unit tests (`ProceduralTextures.*`/`BuildBlockAtlasPixels.*`) -
  including a real proof the atlas-packing math lands each tile at its
  own correct slot (comparing a packed atlas pixel against that same
  tile's own standalone generator output), not just "the buffer is the
  right size".
- Verified via a real headless run (`Texture atlas: use_textures=true
  atlas_texture_valid=true` - the real generated atlas uploads
  successfully under the real, headless Noop backend) and a real
  `LCU_BUILD_SHADER_TOOLS=ON` build (unchanged shaders from Phase 53,
  still compile cleanly). `ctest` 561/561 (bgfx, up from 553) / 553/553
  (non-bgfx, up from 545).
- Honestly scoped: no `BlockDefinition`/`ItemDefinition` yet reference
  any of these real textures - every face/icon still renders atlas tile
  0 (`TileId::GrassTop`) regardless of block type until Phase 55/56
  wire real per-face/per-item texture indices; what these textures
  actually look like on a real GPU/display is still **NOT VERIFIED —
  ENVIRONMENT LIMITATION**.

### Phase 53

- **Real texture-atlas pipeline** (infrastructure only - no actual block
  textures yet, see Phase 54): new `engine/assets::texture_atlas.{h,cpp}`
  (pure logic, no bgfx dependency) - a fixed 256x256 RGBA atlas packed
  as a 16x16 grid of 16x16-pixel tiles, `tile_uv_range(tile_index)`
  returning each tile's real sample rect. Anti-bleed is a real half-texel
  UV inset, not literal padding pixels between tiles (the atlas's own
  fixed 256x256/16x16-tiles-of-16x16-pixels size leaves no spare pixel
  budget for a literal border without shrinking real tile content or
  growing the atlas - see DECISIONS.md).
- **New `Renderer::create_texture_from_pixels`/`destroy_texture`**:
  real bgfx 2D RGBA8 texture upload, nearest-filtered + clamp-addressed
  (`BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP`) baked into the
  texture's own creation flags.
- **`voxel::MeshVertex` gains a real `u16 texture_index` field** (Phase
  53.4) - placed *before* the trailing `light` byte, not after, so its
  2-byte alignment needs zero compiler-inserted padding (see the field's
  own doc comment for the real bug this avoids: an internal gap the
  existing tightly-packed `chunk_mesh_vertex_layout()` doesn't account
  for). `ChunkMeshLayer::add_quad` takes a new, defaulted
  `texture_index` parameter - existing call sites (including
  `mesh_chunk_greedy`'s own two) are unaffected, still resolving to
  atlas tile 0 until Phase 55 gives `BlockDefinition` real per-face
  texture assignments.
- **Real chunk-shader atlas sampling**: `vs_chunk.sc`/`fs_chunk.sc`/
  `varying.def.sc` extended with the tile index + 3 new uniforms
  (`u_useTextures`, `u_tileStep`, `u_tileInset`) and an `s_atlas`
  sampler. `local_uv = fract(v_texcoord0)` wraps a greedy-meshed quad's
  per-block UV back into 0..1 so a merged multi-block face tiles the
  same texture repeatedly instead of stretching one tile across the
  whole run. `u_useTextures.x` mixes between the real atlas sample and
  the existing flat-color/noise path - driven directly by whether
  `Renderer::submit_chunk_mesh` was actually handed a valid atlas
  texture this draw (a real, always-in-sync source of truth, not a
  separately-tracked toggle that could drift).
- **New `LCU_USE_TEXTURES` env toggle** (default ON, `=0` forces the
  exact pre-Phase-53 procedural-only path). `client/main.cpp` creates a
  real placeholder 256x256 flat-white atlas texture when textures are
  on - proves the real GPU round-trip (`create_texture_from_pixels` ->
  a valid `bgfx::TextureHandle` under the real, headless Noop backend)
  end to end this phase, not just declared/unused API surface; Phase 54
  replaces this placeholder with real procedurally-generated content.
- 6 new unit tests (`TextureAtlasConstants`/`TileUvRange`).
- Verified via real `LCU_USE_TEXTURES=1`/`=0` headless runs (bgfx build
  - `Texture atlas: use_textures=true atlas_texture_valid=true` /
  `use_textures=false atlas_texture_valid=false`), real regression runs
  of `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_HEALTH` (both still pass
  byte-identical to Phase 52 - fall damage/eating/item pickup all still
  log correctly with the new vertex format in place), and a real
  `LCU_BUILD_SHADER_TOOLS=ON` build (`vs_chunk.sc`/`fs_chunk.sc` compile
  cleanly to all 3 real shader profiles - spirv/glsl/essl). `ctest`
  553/553 (bgfx, up from 547) / 545/545 (non-bgfx, up from 539).
- **Deliberately deferred, marked PARTIAL**: Phase 53.2's optional
  stb_image-based debug PNG dump of the atlas was not implemented -
  this sandbox has no display to actually view a dumped debug image
  against, the directive itself marks it optional, and pulling in a new
  third-party dependency for a feature nobody here can currently use to
  verify anything would be real, avoidable scope creep. See
  DECISIONS.md.

### Phase 52

- **Documentation-only phase** closing the Phases 43-52 program
  (rebindable input through health/hunger/respawn). New README.md
  `## Controls` section: a full table of every real default keybinding
  (from `KeyBindings::reset_to_defaults()`), each one noted as
  rebindable via the in-game Controls screen. New BUILDING.md
  `## Options file (options.txt)` section documenting its real
  `SDL_GetPrefPath`-derived path (logged on every run), the CWD-relative
  fallback if that call fails, and that deleting it is a safe reset.
  New DECISIONS.md entry recording *why* the standing directive's own
  exclusion list (mobs/redstone/enchantments/Nether/villagers/
  structures/farming/chat/skins) holds up - each is a genuinely separate
  content vertical, none of them block what Phases 43-51 actually built.
  CHANGELOG/PROJECT_STATE/TASK_QUEUE entries for Phases 43-51 were kept
  up to date incrementally as each phase landed (not deferred to this
  phase) - see each phase's own section above.
- No code changes; `ctest` unchanged at 547/547 (bgfx) / 539/539
  (non-bgfx).

### Phase 51

- **Real health/hunger/fall-damage/respawn** - new `game::components::
  PlayerHealth`/`PlayerHunger` (plain structs, not ECS components -
  matches `PlayerPhysicsState`'s own placement, see each header's own
  doc comment) and `game::systems::player_vitals_system.{h,cpp}` (pure
  logic, 24 new unit tests): `apply_damage` (clamped, returns true only
  on the real 0-transition so a caller can call it every frame without
  re-triggering death), `fall_damage_for_distance`/`update_fall_tracking`
  (real Minecraft-shaped `fallDistance` semantics - accumulates only
  while airborne and actually descending, damage applied/reset only on
  the real landing-frame transition, so a normal jump deals zero
  damage), `update_health_regen` (+1 HP/4s at hunger >= 18),
  `update_starvation` (-1 HP/4s at hunger == 0), `update_hunger_drain`
  (-1/30s normally, 2x while sprinting-and-moving), `apply_jump_hunger_
  cost`/`eat` - each real per-frame timer using the same caller-owned
  accumulator pattern `hand_swing_elapsed` already established.
- **Wired into `client/main.cpp`**: `previous_player_y` captured before
  gravity/collision resolve each frame, fed to `update_fall_tracking`
  after (works for both the single-player and networked physics path -
  both mutate the same `player.aabb`/`player.grounded`); jump hunger
  cost applied on a real edge-detected fresh Jump press, read *before*
  `try_jump` itself changes `player.grounded`; hunger drain/regen/
  starvation tick every frame gated on `paused` alone (like day/night -
  vitals keep ticking while the inventory/workbench screen is open,
  matching real Minecraft, only movement/mining/placing/eating lock for
  those). Real HUD wiring: `hud_state.health`/`hunger` now read straight
  from `player_health`/`player_hunger` instead of `HudState`'s own 20/20
  defaults - Phase 47's icon math needed no changes at all.
- **Real eating**: new `game:apple` (+4 hunger)/`game:bread` (+5 hunger)
  items, real Minecraft restore values. Right-click dispatch is now a
  real three-way branch: crafting-table intercept (Phase 50.3, checked
  first) -> eating (new - deliberately doesn't require a raycast hit,
  same as Minecraft letting you eat while looking at open air) ->
  normal slot-driven placement (Phase 49, unchanged). Neither apple nor
  bread has a survival obtain path yet (no farming/mob drops - both
  explicitly out of scope, see PROJECT_STATE.md Known Limitations) -
  `LCU_VERIFY_HEALTH` grants one directly, same synthetic-setup honesty
  every other verify hook's own item grant already uses.
- **Real death + respawn**: a death transition (from fall damage or
  starvation) calls `handle_player_death` - drops the *entire* 36-slot
  inventory as real item entities at the player's position (reusing the
  exact same `ItemEntity`/`update_item_entities`/`pickup_item_entities`
  pipeline Phase 50 built, just centered on the player instead of a
  broken block), closes any open inventory/workbench screen, and pushes
  a new "Du bist gestorben" `MenuScreen` (reusing Phase 46's `MenuStack`
  framework wholesale - no new UI code needed) with a single Respawn
  row. Respawn resets position to the original spawn point, health/
  hunger to full, and every fall/regen/starvation/drain accumulator.
- **New `LCU_VERIFY_HEALTH` headless hook** (the project's seventh):
  directly teleports the player 10 blocks above their own real spawn
  ground position with `grounded=false` (a real jump can't reach that
  height deterministically, but the fall from there on is real,
  unmodified gravity/collision - the same "synthesize exactly the state
  a real action would produce" honesty `LCU_VERIFY_WORKBENCH`'s own
  direct world-block seed already uses), seeds hunger below max (so
  eating has an observable effect) and grants 1 `game:apple`, then
  simulates a real right-click once the fall has had time to land. A
  real run's log output proves both halves of this phase's own "jump
  from a tower, health drops; eat, hunger rises" directive: `Fall
  damage: 6.9 (health: 13.1/20.0)` then `Ate game:apple (hunger: 14.0/
  20.0)`. Death + respawn were separately confirmed via a real run with
  a temporarily-lethal fall height (`Player died (health reached 0)` /
  `Player died - inventory dropped as item entities`) - the respawn
  button itself reuses the exact same `MenuStack`/`pending_menu_action`
  machinery `LCU_VERIFY_MENU` already proves works, so it wasn't
  re-verified via a second simulated click sequence.
- **A real, confirmed single-player-only gap**: in networked mode,
  `LCU_VERIFY_HEALTH`'s synthetic mid-air teleport is invisible to
  `VoxelServer`'s own authoritative simulation (the server only ever
  learns the player's position from real `PlayerInput` packets), so the
  very next `PlayerCorrection` snaps the client back down before a real
  fall distance can accumulate - confirmed via a real two-process
  networked run (`Ate game:apple (hunger: 14.0/20.0)` appears; no `Fall
  damage:` line does). Eating verifies correctly in networked mode
  regardless (item grants/consumption are real client-authoritative
  state, same as every other verify hook's own item grant) - documented
  in the hook's own doc comment, not silently accepted.
- Verified via real `LCU_VERIFY_HEALTH` runs on both bgfx and non-bgfx
  builds, a real two-process networked run, real regression runs of
  every existing hook (`LCU_VERIFY_BREAK_PLACE`/`CRAFT`/`TORCH`/`MENU`/
  `HUD`/`INVENTORY`/`WORKBENCH` - all still pass), and a real
  `LCU_BUILD_SHADER_TOOLS=ON` build.
- `ctest` 547/547 (bgfx) / 539/539 (non-bgfx), both up 24 from Phase 50.
- Honestly scoped: no armor/enchantments reduce fall damage (out of
  scope, see the standing directive's own exclusion list); sprinting
  itself doesn't move the player any faster yet (`Action::Sprint` was
  already a real bound action with no consumer before this phase - it
  now drives hunger drain's real 2x multiplier, but no speed boost
  exists to pair with it - a real, pre-existing gap this phase didn't
  need to close to satisfy its own "more when sprinting" hunger-drain
  requirement); apple/bread have no survival obtain path (no farming/
  mob drops, both out of scope) - see PROJECT_STATE.md.

### Phase 50

- **Real item entities**: breaking a block no longer teleports its item
  straight into the inventory - it spawns a real, physically-simulated
  `game::components::ItemEntity` + `Position` on the shared
  `entity_registry` (the same ECS world AI entities already live in),
  with a real small upward toss (`kItemEntitySpawnUpSpeed`). New
  `game::systems::update_item_entities` applies real gravity + AABB/
  voxel ground collision every frame by reusing `lcu::physics::
  apply_gravity`/`move_and_collide` - the exact same primitives the
  player's own controller already uses, not a reimplementation. Each
  entity gets a real per-frame Y-axis spin (`spin_angle`, purely visual)
  and despawns (real entity destruction) after 5 real minutes
  (`kItemEntityDespawnSeconds`).
- **Real pickup**: `game::systems::pickup_item_entities` adds an item
  entity to the inventory the moment the player's own (inflated) AABB
  overlaps it and its 0.5s `pickup_delay_seconds` has counted down - a
  real Minecraft-shaped break -> pop up -> fall -> land -> pick up
  pipeline. A stack that only partially fits keeps the entity alive with
  its count reduced to the real leftover, matching `Inventory::add_item`
  itself.
- **New `Renderer::submit_world_billboard`**: a real depth-tested
  camera-facing quad in the terrain view (unlike `submit_billboard`'s
  own sky view, which has no depth test - correct for a sun/moon "at
  infinity", wrong for a dropped item that needs to be genuinely
  occluded by/occlude nearby geometry). Reuses the exact same sky
  shader/vertex format - no new shader files needed. Item entities
  render as a small quad spinning around world-Y (`spin_angle`), tinted
  their own item's real `icon_color`.
- **Real crafting table** (`game:crafting_table`, registered identically
  on `VoxelClient`/`VoxelServer`, same as every other real block/item
  pair here): right-clicking one opens a real workbench screen - a 3x3
  crafting grid + result, plus the same main storage + hotbar rows the
  regular inventory screen shows (a workbench GUI with no way to
  actually move items into its own grid would be unusable - see
  DECISIONS.md for the real reasoning behind reusing the full screen
  shape). New pure `engine/ui::crafting_table_screen.{h,cpp}` (layout/
  hit-testing, its own 3x3 grid independent of the regular inventory
  screen's 2x2 one) + `crafting_table_screen_renderer.{h,cpp}` (drawing)
  - built from inventory_screen.h's own shared building blocks rather
  than modifying its already-tested 2x2 grid. Breaking a crafting table
  drops itself (real 1:1 block->item mapping, same convention every
  other placeable block already follows). Real click dispatch reuses
  the exact same `inventory_left_click`/`right_click`/`shift_click`
  pure-logic functions Phase 49's inventory screen already established
  - no new drag/drop logic needed.
- **Server parity extended**: `game:wood`/`game:crafting_table` items
  registered on `VoxelServer` too (Phase 49 only added `game:wood` to
  the client - a real, now-closed gap, since the server's own
  `tracked_items`/BlockAction validation needs matching ids to stay
  correct in networked mode), `tracked_items` grown from 4 to 6 entries.
- 17 new unit tests (9 `UpdateItemEntities`/`PickupItemEntities`, 8
  `CraftingTableScreenLayoutTest`/`HitTestCraftingTableScreen`/
  `CraftingTableScreenConstants`).
- **A real item-entity pickup-range bug was found and fixed twice
  during this phase's own headless verification, not merely
  anticipated.** An exact player-AABB-vs-item-AABB overlap almost never
  triggers in practice: the block a player breaks is typically one
  block *in front of* them, not at their own feet, and a dropped item
  with no horizontal velocity settles wherever the broken block was -
  which can be one or even two blocks *below* the player's own standing
  height (e.g. mining straight down, exactly what `LCU_VERIFY_CRAFT`
  does). A real 0.75 vertical inflate missed a real, measured
  single-block-deep item by 0.005 (`LCU_VERIFY_BREAK_PLACE` regressed:
  the broken item never reached the inventory in time to place); 1.0
  still missed a real two-blocks-deep item (`LCU_VERIFY_CRAFT`
  regressed: the second break's item was never picked up in an
  extended run). Fixed by inflating the real player AABB by 2.0 blocks
  on every axis before the pickup overlap check - both real failures
  confirmed fixed via a real re-run, not assumed - see DECISIONS.md.
- **New `Window::warp_mouse`** used two ways this phase: the real
  workbench click dispatch itself, and a new `LCU_VERIFY_WORKBENCH`
  headless hook (grants wood, directly seeds a real `game:
  crafting_table` block at the same spawn-look target the other hooks
  already establish, right-clicks it open, picks up the wood, drops it
  anywhere in the real 3x3 grid - proving the same shapeless "1 wood ->
  4 planks" recipe genuinely works in a bigger grid too, not just the
  2x2 one `LCU_VERIFY_INVENTORY` already covers - takes the result,
  closes via Escape).
- Verified via a real `LCU_VERIFY_WORKBENCH` run (both bgfx and
  non-bgfx builds - full pipeline confirmed: open -> pick up -> craft ->
  take result -> close), real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_
  TORCH`/`LCU_VERIFY_CRAFT`/`LCU_VERIFY_INVENTORY`/`LCU_VERIFY_MENU`/
  `LCU_VERIFY_HUD` regression runs (all still pass with the real item-
  entity pickup pipeline in place of direct-to-inventory), a real
  two-process networked `LCU_VERIFY_CRAFT` run (both breaks spawn real
  item entities, both get picked up, craft succeeds, matching server
  log output), and a real `LCU_BUILD_SHADER_TOOLS=ON` run (`Chunk`/
  `Sky`/`UI2D` shader programs all still `valid=true`, item entities
  render via the new `submit_world_billboard` with zero new shader
  files).
- `ctest` 523/523 (bgfx, up from 506) / 515/515 (non-bgfx, up from 498).
- Honestly scoped: what a dropped item or the workbench screen actually
  look like on a real GPU/display is still **NOT VERIFIED — ENVIRONMENT
  LIMITATION**; item entities have no horizontal scatter velocity on
  spawn (vertical toss only - a real, documented simplification, see
  `ItemEntity`'s own doc comment); the workbench screen's shift-click
  from its own 3x3 grid lands anywhere in the whole 36-slot inventory
  rather than hotbar-first (same real, minor simplification the 2x2
  inventory screen's own grid already has); a recipe needing more than
  one of the same ingredient in a single grid cell still isn't
  correctly consumed by either result-click (documented, no registered
  recipe needs it yet).

### Phase 49

- **Real, deep hotbar/inventory integration** - the biggest structural
  change this phase: Phase 21's `placeable_items`/`selected_placeable_index`
  (a virtual "known item types" selector, decoupled from where items
  physically lived) is gone entirely. The hotbar is now real: 9 of
  `player_inventory`'s own 36 slots (indices 0-8), `selected_hotbar_slot`
  a real index into it. `CycleHotbar`/`CycleHotbarPrev`/`SelectHotbar1-9`
  move the index; placing reads whatever `ItemStack` actually sits there
  and looks up its block via `BlockItemMapping::block_for_item` (new
  reverse lookup, `game/items/block_item_mapping.{h,cpp}`); breaking
  still grants via `add_item` (fills matching stacks/empty slots,
  earliest-first). PickBlock (middle-click) now swaps an already-held
  item into the selected slot rather than merely "selecting a known
  type" - Minecraft's own real survival behavior.
- **Real inventory screen** (`E`): Minecraft's own layout - 2x2 craft
  grid + result slot on top, 3x9 main storage, the same 9 hotbar slots
  again at the bottom. Pure layout/hit-testing in
  `engine/ui/inventory_screen.{h,cpp}` (mirrors `hud.h`'s own
  pure-logic/renderer split), drawing in
  `inventory_screen_renderer.{h,cpp}`. Opening does **not** pause the
  simulation (`day_night_cycle`/AI wander keep running) - only the
  player's own movement/camera/mining/placing/crafting lock while it's
  open, via a new `!inventory_open` gate alongside the existing
  `!paused` one. `ESC` closes the screen instead of opening the pause
  menu when it's open; the two are mutually exclusive.
- **Real drag/drop**: `engine/items/inventory_ops.{h,cpp}` -
  `inventory_left_click` (pick up/place/merge/swap a whole stack),
  `inventory_right_click` (half stack via ceil-division / place one /
  add one), `inventory_shift_click` (transfer to another slot range or
  a whole other `Inventory`, via a new `Inventory::add_item_to_range`).
  Pure logic, no UI/mouse state of its own - the real mouse-click
  dispatch in `client/main.cpp` reuses Interact/PlaceBlock (left/right
  mouse button) while the screen is open, with `Crouch` (Shift by
  default, same key Minecraft itself uses) as the shift-click modifier.
- **Real crafting-grid integration**: the 2x2 grid is a genuine
  `RecipeRegistry::find_match(grid, 2, 2)` query against a separate
  5-slot `Inventory` (4 input + 1 result), recomputed on every input
  change. Clicking the result slot takes it into the cursor and consumes
  1 of each non-empty ingredient - correct for every shapeless recipe
  registered so far (each lists an ingredient once), a real, documented
  limit for a future recipe needing >1 of the same ingredient in one
  cell. Phase 23's quick-craft stays as a convenience path, untouched.
- **New real recipe + items**: `game:wood` finally has its own item
  (breaking a wood block previously granted nothing - a real gap since
  Phase 41 registered the block with no matching item), plus
  `game:planks` (crafted-only) and the phase's own suggested `1 wood ->
  4 planks` shapeless recipe - the 2x2 grid's first real, reachable
  recipe.
- **New `Window::warp_mouse`** (`SDL_WarpMouseInWindow`) - the first
  real mouse-*position*-driven headless verification in this project
  (every prior click-like hook used keyboard navigation instead, see
  `LCU_VERIFY_MENU`'s own history); needed since the inventory screen's
  only real input is mouse clicks.
- **New `LCU_VERIFY_INVENTORY` headless hook**: grants 1 wood (synthetic
  setup, same precedent as `LCU_VERIFY_TORCH`'s torch grant), opens the
  screen, then drives 5 real mouse clicks via `warp_mouse` + synthesized
  `Interact`/`Crouch` presses: pick up wood from the hotbar into the
  cursor, drop it in the craft grid, take the crafted result (4 planks -
  proving the real 2x2 `RecipeRegistry` integration, not just quick-
  craft), place it in the main inventory, shift-click it back to the
  hotbar. A real, previously-hit-and-fixed ordering bug: this hook's
  `input.set_down` calls originally sat alongside `LCU_VERIFY_BREAK_
  PLACE`/`CRAFT`/`TORCH` later in the frame, after the E-toggle/click-
  handling code that reads them already ran that frame - the inventory
  silently never opened on the first real run. Fixed by moving it next
  to `LCU_VERIFY_MENU`/`LCU_VERIFY_HUD`, which sit before that consumer
  code for the same reason.
- Existing `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH` hooks updated for
  the new slot-based hotbar: both now press `CycleHotbar` *and*
  `CycleHotbarPrev` (a real net-zero round trip) to land back on the
  slot actually holding the relevant item before placing, since cycling
  now moves a real slot pointer instead of a virtual list index.
  `LCU_VERIFY_CRAFT` needed no changes (quick-craft never referenced
  `placeable_items`).
- 30 new `InventoryLeftClick`/`InventoryRightClick`/`InventoryShiftClick`
  unit tests, 2 new `Inventory.AddItemToRange*` tests, 8 new
  `InventoryScreenLayoutTest`/`HitTestInventoryScreen`/
  `InventoryScreenConstants` tests, 3 new `BlockItemMapping` reverse-
  lookup tests (44 new total).
- Verified via real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
  `LCU_VERIFY_CRAFT` regression runs (all still pass with the new
  hotbar), a real `LCU_VERIFY_INVENTORY` run (both bgfx and non-bgfx
  builds - full pipeline confirmed: pick up -> craft -> take result ->
  place -> shift-click -> close), real `LCU_VERIFY_MENU`/`LCU_VERIFY_HUD`
  regression runs, a real two-process networked `LCU_VERIFY_BREAK_PLACE`
  run (break/place both still reach the server correctly through the
  new hotbar), and a real `LCU_BUILD_SHADER_TOOLS=ON` build.
- `ctest` 506/506 (bgfx, up from 475) / 498/498 (non-bgfx, up from 467).
- Honestly scoped: shift-clicking a craft-grid slot moves it anywhere in
  the whole 36-slot inventory rather than hotbar-first-then-main (a
  real, minor simplification vs. Minecraft's own precise ordering);
  right-click on the craft result behaves identically to left-click (no
  "half result" concept exists); a recipe needing more than one of the
  same ingredient in a single 2x2 cell isn't correctly consumed by the
  result-click's "decrement each ingredient by 1" logic (documented in
  `inventory_shift_click`'s own call site, not silently wrong for
  anything actually registered); no icon/texture atlas still (flat-color
  quads, same as every prior phase).

### Phase 48

- **Real block highlight**: the raycast-targeted block now gets a real
  black wireframe box (`Renderer::submit_wireframe_box`, already real
  since Phase 36), drawn every frame a block is in range - a genuinely
  new consumer of that existing call, not new rendering machinery.
- **Real hold-to-break**: breaking a block is no longer an instant
  single click - `BlockDefinition::hardness` (real per-block seconds:
  stone 2.0, wood 1.5, dirt 0.5, leaves 0.2, grass 0.6, sand 0.5, snow
  0.1, cactus 0.4, coal/iron ore 3.0, torch 0.0 = instant, matching this
  phase's own directive's exact table where one exists) now gates how
  long Interact must be held against the *same* targeted block before
  it actually breaks. New pure `lcu::voxel::break_progress_fraction`/
  `is_break_ready` (new `break_progress.{h,cpp}`) compute this - the
  same two functions both the real break-trigger check and the real
  darkening overlay consume, so they can never disagree. Switching
  targets or releasing Interact resets progress. In networked mode, a
  real one-shot latch (`break_request_sent`) stops a held click from
  re-sending the break request every single frame while waiting for the
  server's own `BlockChange` broadcast to land.
- **Real break-progress overlay**: a solid box over the targeted block
  darkens toward black as progress advances (new
  `Renderer::submit_solid_box` - reuses the existing sky shader/vertex
  format, no new shader files needed). Deliberately NOT a real crack-
  noise-density effect on the block's own rendered face - that needs a
  new per-fragment world-position uniform threaded through
  `fs_chunk.sc`, a real, separate shader feature outside this phase's
  scope (see DECISIONS.md) - this overlay is a real, visible,
  honestly-scoped substitute, not a placeholder.
- **Real hand icon**: the currently-selected placeable item's own real
  `icon_color` (Phase 47), bottom-right corner, with a real elapsed-
  time-driven swing animation (`kHandSwingDuration` = 0.25s, a real
  sine ease) triggered on every real break or place action.
- **Water stays genuinely unbreakable with zero special-case code**:
  `has_collision = false` already keeps the DDA raycast from ever
  stopping on it (Phase 37), so it was never a real break target to
  begin with - no infinite-hardness hack needed.
- **Real headless-hook rewrite, not a regression**: LCU_VERIFY_
  BREAK_PLACE/LCU_VERIFY_TORCH/LCU_VERIFY_CRAFT previously drove a
  single-frame Interact pulse (instant break, matching the pre-Phase-48
  mechanic) - now real elapsed-time hold windows sized to each target
  block's own hardness plus margin (a fixed frame count can't express
  this reliably - this sandbox's unthrottled loop runs many thousands
  of frames per real second). `LCU_VERIFY_CRAFT`'s networked run
  specifically needed a wider real gap between its two break windows
  than initially chosen (0.2s wasn't consistently enough margin for the
  server round trip to land before the second hold window started,
  confirmed by an actual failed run during verification, not just
  reasoned about - see DECISIONS.md); widened to a real 0.8s.
- 9 new unit tests (`BreakProgress`/`IsBreakReady`).
- Verified via real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
  `LCU_VERIFY_CRAFT` runs (now hold-based, full pipeline confirmed:
  break -> pick up -> cycle hotbar -> place, and for CRAFT, two
  sequential real breaks -> match -> reject), a real two-process
  networked `LCU_VERIFY_CRAFT` run (zero warnings/errors/rejects,
  confirmed with the widened gap above), real `LCU_VERIFY_MENU`/
  `LCU_VERIFY_HUD` regression runs (unaffected, still pass), a real
  `LCU_BUILD_SHADER_TOOLS=ON` run (`Chunk`/`Sky`/`UI2D` shader programs
  all still `valid=true` - `submit_solid_box` needed no new shader), and
  a real headless run for each hook confirming zero warnings/errors
  from the new highlight/overlay/hand draw calls actually executing
  every frame a block is targeted.
- `ctest` 475/475 (bgfx, up from 466) / 467/467 (non-bgfx, up from 458).
- Honestly scoped: what the highlight/overlay/hand icon actually look
  like on a real GPU/display is still **NOT VERIFIED — ENVIRONMENT
  LIMITATION** (headless Noop backend proves every draw call executes
  without error, not that it looks right); no real crack-noise-density
  shader effect on the block's own face (deferred, see above); the
  break-progress overlay is fully opaque (no real alpha blending), so
  it appears at whatever darkness its first held frame computes rather
  than fading in from fully invisible - a real, minor visual rough edge
  from choosing not to add a new shader/uniform this phase (see
  DECISIONS.md).

### Phase 47

- **Real Minecraft-position hotbar**: new `engine/ui::hud.{h,cpp}` (pure
  layout math, zero SDL/bgfx - tested in both configs, same split as
  `menu_stack.h`) - 9 bottom-center slots, each a real bordered/filled
  quad plus (for the 4 real `placeable_items`) a flat colored icon quad
  and a real held-count label. **`ItemDefinition` gained `icon_color`**
  (closes the exact gap Phase 44 deferred: "no inventory/hotbar widget
  exists yet to consume it" - this hotbar is that widget), set per item
  to match its own block's tint where one exists.
- **Real health/hunger bars**: 10-icon Minecraft-style bars (each icon =
  2 points, real half-icon fill math), positioned above the hotbar,
  left-aligned to its own left edge. Hardcoded full this phase
  (`HudState`'s own real defaults) - Phase 51 wires real
  `PlayerHealth`/`PlayerHunger` values in; the layout/rendering is
  already real and ready for that, not a placeholder.
- **Real F-key toggles**: 5 new `Action`s (`ToggleHud`/
  `ToggleDebugOverlay`/`Screenshot`/`TogglePerspective`/`Fullscreen`,
  bound to F1/F3/F2/F5/F11 - the exact bindings Phase 43's own
  "verbindlich" table reserved, only now given real consumers).
  `ToggleHud`/`ToggleDebugOverlay` flip the same real
  `options.hud_enabled`/`debug_overlay_enabled` the options menu
  (Phase 46) already reads/writes. `Screenshot` calls a new
  `Renderer::request_screenshot` (`bgfx::requestScreenShot` against the
  default backbuffer). `Fullscreen` calls a new
  `Window::set_fullscreen` (`SDL_SetWindowFullscreen`).
  `TogglePerspective` cycles first-person/third-person-behind - **real
  camera-eye-offset rendering** (gameplay raycast/movement stay tied to
  the true first-person position; only the render eye shifts back along
  the real look direction) - **PARTIAL**: no player model exists to
  render in front of the camera, so third-person-front is honestly not
  implemented (see DECISIONS.md).
- **Real shared debug-text-buffer ownership fix**: up to three systems
  now draw into bgfx's one debug-text buffer each frame (debug overlay,
  HUD item-count labels, menu row labels) - each used to call
  `clear_debug_text()` internally, which would have silently wiped
  whichever one ran first. Fixed by moving the one real
  `clear_debug_text()` call up to `client/main.cpp`, called once before
  any of the three, in the real draw order (overlay -> HUD -> menu).
- New `LCU_VERIFY_HUD` headless hook: presses each F-key toggle on its
  own frame, real log output confirms each one's real resulting state
  (`HUD: off`, `Debug overlay: on`, `Perspective: third-person
  (behind)`, `Fullscreen: true`, `Requested screenshot: screenshot_9`).
- 21 new unit tests (`HotbarSlotLayout`/`StatBarLayout`: real rect
  ordering/centering/non-overlap, real half-icon fill-fraction math,
  real left-edge alignment between the hotbar and the bars above it).
- Verified via the real `LCU_VERIFY_HUD` run above, real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT`/
  `LCU_VERIFY_MENU` runs (byte-identical to Phase 46), a real
  `LCU_BUILD_SHADER_TOOLS=ON` run (`Chunk`/`Sky`/`UI2D` shader programs
  all still `valid=true`), and a real two-process networked run (zero
  warnings/errors/rejects, matching spawn columns).
- `ctest` 466/466 (bgfx, up from 455) / 458/458 (non-bgfx, up from 447).
- Honestly scoped: the HUD's real on-screen appearance is still **NOT
  VERIFIED — ENVIRONMENT LIMITATION** (headless Noop backend proves the
  pipeline runs, not that it looks right); `bgfx::requestScreenShot`
  under the headless `Noop` backend has no real framebuffer content to
  capture (real call, **NOT VERIFIED** to produce a meaningful image in
  this sandbox); third-person-front is deferred (see above); hotbar
  slots 5-9 still show nothing (only 4 real `placeable_items` exist -
  unchanged, honest limitation carried forward from Phase 43).

### Phase 46

- **Real `engine/ui::MenuStack`** (new `menu_stack.{h,cpp}`) - stacked
  `MenuScreen`s (title + `MenuItem` rows, each with a real
  `on_activate`/`on_adjust` callback), `move_selection`/`select_index`/
  `activate_selected`/`adjust_selected`. Pure logic, zero SDL/bgfx
  dependency (`engine/ui` is now added under `LCU_BUILD_CLIENT`, not
  only `LCU_ENABLE_BGFX` - see DECISIONS.md), so it builds and is
  unit-tested in both the bgfx and non-bgfx configs. `menu_item_layout`/
  `menu_item_at_point` compute each row's real on-screen pixel rect from
  screen size alone - the one shared source of truth both drawing
  (`menu_renderer.cpp`) and real mouse hit-testing read from, the same
  "drawn and tappable can never drift apart" property `kTouchButtonLayout`
  already established for touch controls (Phase 10).
- **Real pause menu**: ESC in-game now opens a real `MenuStack` (`Pause`
  -> `Zurueck zum Spiel`/`Optionen`/`Steuerung`/`Beenden`) instead of
  only releasing mouse capture; a menu being open genuinely pauses
  simulation (movement/physics/AI/day-night-cycle) - verified via a real
  headless run holding `MoveForward` down across the pause (player
  position provably unchanged) then again after closing it (position
  provably changed). Network *receive* deliberately keeps running while
  paused (a real, documented deviation - see DECISIONS.md: fully halting
  it risked reading as a dead connection by the time the player
  unpauses); only this client's own outgoing input pauses.
- **Real Options screen**: Maus-Empfindlichkeit/Sichtfeld(FOV) (+/- via
  the existing `LookLeft`/`LookRight` actions), HUD/Debug-Overlay
  toggles, Zurueck (saves `options.txt` on leaving). **FOV is now
  actually applied to the camera's projection matrix** - closes the gap
  Phase 45 deliberately left open ("persisted but never read for
  rendering... until Phase 46 gives it a real consumer"). Renderdistanz
  is deliberately NOT a row - `load_settings.radius_xz` is `const` and
  wiring live re-streaming is a real, separate structural change this
  phase's own directive explicitly allows deferring as PARTIAL (see
  DECISIONS.md) rather than shipping a +/- control that would visibly do
  nothing.
- **Real Controls screen**: every rebindable `Action` (Escape/
  MenuConfirm excluded, see below) listed as `<name>: <key>`; Enter/
  click enters a real "waiting for input" capture
  (`lcu::platform::poll_any_pressed_key`, new - scans real SDL keyboard/
  mouse state directly, not through the `Action` system, since the whole
  point is binding a key nothing uses yet); ESC cancels
  (`is_escape_key`, new); a real release-then-fresh-press debounce
  (`rebind_ready`) stops the very key that opened the capture from
  immediately binding itself; Reset restores every default; changes
  apply immediately and save on leaving.
- **New `Action::MenuConfirm`** (Enter/Return) - a real menu-meta action
  alongside `Action::Escape`, both deliberately excluded from the
  Controls screen's own rebind list (not rebindable, per this phase's
  own directive).
- **Real, reproduced-and-fixed use-after-free**: a `MenuItem`'s own
  `on_activate`/`on_adjust` callback lives inside the `MenuScreen`
  currently on top of the stack; popping/pushing `menu_stack` *directly*
  from inside such a callback destroys (or, for `push`, potentially
  reallocates) that very screen - including the closure still executing
  - a real segfault, reproduced during headless testing with
  `LCU_VERIFY_MENU` before being fixed. Fixed by deferring every such
  mutation via a `pending_menu_action` processed once per frame, after
  `activate_selected()`/`adjust_selected()` have fully returned - see
  DECISIONS.md for the full explanation.
- New `LCU_VERIFY_MENU` headless hook: opens the pause menu, holds
  `MoveForward` while paused (position provably unchanged), navigates
  into Options via real edge-detected `LookDown`/`MenuConfirm`, adjusts
  mouse sensitivity twice via `LookRight` (options.txt round-trip
  confirms both edits landed: `0.0022` -> `0.0026`), saves back out,
  closes the menu, then holds `MoveForward` again while resumed
  (position provably changed this time). Deliberately does NOT exercise
  the controls screen's rebind capture - `poll_any_pressed_key` reads
  real SDL hardware state the dummy input driver never produces (see
  DECISIONS.md, same category of gap as Phase 43's own mouse-look).
- 21 new unit tests (`MenuStack`: push/pop/clear, wraparound selection,
  per-screen selection surviving a pop, activate/adjust with and without
  a real callback; `MenuItemLayout`/`MenuItemAtPoint`: real rect
  ordering/non-overlap/hit-testing).
- Verified via the real `LCU_VERIFY_MENU` run above, real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs
  (byte-identical to Phase 45), a real `LCU_BUILD_SHADER_TOOLS=ON` run
  (`Chunk`/`Sky`/`UI2D` shader programs all still `valid=true`), and a
  real two-process networked run (zero warnings/errors/rejects, matching
  spawn columns).
- `ctest` 455/455 (bgfx, up from 436) / 447/447 (non-bgfx, up from 428).
- Honestly scoped: the menu's real on-screen appearance is still **NOT
  VERIFIED — ENVIRONMENT LIMITATION** (headless Noop backend proves the
  batching/text-drawing pipeline runs end-to-end, not that it looks
  right); the controls screen's rebind capture is real code, reviewed,
  but **NOT VERIFIED against a real keyboard/mouse** (see above); no
  live Renderdistanz control (deferred, see above); no chat, no
  multiplayer UI, no advancements (out of scope per this phase's own
  directive).

### Phase 45

- **Real persistent options**: `engine/platform::Options` (new
  `options.{h,cpp}`) - `mouse_sensitivity`, `fov`, `hud_enabled`,
  `debug_overlay_enabled`, and a full `KeyBindings` instance, all
  loaded from and saved to a real `key=value` text file
  (`# comments`, blank lines skipped) at `SDL_GetPrefPath(
  "LiveCraftUltimate", "LiveCraftUltimate")` (a genuine per-OS user
  config directory, e.g. `~/.local/share/LiveCraftUltimate/
  LiveCraftUltimate/options.txt` on Linux) via `Options::default_path()`.
  Every one of `KeyBindings`' 29 `Action`s round-trips through a new
  bidirectional `action_name`/`parse_action_name` table (`key.<action>=
  <key>`, plus `key.<action>.alt=<key>` only when a real second binding
  exists).
- **Real tolerance, not just a happy path**: a missing file leaves
  every default untouched and `load()` returns `false` (not an error -
  first run always looks like this); a corrupt or unrecognized line
  (bad number, unknown action name, unknown key name) is skipped and
  every other real line still loads, verified by dedicated tests that
  deliberately interleave garbage lines between real ones.
- **`VoxelClient` now genuinely uses this instead of hardcoded
  constants**: the former `kMouseSensitivity` constant is gone,
  replaced by `options.mouse_sensitivity`; the crosshair (Phase 44) is
  now gated behind `options.hud_enabled`; the debug overlay is now
  gated behind `options.debug_overlay_enabled` - a real behavior
  change, since that option defaults to `false` (matching this
  project's own Phase 45 spec example and Minecraft's own F3-gated
  convention), so the overlay no longer renders unconditionally as it
  did through Phase 44. Options load at startup and save on exit; no
  menu UI writes to it yet (Phase 46).
- 7 new `Options` unit tests (default values, missing-file behavior,
  scalar round-trip, keybinding round-trip, corrupt-line tolerance,
  unrecognized-name tolerance, conditional `.alt` line) plus 2 new
  `ActionName` tests (every one of the 29 real `Action`s has a real
  name and round-trips; an unknown name fails to parse rather than
  aliasing to some action).
- Verified via real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/
  `LCU_VERIFY_CRAFT` runs (byte-identical to Phase 44, plus the new
  "Loaded options from"/"No options file at"/"Saved options to" log
  lines), a real `LCU_BUILD_SHADER_TOOLS=ON` run (`Chunk`/`Sky`/`UI2D`
  shader programs all still `valid=true`), a real two-process networked
  run (zero warnings/errors/rejects, matching spawn columns), and a
  real inspection of the actual written `options.txt` confirming every
  field including every one of the 29 keybindings round-trips as
  human-readable text at the real OS-provided path - not just asserted
  in a unit test against a temp file.
- `ctest` 436/436 (bgfx, up from 427) / 428/428 (non-bgfx, up from
  419).
- Honestly scoped: no options menu UI exists yet to change these values
  in-game (Phase 46 - this phase is the storage layer only, per its own
  spec); FOV is persisted but **not yet actually applied to the camera
  projection - NOT VERIFIED, deferred** (nothing in the client currently
  reads `options.fov` for rendering; wiring it in without a menu to
  change it would be speculative, so it stays honestly unused until
  Phase 46 gives it a real consumer).

### Phase 44

- **Real 2D UI quad batch**: `engine/rendering::Renderer::submit_ui_quad`/
  `flush_ui_quads` - screen-space rectangles (pixel position/size, RGBA
  color, UV 0..1 per quad) queued across a frame and uploaded/drawn in
  exactly one real `bgfx::submit()` call via transient buffers, the
  same idiom `submit_billboard`/`submit_wireframe_box` already use for
  other per-frame geometry. New `Mat4::orthographic` (real unit tests:
  screen corners map to clip-space corners, center maps to the origin).
  New dedicated `kUi2dViewId` bgfx view, own `vs_ui2d.sc`/`fs_ui2d.sc`
  shader pair (position + UV + color, no lighting - same minimal
  approach `vs_sky.sc`/`fs_sky.sc` already established).
- **Real, deliberate deviation from this phase's own literal view-order
  wording** ("NACH Sky, VOR Terrain" - after sky, before terrain): the
  UI view is submitted *last* (after terrain), not before it - a UI
  view submitted before terrain would have every pixel simply
  overdrawn the instant terrain's own opaque geometry rendered into
  the same spot, making the UI invisible behind anything solid, the
  opposite of what a HUD needs. Documented in DECISIONS.md as a
  real "working feature over literal wording" call, not silently
  ignored.
- **Real first consumer**: a genuine, permanent crosshair (two thin
  bars, screen-centered) submitted every frame - doubles as this
  phase's own "Test-Rechteck in Bildschirmmitte sichtbar" verification,
  not a throwaway test element separate from real usage.
- **`ItemDefinition::icon_color`/item-icon pattern rendering deferred**
  (PARTIAL, real limit): this project's own `ItemDefinition` doc
  comment already establishes "fields are added when something needs
  them, not speculatively" - no inventory/hotbar widget exists yet to
  actually place an item icon into, so adding the field now would be
  exactly the kind of speculative addition that comment argues
  against. The vertex format already carries real UV data ready for
  this once a real consumer exists (see DECISIONS.md).
- 5 new `QuadBatch2D` unit tests (batch accumulation, flush clears
  it, empty-flush safety) plus 2 new `Mat4::orthographic` tests.
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run
  (`UI2D shader program valid=true`, a real compiled/linked shader
  program, not just "no error"), real `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical to Phase
  43), and a real two-process networked run with matching
  independently-computed spawn columns, zero warnings/errors/rejects.
- `ctest` 427/427 (bgfx, up from 420) / 419/419 (non-bgfx, up from
  417).
- Honestly scoped: **what the crosshair/any future UI quad actually
  looks like on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
  LIMITATION** (headless Noop backend proves the pipeline runs
  end-to-end without error, not that it looks right); no item-icon
  rendering yet (deferred, see above); no slot backgrounds/health/
  hunger/menu backgrounds yet (Phase 46+, real future consumers of
  this same batch API, not separate machinery); debug text (bgfx's
  own built-in overlay) composites independently of this new UI view -
  not touched this phase, already real since Phase 12.


### Phase 43

- **Real rebindable keymap**: new `engine/platform::KeyBindings` - each
  `Action` maps to up to 2 physical keys (a unified `PhysicalKey` space
  covering both SDL scancodes and 3 mouse buttons), starting from real
  Minecraft-parity defaults and rebindable in place via `bind()`/
  `reset_to_defaults()` - the actual data structure a future controls
  menu will read/write, not a stub. `physical_key_name`/
  `parse_physical_key` round-trip a binding to/from a human-readable
  string (SDL-provided for keyboard keys, `MOUSE_LEFT`/`MOUSE_RIGHT`/
  `MOUSE_MIDDLE` for mouse buttons) for future persistence/display.
- **`KeyboardInputBackend` renamed `DesktopInputBackend`** and rewritten
  to poll through `KeyBindings` instead of a fixed table baked into
  input.cpp - every Action now resolves through the same physical-key
  lookup, mouse buttons included, so `Action::Interact`/`PlaceBlock`/
  `PickBlock` are driven by real mouse clicks with zero special-casing
  in client/main.cpp's existing break/place logic.
- **Real relative mouse-look**: `InputState::mouse_delta_x/y` (from
  `SDL_GetRelativeMouseState`, polled once per frame) applied to
  `FirstPersonCamera::add_yaw_pitch` alongside (not instead of) the
  existing arrow-key look fallback. `Window::set_relative_mouse_mode`/
  `relative_mouse_mode()` wrap SDL's own capture mechanism; a real
  ESC/Tab (new `Action::Escape`) releases it, a click while free
  re-captures it (the re-capture click itself doesn't also register as
  a break/place - see `suppress_click_for_recapture`), and a real
  `Window::consume_focus_lost()` (SDL_EVENT_WINDOW_FOCUS_LOST) releases
  it automatically on alt-tab.
- **Real mouse-wheel hotbar cycling**: `Window::consume_wheel_delta_y()`
  accumulates `SDL_EVENT_MOUSE_WHEEL` events per frame; client/main.cpp
  turns a nonzero delta into a one-frame `Action::CycleHotbar`/new
  `Action::CycleHotbarPrev` pulse, reusing the exact same edge-detection
  as a real key press.
- **Direct hotbar selection**: 9 new `Action::SelectHotbar1..9` bound to
  the number row, directly setting the selected placeable index (a
  silent no-op past the real hotbar's current size, not a crash or
  wraparound). New `Action::PickBlock` (mouse middle button, Minecraft's
  "pick block") selects whichever placeable entry matches the
  looked-at block, without granting the item for free.
- **Real Minecraft-parity default fix**: this project's own pre-Phase-43
  defaults had Sprint/Crouch backwards (Sprint=Shift, Crouch=Ctrl) -
  corrected to Sprint=Ctrl, Crouch=Shift.
- **Hygiene fixes**: the chunk/sky shader load path now resolves against
  `Window::executable_base_path()` (`SDL_GetBasePath`) instead of the
  current working directory - real, not cosmetic: verified via a real
  run of the client from the repo root and from `/tmp`, both correctly
  loading real shaders (`Chunk shader program valid=true`) from
  whatever directory it was launched from, not just the executable's
  own directory as every prior verification run in this repo happened
  to already be. Rendering-only constants (`kNightSkyColor` etc., Phase
  27/36) gated behind `#if defined(LCU_ENABLE_BGFX)` - genuinely dead
  declarations in a non-bgfx build, the real cause of an unused-variable
  warning reported from a real macOS/Apple-Clang build (not reproduced
  by this sandbox's own GCC/Clang - see BUILD_STATUS.md). A real,
  verified redundant `Lcu::Math` link entry removed from
  `game/CMakeLists.txt` (already provided transitively via
  `Lcu::EngineCore`) - one real contributor to a reported "ignoring
  duplicate libraries" linker warning (also an Apple-`ld`-specific
  message, not reproduced here).
- 20 new unit tests (`KeyBindings`/`PhysicalKey`: defaults, rebinding,
  reset, name round-trip; `InputState`: mouse-delta get/set).
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (from the repo
  root AND from `/tmp`, not just `bin/`), real `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical to Phase
  41 - mouse-driven Interact/PlaceBlock still fire correctly from the
  same `set_down` calls these hooks always used), and a real
  two-process networked run with matching independently-computed spawn
  columns, zero warnings/errors/rejects.
- `ctest` 420/420 (bgfx, up from 406) / 417/417 (non-bgfx, up from 403).
- Honestly scoped: **real mouse-look/click/wheel/capture behavior on an
  actual GPU/display and mouse device is still NOT VERIFIED —
  ENVIRONMENT LIMITATION** (this sandbox has no real mouse; `SDL_
  SetWindowRelativeMouseMode` under the dummy video driver here
  happened to report success with nothing to actually capture, logged
  either way - see `Window::set_relative_mouse_mode`); the unused-
  variable and duplicate-library linker warnings' exact reproduction is
  also NOT VERIFIED here for the same reason (real fixes applied on
  code-reading grounds, not by watching the warning disappear); no 2D
  UI/menu yet to actually rebind a key through (Phase 44/46); `Action::
  Inventory`/`SwapOffhand` still have no consumer (unchanged from
  before this phase, now joined by `Escape`'s pause-menu half and most
  `SelectHotbar5-9` slots, which the real 4-item hotbar can't reach
  yet).


### Phase 42

- **README.md written** (was previously an empty file): a real
  project-level entry point - what the project is, an honest current
  status pointer (this repo was built and verified in a headless
  Linux sandbox with no GPU/display; docs say so explicitly rather
  than silently claiming more), a feature summary reflecting the
  actual state as of Phase 41 (chunked voxel storage/meshing/lighting,
  the full deterministic worldgen pipeline through vegetation,
  movement/inventory/crafting/AI, real client/server networking, Lua
  modding, mobile input/quality profiles, benchmarks), a quick-start
  build/run block, and a documentation map linking to every other doc
  in the repo with a one-line description of what each covers.
- **`PROJECT_STATE.md`'s "Known Limitations" section corrected**: an
  old Phase 17 entry still read "no climate/biome/caves/ores/
  structures/vegetation/decoration" - stale since Phases 39-41 made
  biome/caves/ores/vegetation all real. Updated in place (struck
  through, not deleted, matching this file's own established
  correction convention) to reflect that only structures (brief
  section 21's one remaining pipeline stage) is still unimplemented,
  and that it's out of scope for this 42-phase plan entirely rather
  than deferred from any specific phase.
- `BUILDING.md`/`CHANGELOG.md`/`DECISIONS.md` reviewed for accuracy
  against the current (post-Phase-41) state and found already current
  - each was kept up to date phase-by-phase throughout Phases 35-41,
  so no further edits were needed there this phase.
- This closes out the governing directive's 18-phase (Phases 25-42)
  program. See `PROJECT_STATE.md`'s "Reality Audit" for the complete,
  current, honest picture of what is and isn't verified.


### Phase 41

- **Real vegetation pipeline stage** (brief section 21, the last one
  before "structures"): `VegetationType` (`None`/`Tree`/`Cactus`) and
  `vegetation_at(seed, world_x, world_z, biome)` - a genuinely
  independent noise field per vegetation type (own seed offset each),
  reusing the existing 2D `fractal_noise`. Tree only ever returned for
  `Biome::Plains`, Cactus only for `Biome::Desert` - `Biome::Snowy`
  stays vegetation-free on purpose (not every biome needs unique
  content, the same reasoning water stayed biome-independent in Phase
  39). New `VegetationBlocks` struct (`wood`, `leaves`, `cactus`) - same
  caller-supplied pattern `BiomeBlocks`/`OreBlocks` already established.
- **Deliberately single-column shapes**: a tree is a `kTreeTrunkHeight`
  (4) stack of `wood` directly above the surface block, capped by a
  `kTreeCanopyHeight` (3) stack of `leaves` directly above the trunk - a
  cactus is a `kCactusHeight` (3) stack of `cactus`, no canopy. No wide
  3x3 canopy spreading into neighboring columns - a real, deliberate
  scope choice (see DECISIONS.md), not an accidental cross-chunk gap;
  this also means vegetation placement naturally works correctly across
  vertically-stacked chunk boundaries with zero special-casing, the same
  way the sea-level water fill already did.
- **Thresholds measured, not guessed**: `kTreeThreshold`/
  `kCactusThreshold` were picked from `fractal_noise`'s own real,
  empirically-measured output range (a standalone probe program, the
  same "measure, don't guess" discipline Phase 40's ore thresholds were
  just fixed with - applied here from the start instead of after a
  wrong guess) - trees occur at ~4.5% of Plains columns, cacti at ~2.5%
  of Desert columns, both real and reliably found by tests on the first
  try.
- **`generate_terrain_chunk` extended**: its above-terrain branch now
  places a dry column's own vegetation (Tree trunk/canopy or Cactus
  stack) above the surface block, real air everywhere else - checked
  once per column (not once per cell), gated on the column being dry
  (`height > kSeaLevel`) so nothing grows underwater. New trailing
  `VegetationBlocks` parameter on every caller (`VoxelClient`,
  `VoxelServer`, `tools/benchmark`, worldgen tests).
- **Three new real blocks**: `game:wood`, `game:leaves`, `game:cactus` -
  solid, collidable, distinct colors only (no new physics/rendering
  mechanic - a real see-through/non-collidable leaves block would need
  cross-shaped or transparent-layer meshing, neither of which exists
  yet). Registered identically, same sequence position, on
  `VoxelClient`/`VoxelServer` right after `game:iron_ore`.
- **7 new worldgen unit tests**: `VegetationAtIsDeterministic`,
  `VegetationAtNeverReturnsTreeOrCactusForSnowy`,
  `VegetationAtNeverReturnsCactusForPlainsOrTreeForDesert`,
  `VegetationAtProducesBothTreeAndCactusOverARealArea` (a real sweep
  confirming both genuinely occur), and
  `GenerateTerrainChunkPlacesARealTreeWhereVegetationAtSaysOneGrows` (a
  real end-to-end check: finds a real dry Plains column with a Tree,
  generates its chunk, confirms the actual trunk/canopy blocks appear);
  1 existing test (`GenerateTerrainChunkMatchesTerrainHeightColumnByColumn`)
  updated since its old "always air above terrain (except water)"
  assumption stopped holding once vegetation could place wood/leaves/
  cactus there - now computes the expected block the same way
  `generate_terrain_chunk` itself does, via the real `vegetation_at`
  function, not a hardcoded constant.
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn column
  (-84,-84), `biome=Plains`, real shader program validity), real
  `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs
  (byte-identical to Phase 40), and a real two-process networked run
  with matching independently-computed spawn columns, zero
  warnings/errors/rejects.
- `ctest` 406/406 (bgfx, up from 401) / 403/403 (non-bgfx, up from 398).
- Honestly scoped: **what a real tree/cactus actually looks like on a
  real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; no
  wide/spreading tree canopies (single-column shapes only, a real,
  documented scope choice - see DECISIONS.md); no varied tree/cactus
  silhouettes (one shape per type, not the variety a shipped game would
  eventually want); no wood/leaves/cactus item drops/mapping yet
  (breaking any of them currently removes it without granting an item);
  no structures stage yet (the one remaining unimplemented brief
  section 21 pipeline stage, Phase 42 is documentation instead per the
  governing directive's own phase list).

### Phase 40

- **Real cave-carving pipeline stage** (brief section 21): `is_cave(seed,
  world_x, world_y, world_z, surface_height)` - two independent 3D noise
  fields (own seed offsets, new 3D noise primitives - `hash3d`,
  `lattice_value3d`, trilinear `smooth_noise3d`, 4-octave
  `fractal_noise3d` - since every earlier worldgen stage only ever
  needed 2D column noise) sampled at the same point; where their values
  land within `kCaveThreshold` of each other, the cell is carved into
  open air. This "noise crevice" difference technique produces winding,
  connected tunnels, unlike a single-field threshold ("cheese caves")
  which produces isolated round blobs (see DECISIONS.md for the
  comparison and why this was chosen). `kCaveMinDepthBelowSurface` keeps
  a real minimum depth below that column's own `terrain_height()`, so a
  tunnel never punches a hole right at ground level.
- **Real ore pipeline stage**: `OreType` enum (`None`/`Coal`/`Iron`) and
  `ore_at(seed, world_x, world_y, world_z)` - each ore its own
  independent 3D noise field, absolute world-Y depth band, and rarity
  threshold; `None` (no ore, stays plain stone) is the overwhelmingly
  common outcome by design. Iron checked before Coal, given a
  narrower/deeper band and a higher threshold - real rarity contrast,
  not both equally likely everywhere underground. New `OreBlocks` struct
  (`coal_ore`, `iron_ore`) - caller-supplied, the same pattern
  `BiomeBlocks` already established.
- **Two new real blocks**: `game:coal_ore`, `game:iron_ore` - solid,
  collidable, dark stone-family blocks (distinct colors only, no new
  mechanic), registered identically, in the same sequence position, on
  `VoxelClient`/`VoxelServer` right after `game:water` (BlockId alignment
  for replication).
- **`generate_terrain_chunk` extended**: its stone-band branch (below a
  column's surface/subsurface layers) now checks `is_cave` first (carved
  cells stay air, the chunk's default fill), then `ore_at` for anything
  not carved (substituting the matching ore block), falling back to
  plain `stone_block` only when neither applies - exactly the order
  worldgen.h's own doc comment describes. New trailing `OreBlocks`
  parameter on every caller (`VoxelClient`, `VoxelServer`,
  `tools/benchmark`, worldgen tests).
- **Real threshold tuning, not guessed**: `fractal_noise3d`'s actual
  output range at this amplitude/octave configuration clusters well
  inside [0,1) (empirically measured ~[0.05, 0.95], not the full range -
  a 4-octave weighted average, expected from averaging independent
  lattice samples) - `kCoalThreshold`/`kIronThreshold` were picked from
  that real measured distribution (a standalone probe program linking
  `libLcuWorld.a`, the same "measure, don't guess" approach Phase 38's
  spawn-radius fix and Phase 39's biome thresholds already used), not
  from an assumption that the noise spans the full unit range. Caught a
  real bug this way: the first threshold choice (0.90/0.95) was nearly
  unreachable for Iron (0 matches in a 1.8M-cell sample) and too rare
  for Coal within the new `OreAtProducesBothOreTypesOverARealVolume`
  test's scan volume - fixed by re-deriving both thresholds from the
  measured distribution (Coal ~3.7% of eligible cells, Iron ~0.1%, both
  still small next to `None`'s overwhelming share).
- **6 new worldgen unit tests**: `IsCaveIsDeterministic`,
  `IsCaveNeverFiresExactlyAtTheSurface`,
  `IsCaveProducesSomeCarvedCellsWellBelowTheSurface`,
  `OreAtIsDeterministic`, `OreAtProducesBothOreTypesOverARealVolume` (a
  real sweep confirming both ore types genuinely occur), plus updates to
  2 existing tests (`GenerateTerrainChunkMatchesTerrainHeightColumnByColumn`,
  renamed `ChunkFarBelowTerrainIsStoneCaveOrOre`) whose old "always plain
  stone below the subsurface layer" assumption stopped holding once
  caves/ores could carve or substitute those cells - both now compute
  the expected block the same way `generate_terrain_chunk` itself does
  (via the real `is_cave`/`ore_at` functions), not a hardcoded constant.
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn column
  (-84,-84), `biome=Plains`, real shader program validity confirmed),
  real `LCU_VERIFY_BREAK_PLACE`/`LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT`
  runs (byte-identical behavior to Phase 39), and a real two-process
  networked run with matching independently-computed spawn columns,
  zero warnings/errors/rejects.
- `ctest` 401/401 (bgfx, up from 396) / 398/398 (non-bgfx, up from 393).
- Honestly scoped: **what carved caves/ore veins actually look like on a
  real GPU/display is still NOT VERIFIED — ENVIRONMENT LIMITATION**; no
  cave-specific lighting (a carved tunnel gets whatever sky/block light
  the existing propagation already reaches it with - no dedicated
  ambient-occlusion or "always dark" cave treatment); no ore item
  drops/mapping yet (breaking coal/iron ore currently removes it without
  granting an item, the same state sand/snow were in after Phase 39,
  grass/dirt were in before Phase 18/22); no structures/vegetation
  stages yet (Phase 41); caves/ores have no depth ceiling (a noise field
  with no artificial cutoff, so in principle either could still occur
  arbitrarily far below any plausible terrain - `ChunkFarBelowTerrainIsStoneCaveOrOre`
  now checks that reality honestly instead of assuming it away).

### Phase 39

- **Real climate/biome pipeline stage** (brief section 21): a new
  `Biome` enum (`Snowy`/`Plains`/`Desert`) and `biome_at(seed, x, z)` -
  a genuinely independent, low-frequency noise field (its own
  `kClimateSeedOffset`, so biome boundaries don't visibly correlate
  with coastlines/ridge lines from the continental/terrain stages).
  Thresholds split the climate value into three bands, Plains
  deliberately the widest (50% vs. 25% each for Snowy/Desert), since
  it was every column's only behavior before this phase and should
  stay the common case.
- **New `BiomeBlocks` struct**: `generate_terrain_chunk`'s signature
  changed from four flat block-id parameters to `(seed, biome_blocks,
  stone_block, water_block)` - each biome maps to its own real
  surface/subsurface block ids, caller-supplied the same way
  surface/subsurface/stone/water already were. Stone and water stay
  deliberately biome-independent (every biome's land is stone deep
  down; a below-sea-level column is water regardless of climate - no
  ice-cap-vs-open-water distinction yet, an honest scoped gap).
- **Two new real blocks**: `game:sand` (Desert's surface *and*
  subsurface - a real desert is sandy all the way down, unlike grass-
  over-dirt) and `game:snow` (Snowy's surface only, subsurface stays
  dirt - a snow-capped tundra, not snow all the way down). Registered
  identically, in the same sequence position, on `VoxelClient`/
  `VoxelServer` (BlockId alignment for replication).
- New `LCU_LOG_INFO` biome name at the spawn-load log line (`"...
  biome=Plains)..."`) - real, observable confirmation the climate
  stage actually ran and produced something concrete for that column,
  not just a log line claiming it does.
- 2 new worldgen unit tests (`BiomeAtIsDeterministic`,
  `BiomeAtProducesAllThreeCategoriesOverARealArea` - a real sweep
  confirming all three bands genuinely occur, not just the default);
  5 existing tests rewritten to be biome-aware (computing the expected
  surface/subsurface block from the actual biome at each test
  coordinate via `biome_at`, rather than assuming Plains).
- Verified via a real `LCU_BUILD_SHADER_TOOLS=ON` run (spawn column
  (-84,-84), `biome=Plains`, confirmed by breaking the spawn block and
  picking up `game:grass`), real `LCU_VERIFY_BREAK_PLACE`/
  `LCU_VERIFY_TORCH`/`LCU_VERIFY_CRAFT` runs (byte-identical behavior),
  and a real two-process networked run with matching independently-
  computed spawn columns, zero warnings/errors.
- `ctest` 396/396 (bgfx, up from 394) / 393/393 (non-bgfx, up from
  391).
- Honestly scoped: **what sand/snow/biome transitions actually look
  like on a real GPU/display is still NOT VERIFIED — ENVIRONMENT
  LIMITATION**; a temperature-only climate model, no humidity axis, no
  Whittaker-diagram-style biome table (three real, distinct categories,
  not the full variety a shipped game would eventually want - see
  DECISIONS.md); no elevation-climate coupling (a highland column can
  be Desert just as easily as a lowland one - real mountains are
  colder at altitude in reality, not modeled here); no item mapping
  for sand/snow yet (breaking either currently removes it without
  granting an item, the same state grass/dirt were in before Phase
  18/22 added theirs); no caves/ores/structures/vegetation stages yet
  (Phases 40-41).

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
