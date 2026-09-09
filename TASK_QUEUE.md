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
- [x] Greedy meshing, hidden-face removal, opaque layer. `mesh_chunk_greedy<EdgeLength>()`: axis-sweep algorithm, registry-driven opacity (not hardcoded air checks), merges same-block same-facing coplanar faces. Transparent/water layers exist structurally (`ChunkMesh::transparent`/`::water`) but are always empty — no transparent block exists yet to mesh, and transparent-vs-transparent face rules are deliberately deferred (see DECISIONS.md) rather than guessed. Winding verified via a geometric cross-product check on every emitted triangle (no display available to check visually). 8 unit tests. Not yet dispatched through the job system — still called synchronously; that wiring is the next task.
- [x] Job system (engine/jobs) — `JobSystem`: worker pool, priority scheduling, dependency graphs with cascading cancellation, cancellation of not-yet-started jobs. Correctness-first (one mutex+condvar), not yet lock-free/work-stealing — revisit only if Phase 11 profiling shows it matters (see DECISIONS.md). 12 unit tests covering ordering/dependencies/cancellation/priority, plus 200 repeated runs and 50 runs under ThreadSanitizer with zero failures/races. Now has a real (if not-yet-wired-in) consumer in greedy meshing.
- [ ] Wire meshing through `JobSystem` (currently called directly/synchronously) and feed `ChunkMesh` into `engine/rendering` -> bgfx as actual GPU vertex/index buffers.

## Phase 3 — World generation + streaming + save

- [ ] Deterministic worldgen pipeline (seed -> continental -> terrain -> climate -> biome -> caves -> ores -> structures -> vegetation -> decoration). Start with continental+terrain only, extend.
- [ ] Chunk lifecycle state machine (engine/world) matching ARCHITECTURE.md.
- [ ] World streaming by player position/view direction/distance priority.
- [ ] Save/load: engine/serialization, versioned format, corruption detection (no silent overwrite).
- [ ] Add a compression library dependency (decision recorded in DECISIONS.md when this task starts).

## Phase 4 — Player + physics + interaction

- [ ] AABB + voxel collision, gravity, jump/crouch/swim/step.
- [ ] Voxel DDA raycaster.
- [ ] First-person camera, block break/place.

## Phase 5 — Items + inventory + crafting

- [ ] ItemRegistry, Inventory component, RecipeRegistry.

## Phase 6 — Entities + AI + lighting + day/night

- [ ] engine/ecs entity/component storage.
- [ ] Sunlight + block light propagation/removal (local updates, not full recompute).
- [ ] Simple AI, day/night cycle.

## Phase 7 — Networking + dedicated server

- [ ] engine/network transport (reliable/unreliable channels).
- [ ] Server-authoritative state, VoxelServer real simulation loop (replacing the Phase 0 tick-loop placeholder).

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
meshing are done and thoroughly unit tested (job system additionally
stress-tested: 200 repeated runs + 50 runs under ThreadSanitizer, zero
failures/races; meshing verified via a geometric winding check since
there's no display to check visually). Next task to pick up: dispatch
`mesh_chunk_greedy` calls **through `engine/jobs::JobSystem`** instead
of calling it synchronously (its first real consumer, not just its own
tests), then feed the resulting `ChunkMesh` into `engine/rendering` as
actual bgfx vertex/index buffers for a textured-cube-on-screen
milestone. Needs at least one real `BlockDefinition` registered
somewhere for `VoxelClient` to actually have something to mesh — a
trivial "game:stone" placeholder is enough, real content isn't the
point yet. Still only headlessly verifiable here; needs a real display
to confirm visually.

Also outstanding from Phase 1, lower priority, revisit opportunistically:
confirm the bgfx build on a machine/CI runner with a real display and
GPU (Vulkan or GL) — this sandbox can only verify the headless Noop
path.
