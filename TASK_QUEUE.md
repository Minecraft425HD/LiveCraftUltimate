# Task Queue

`[ ]` TODO   `[~]` IN PROGRESS   `[x]` COMPLETE   `[!]` BLOCKED

A task is never marked `[x]` unless it was actually built and, where it
produces a runnable artifact, actually run and observed to behave
correctly in this environment. See `BUILD_STATUS.md` for exactly what was
verified and how.

## Phase 0 — Repository + build system + state system

- [x] Create engine/game/client/server/tools/third_party/mods/examples/tests directory structure.
- [x] Write PROJECT_STATE.md, ROADMAP.md, TASK_QUEUE.md, BUILD_STATUS.md, ARCHITECTURE.md, DECISIONS.md, CHANGELOG.md.
- [x] Root CMakeLists.txt with LCU_BUILD_* options, CMakePresets.json for linux/windows/macos/android/ios.
- [x] third_party/CMakeLists.txt: FetchContent for fmt, SDL3, GoogleTest, bgfx.cmake (bx/bimg/bgfx); third_party/README.md dependency table.
- [x] engine/core: types.h, log.h/.cpp (fmt-backed), assert.h/.cpp. Unit tested.
- [x] engine/math: Vec3, Vec4, Mat4 (translation/scale/perspective/look_at). Unit tested.
- [x] Verify full non-bgfx build + ctest + run VoxelClient (headless) + run VoxelServer + confirm VoxelServer has zero SDL/bgfx link dependency via ldd.

## Phase 1 — SDL3 + bgfx + window + game loop + input

- [x] engine/platform: Window (SDL3-backed), event pump, resize handling. Unit-of-work tested via VoxelClient headless run.
- [x] VoxelClient: opens window, runs loop, clean shutdown. Verified headless (SDL_VIDEODRIVER=dummy); **not** verified with a real display/GPU (none available in this sandbox) — needs confirmation on a machine with a display.
- [x] bgfx integration: fetch + build validation of the bgfx.cmake wrapper. Required installing libgl1-mesa-dev/libglu1-mesa-dev/mesa-common-dev/libwayland-dev in this sandbox (see DECISIONS.md); documented in BUILDING.md for other environments.
- [x] engine/rendering: bgfx init against the SDL3 native window handle (engine/platform::get_native_window_handle, X11/Wayland/Win32/Cocoa/UIKit/Android branches), first cleared frame. Verified headless: falls back to bgfx::RendererType::Noop when no native handle is available (dummy SDL driver) and completes a 5-frame clear/frame loop cleanly. **Not** verified: real Vulkan/GL backend actually presenting on a real display — no GPU/display in this sandbox.
- [x] Input abstraction (engine/platform): Action enum (MoveForward/Jump/Interact/...) + InputState + KeyboardInputBackend (SDL_GetKeyboardState-based). Gamepad/touch backends deferred to Phase 10 (brief sections 27-28) — not built speculatively now. Unit tested (InputState set/is_down); KeyboardInputBackend itself untested by unit test (needs a live SDL keyboard state) but exercised every VoxelClient run.
- [x] Debug overlay skeleton (engine/debug::FrameStats): FPS/avg frame time text line, reported once per second. Pure accumulator, no SDL/bgfx dependency (reusable by VoxelServer for tick-rate reporting later). Unit tested; also verified via a real 2-second VoxelClient run producing actual fps=60165.4 frame_ms=0.02 output. CPU/GPU/RAM/chunks/entities/ping/bandwidth/draw-calls/jobs lines from brief section 60 are added once the systems producing those numbers exist — not stubbed out now.

## Phase 2 — Voxel storage + chunk + meshing + rendering

- [x] engine/voxel: chunk storage. `ChunkStorage<EdgeLength>` template (default 16x16x16 via `Chunk = ChunkStorage<16>`), flat contiguous `BlockId` (u16) array, no per-block C++ instance. Alternative chunk sizes proven via a unit test with `ChunkStorage<8>`. Block *state* (rotation/orientation/powered/etc., brief section 17) is not encoded yet — `BlockId` alone for now; state packing is added once a block that needs it exists (e.g. a directional block in the example mod, Phase 9), not speculatively.
- [x] engine/voxel: `world_to_chunk_and_local()` — correct floor-division coordinate splitting (`ChunkCoord` + `LocalBlockCoord`), the ARCHITECTURE.md "Coordinate spaces" piece needed before world storage/streaming can be built.
- [x] BlockRegistry (landed in `engine/voxel` — recorded in DECISIONS.md; revisit only if `engine/modding`'s registry needs pull it elsewhere later). Namespaced ids (`game:stone`), air always id 0, datadriven `BlockDefinition` (hardness/transparency/collision/light_emission). Block *tags* and full mod-facing registration API are Phase 9 work.
- [x] Greedy meshing, hidden-face removal, opaque layer. `mesh_chunk_greedy<EdgeLength>()`: axis-sweep algorithm, registry-driven opacity (not hardcoded air checks), merges same-block same-facing coplanar faces. Transparent/water layers exist structurally (`ChunkMesh::transparent`/`::water`) but are always empty — no transparent block exists yet to mesh, and transparent-vs-transparent face rules are deliberately deferred (see DECISIONS.md) rather than guessed. Winding verified via a geometric cross-product check on every emitted triangle (no display available to check visually). 8 unit tests.
- [x] Job system (engine/jobs) — `JobSystem`: worker pool, priority scheduling, dependency graphs with cascading cancellation, cancellation of not-yet-started jobs. Correctness-first (one mutex+condvar), not yet lock-free/work-stealing — revisit only if Phase 11 profiling shows it matters (see DECISIONS.md). 12 unit tests covering ordering/dependencies/cancellation/priority, plus 200 repeated runs and 50 runs under ThreadSanitizer with zero failures/races. Now has a real consumer: `VoxelClient` dispatches `mesh_chunk_greedy` through it.
- [x] Wire meshing through `JobSystem` and feed `ChunkMesh` into `engine/rendering` -> bgfx as actual GPU vertex/index buffers. `upload_chunk_mesh_layer`/`destroy_gpu_chunk_mesh`, 3 unit tests, and a real end-to-end `VoxelClient` run (256-block slab -> job-dispatched mesh -> 6 merged quads -> real bgfx buffer handles, `valid=true index_count=36`).
- [x] Real draw call. `client/shaders/{vs_chunk,fs_chunk}.sc` (minimal directional+ambient lighting, no texturing - no atlas yet), compiled via bgfx's `shaderc` (opt-in `LCU_BUILD_SHADER_TOOLS`, see DECISIONS.md), loaded at runtime (`engine/rendering::load_chunk_program`, profile selected by active bgfx renderer type), submitted every frame via `Renderer::submit_chunk_mesh()`. Verified: `"Chunk shader program valid=true"` and 3 clean frames with the draw call actually executing, under bgfx's `Noop` backend. **What's not verified**: what it looks like on a real GPU/display - none exists in this sandbox.

## Phase 3 — World generation + streaming + save

- [x] Deterministic worldgen pipeline: continental+terrain stages only (`engine/world::worldgen::terrain_height`, seeded 4-octave value noise; same seed+coord always same height, different seeds differ, adjacent columns smooth not random - all three properties unit tested). Climate/biome/caves/ores/structures/vegetation/decoration deferred - no biome/structure types registered anywhere to drive them yet (see DECISIONS.md).
- [x] Chunk lifecycle state machine (`engine/world::World`, `ChunkLifecycleState`) matching ARCHITECTURE.md: Unloaded -> Requested -> Generating -> Generated driven directly; Lighting/Meshing/GpuUpload/Ready/Visible states declared but not yet driven by World itself (Lighting is Phase 6; meshing/GPU upload already exist in `engine/voxel`/`engine/rendering` but aren't wired through World's state machine yet - that's client-side bookkeeping once more than one chunk streams).
- [x] World streaming by distance (`World::update_streaming`, Chebyshev radius with load/unload hysteresis). **Not yet** by view direction/movement direction/player position specifically - no camera/player exists yet to supply those signals (Phase 4); known simplification recorded in DECISIONS.md (streams a 3D cube, not a horizontal disc).
- [x] Save/load: `engine/serialization::chunk_serializer`, versioned format (`kChunkFormatVersion`), corruption detection via zstd's content checksum, version-mismatch detection - all with dedicated tests, not just a happy-path round-trip. No silent overwrite: a failed load leaves the caller's chunk untouched (tested).
- [x] Compression library: zstd (decision + rationale in DECISIONS.md, dependency table in third_party/README.md).

## Phase 4 — Player + physics + interaction

- [x] AABB + voxel collision, gravity, jump/crouch/swim/step (`engine/physics::move_and_collide`/`integrate_player`, 18 unit tests, including a hand-verified auto-step regression and a dedicated-ground-probe grounding fix - see DECISIONS.md).
- [x] Voxel DDA raycaster (`engine/physics::raycast`, Amanatides & Woo, 9 unit tests incl. a hand-computed exact-distance case).
- [x] First-person camera, block break/place (`engine/player::FirstPersonCamera`/`movement_direction_from_input`, 12 unit tests; `VoxelClient` now builds a multi-chunk `World`, spawns a physics-driven player on the terrain surface, and wires raycast hits into `World::chunk_at_mutable()` for edge-detected break/place, remeshing the affected chunk plus any chunk sharing the mutated boundary - verified end-to-end with a real headless run, not a mock).

## Phase 5 — Items + inventory + crafting

- [x] `engine/items::ItemRegistry`/`ItemDefinition` - namespaced, datadriven, mirrors `BlockRegistry` (`kNoItemId` reserved like `kAirBlockId`). 5 unit tests.
- [x] `engine/items::Inventory` - slot-based `ItemStack` storage, `add_item`/`remove_item`/`count_item`, respects each item's `max_stack_size`. 10 unit tests incl. partial-stack top-up before spilling into a new slot, and leftover-on-full behavior.
- [x] `engine/items::RecipeRegistry` - shaped (bounding-box-trimmed, exact orientation) and shapeless (ingredient-multiset) recipe matching. 9 unit tests. No crafting-UI caller yet (tested standalone, same as `BlockRegistry`/`ItemRegistry` were before their first real callers existed).
- [x] `VoxelClient`: breaking a block now hands the player a real `game:stone` item via `Inventory::add_item` (block-break's first item consumer); placing a block now consumes one from the inventory via `Inventory::remove_item`, refunding it if the target chunk turns out not to be loaded. Verified end-to-end via the existing `LCU_VERIFY_BREAK_PLACE` headless hook: break picks up 1 stone (inventory: 1), place consumes it (inventory: 0).

## Phase 6 — Entities + AI + lighting + day/night

- [x] `engine/ecs::Registry` - generation-checked `EntityId` handles, sparse-set `ComponentPool<T>` per component type (dense contiguous storage, swap-and-pop removal). `create`/`destroy_entity`, `add`/`get`/`has`/`remove_component`, `pool_for<T>()` for dense iteration. No query DSL/archetypes/multithreaded dispatch - not needed yet. 13 unit tests.
- [x] Sunlight + block light propagation/removal. `engine/lighting::LightStorage` (packed 4-bit sky + 4-bit block light per voxel). `compute_block_light`/`compute_sky_light` for the initial per-chunk flood; `propagate_added_block_light`/`unpropagate_block_light` for true incremental local updates on a single block add/remove (the standard two-phase BFS removal algorithm, not a full recompute per edit) - see DECISIONS.md for the single-chunk-scope simplification. 12 unit tests, incl. an exact-match check between incremental and full-recompute paths and a two-source removal/refill test.
- [x] Simple AI, day/night cycle. `game::components::{Position, AIWander}` + `game::systems::update_ai_wander` (idle/walk-to-target/pick-new-target loop, explicit `std::mt19937` for determinism); `game::systems::DayNightCycle` (cosine sky-light-scale curve, full at noon, dim floor at midnight). 13 unit tests.
- [x] `VoxelClient`: spawns 3 wandering AI entities and a `DayNightCycle`, both updated every frame; computes real per-chunk block+sky light at load and keeps it correct through every break/place edit via the incremental primitives (not a full recompute), plus a per-column sky light refresh. Also fixed a real off-by-one found while verifying this - the player (and AI) previously spawned embedded one block into the ground due to misreading `terrain_height()`'s solid/air boundary; now spawns resting on top of it, as documented.

## Phase 7 — Networking + dedicated server

- [x] `engine/network` transport - all four channels ARCHITECTURE.md commits to (`UnreliableUnordered`, `UnreliableSequenced`, `ReliableUnordered`, `ReliableOrdered`) over a hand-rolled ack/retransmit protocol on UDP (`UdpSocket`, `Connection`, `PacketHeader`, `sequence_greater_than`). See `NETWORKING.md` for the wire format. 46 unit tests, including two full loopback integration tests over real sockets (one deliberately drops a real datagram and verifies retransmission recovers it).
- [x] Server-authoritative state, `VoxelServer` real simulation loop. Replaced the Phase 0 sleep-only placeholder: `VoxelServer` now generates/loads a real `World`, runs the same wandering-AI simulation (`engine/ecs` + `game::systems::update_ai_wander`) as `VoxelClient`, and listens for real UDP connections - a peer is "connected" on its first datagram, gets a real `ReliableOrdered` Welcome (world seed + tick rate), and receives a per-tick `UnreliableSequenced` Heartbeat. Verified via a real two-process test: a standalone Python UDP client connects to a running `VoxelServer` and receives the real handshake + live heartbeats. `ldd` reconfirmed zero SDL/bgfx dependency.

## Phase 8 — Replication + prediction + interpolation

- [ ] Client-side prediction + reconciliation, remote entity interpolation, interest management, chunk network streaming + compression.

## Phase 9 — Modding + registries + Lua + events

- [ ] Add Lua dependency (decision recorded when this starts).
- [ ] Registries (Block/Item/Entity/Biome/Recipe/Structure/Sound/Command), namespaced IDs.
- [ ] Event system, mod loader, example_mod per brief section 91.

## Phase 10 — Mobile + touch + Android + iOS

- [ ] Real Android Gradle/NDK project structure, real iOS Xcode project generation. Only after CMakePresets.json android-arm64/ios presets have been exercised on an actual toolchain (this sandbox cannot; needs CI or a dev machine).
- [ ] Touch input mapped through the same input-action abstraction as desktop.
- [ ] Quality profiles (MOBILE_LOW/MEDIUM/HIGH).

## Phase 11 — Optimization + profiling

- [ ] Benchmarks (tools/benchmark) for voxel access, chunk gen, meshing, lighting, physics, serialization, compression, network, entity sim.

## Phase 12 — UI + audio + content + polish

- [ ] SDL3 audio backend, positional audio.
- [ ] UI system usable from desktop/gamepad/touch.

---

Phase 1 is functionally complete for what a headless sandbox can verify:
window, event loop, bgfx rendering bootstrap, action-based input, minimal
debug overlay. Mouse-look (camera control) is intentionally not built yet
— there is no camera/player entity until Phase 2-4 give it something to
control, so a mouse-delta API would have no consumer yet (brief section
98, no overengineering ahead of need).

Phase 2 chunk storage/coordinates/BlockRegistry/job system/greedy
meshing/GPU upload are all done and verified end-to-end: `VoxelClient`
registers a placeholder "game:stone" block, builds a flat ground slab
`Chunk`, dispatches `mesh_chunk_greedy` through `engine/jobs::JobSystem`,
and uploads the result into real bgfx vertex/index buffers (verified
headlessly via the `Noop` backend - confirmed `valid=true index_count=36`
from a real run, not a mock).

A real draw call is now working: `LCU_BUILD_SHADER_TOOLS=ON` builds
bgfx's `shaderc` (confirmed buildable: glslang/SPIRV-Tools/SPIRV-Cross/
Dawn-Tint, ~700 extra build steps, see DECISIONS.md), compiles
`client/shaders/{vs_chunk,fs_chunk}.sc` into spirv/glsl/essl binaries via
`bgfx_compile_shaders()`, and `VoxelClient` loads them into a real
`bgfx::ProgramHandle` and calls `Renderer::submit_chunk_mesh()` ->
`bgfx::submit()` every frame. Verified via a real run: `"Chunk shader
program valid=true"` followed by 3 clean frames with the draw call
actually submitted, under bgfx's `Noop` backend (no GPU/display here).

Phase 2 and Phase 3 are both functionally complete for what this
sandbox can verify: chunk storage, meshing, GPU upload, real draw
calls, multi-chunk `World` with streaming, deterministic terrain
generation, and versioned/corruption-checked save/load are all done
and tested (see the phase sections above for specifics).

Phase 4 is now functionally complete for what this sandbox can verify:
voxel DDA raycasting, AABB collision/player physics (gravity, jump,
auto-step, a dedicated ground probe fixing a real grounding-detection
bug found before it ever shipped), and a first-person camera are all
implemented and unit tested. `VoxelClient` was rewritten from its
Phase 2 single-placeholder-chunk approach to load a real multi-chunk
`World` (36 chunks around spawn), spawn a physics-driven player resting
on the generated terrain surface, drive the camera from arrow-key look
input and WASD movement, raycast every frame for block selection, and
mutate the world on edge-detected Interact (break)/PlaceBlock (place)
presses - remeshing and re-uploading the affected chunk (plus any
neighbor chunk sharing the mutated block's boundary, so cross-chunk
face culling stays correct). Verified via a real headless run with a
synthetic input hook (`LCU_VERIFY_BREAK_PLACE`, see DECISIONS.md): a
block is broken, logged, then the next frame's raycast (now reaching
one block deeper) is used to place a new block back at the exact same
world position - a real round-trip through the mutate/remesh/reupload
pipeline, not a mock of it. This closes brief section 80's slice 1
vertical slice (save/load already existed from Phase 3, just not yet
wired to any trigger in `VoxelClient` - see Known Limitations).

Phase 5 is now functionally complete for what this sandbox can verify:
`engine/items::{ItemRegistry, Inventory, RecipeRegistry}` are all
implemented and unit tested, and `VoxelClient`'s break/place loop now
runs on a real item economy - breaking a block adds a `game:stone` item
to a 9-slot player `Inventory`, and placing one consumes it back out
(refunded if the placement target turns out to be unloaded). Verified
via the same `LCU_VERIFY_BREAK_PLACE` headless hook used for Phase 4:
"Picked up 1 game:stone (inventory: 1)" then "Placing block ...
(inventory: 0)".

Phase 6 is now functionally complete for what this sandbox can verify:
`engine/ecs::Registry`, `engine/lighting::{LightStorage, propagation}`,
`game::systems::{update_ai_wander, DayNightCycle}` are all implemented
and unit tested, and `VoxelClient` now loads real per-chunk lighting,
keeps it correct incrementally through every block edit, and runs 3
wandering AI entities plus a ticking day/night cycle every frame.
Verified via a real headless run: "Sky light 5 blocks above spawn
column: 15", AI entities logging real (deterministic, seeded) positions,
and "Day/night: time_of_day=0.000 sky_light_scale=0.550" alongside the
still-passing `LCU_VERIFY_BREAK_PLACE` round-trip.

Phase 7 is now functionally complete for what this sandbox can verify:
`engine/network` implements all four committed channel semantics over a
hand-rolled ack/retransmit UDP protocol (see `NETWORKING.md` for the
wire format), and `VoxelServer` runs a real `World` + AI simulation and
a real UDP handshake instead of the Phase 0 sleep-only placeholder.
Verified via 46 new unit/integration tests (including real loopback
sockets with deliberately simulated packet loss) and a real two-process
run: a standalone Python UDP client received a genuine Welcome message
and live Heartbeats from a running `VoxelServer`.

Next task to pick up: **Phase 8 — Replication + prediction +
interpolation.** Client-side prediction + reconciliation, remote entity
interpolation, interest management, chunk network streaming +
compression. This is the phase that finally connects `VoxelClient` to
`VoxelServer` over the transport Phase 7 built - `VoxelClient` still
runs entirely single-player/local today, with no network code of its
own at all.

Known simplifications carried forward, still accurate and still
acceptable until something needs more: `RecipeRegistry` has no
crafting-UI caller; item drops are a direct 1:1 block->item mapping, not
a loot-table system; lighting is single-chunk scoped (no cross-chunk
bleed); `World::update_streaming` still isn't called by `VoxelClient`
(a static area is loaded once at startup); `engine/network`'s reliable
channel has no RTT estimation/congestion control, and its server
connection model has no authentication - see
NETWORKING.md/DECISIONS.md/PROJECT_STATE.md for each.

**Not done, and out of scope for this sandbox regardless of what's
built next**: confirming what any of this actually looks like on a real
GPU/display, since none exists here. Every claim about rendering,
camera behavior, etc. is about the logic/API being correct, not about
visual appearance.

Also outstanding from Phase 1, lower priority, revisit opportunistically:
confirm the bgfx build on a machine/CI runner with a real display and
GPU (Vulkan or GL) — this sandbox can only verify the headless Noop
path.
