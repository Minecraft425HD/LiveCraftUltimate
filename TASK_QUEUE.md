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
- [ ] Input abstraction (engine/platform or new engine/input): actions (MoveForward, Jump, ...) decoupled from raw keys; keyboard/mouse backend first.
- [ ] Debug overlay skeleton (FPS/frame time) per section 60 — minimal text output is enough to start.

## Phase 2 — Voxel storage + chunk + meshing + rendering

- [ ] engine/voxel: chunk storage (16x16x16 default, configurable), compact block state encoding.
- [ ] BlockRegistry (engine/modding or engine/voxel — decide and record in DECISIONS.md when reached).
- [ ] Greedy meshing, opaque/transparent/water layers, hidden-face removal.
- [ ] Job system (engine/jobs) — chunk generation/meshing off the main thread. Needed before meshing can be "done" per the brief (section 18).
- [ ] Wire meshes into engine/rendering -> bgfx.

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

Next task to pick up: Phase 1 input abstraction (action-based, not raw
key checks in gameplay code) — keyboard/mouse backend first, per brief
section 27. After that: debug overlay skeleton (FPS/frame time text
output), then Phase 2 voxel storage.

Also outstanding from Phase 1, lower priority than the above: confirm the
bgfx build on a machine/CI runner with a real display and GPU (Vulkan or
GL), since this sandbox can only verify the headless Noop path.
