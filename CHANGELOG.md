# Changelog

All notable changes to this project are recorded here, newest first.

## Unreleased — Phase 0 / Phase 1 / Phase 2 / Phase 3 / Phase 4 / Phase 5 / Phase 6 / Phase 7 / Phase 8 / Phase 9 / Phase 10 / Phase 11 / Phase 12 / Phase 13 / Phase 14 / Phase 15 / Phase 16 / Phase 17 / Phase 18 / Phase 19 / Phase 20 / Phase 21 / Phase 22 / Phase 23 / Phase 24 / Phase 25

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
