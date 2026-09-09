# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, in that
order, before touching code. The repository is the source of truth, not
this file's prose if the two disagree — if in doubt, run the build and
tests and trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

Phase 0 complete. Phase 1 (SDL3 + bgfx + window + game loop + input) in
progress: window, event loop, and bgfx rendering bootstrap are done and
verified headlessly; input abstraction and debug overlay are not started.

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: input abstraction
(`MoveForward`/`Jump`/... actions decoupled from raw keys, brief section
27), keyboard/mouse backend first.

## Last Completed Task

Wired `engine/rendering::Renderer` (bgfx init/frame/shutdown) and
`engine/platform::get_native_window_handle` (X11/Wayland/Win32/Cocoa/
UIKit/Android native handle extraction from the SDL3 window) into
`VoxelClient`. Verified end-to-end under `SDL_VIDEODRIVER=dummy`: bgfx
initializes on the `Noop` backend (no native handle available headlessly),
runs a 5-frame clear loop, shuts down cleanly. `ctest` still 12/12 passing
with `LCU_ENABLE_BGFX=ON`. Required installing
`libgl1-mesa-dev`/`libglu1-mesa-dev`/`mesa-common-dev`/`libwayland-dev` in
this sandbox for bgfx.cmake's configure/link to succeed — documented in
`BUILDING.md` and `DECISIONS.md`.

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx` (or `build/dev-nobgfx`): 12/12 passing
(Log, Vec3, Mat4 unit tests). No integration tests yet (no networking/save
system exists yet to integration-test).

## Known Bugs

None currently tracked.

## Known Limitations

- `VoxelClient` opens a window and clears a frame via bgfx but draws no
  geometry yet — nothing to mesh/render until Phase 2 (voxel storage).
- No voxel data, no world, no gameplay of any kind yet — intentionally
  still pre-vertical-slice (see `ROADMAP.md` "Vertical slice targets").
- `VoxelServer`'s tick loop is a placeholder (sleeps at 20 TPS, no actual
  simulation) until Phase 7.
- bgfx's real GPU backend (Vulkan/GL/Metal/D3D) selection is untested —
  only the `Noop` headless fallback has been exercised, since this sandbox
  has no GPU/display.
- Mobile/Windows/macOS builds are untested from this Linux-only sandbox;
  `CMakePresets.json` presets exist for them but have not been exercised
  on their native toolchains.
- No input abstraction yet — the game loop doesn't read input at all yet,
  so there's nothing to abstract prematurely.

## Next Task

1. Input abstraction: define actions (`MoveForward`, `Jump`, `Interact`,
   ...) in a new `engine/platform` (or `engine/input`, decide + record in
   `DECISIONS.md` when starting) header, backed by SDL3 keyboard/mouse
   first. Gamepad/touch backends come later (brief sections 27-28) — don't
   build them speculatively now.
2. Minimal debug overlay (FPS/frame time as text output is enough to
   start, brief section 60).
3. Update state docs and commit after each, same as every prior step.
4. Then Phase 2: `engine/voxel` chunk storage.

## Current Architecture

See `ARCHITECTURE.md`. Layering is GAME -> VOXEL ENGINE ->
RENDERING ABSTRACTION -> BGFX -> platform backend. Server links only the
headless `Lcu::EngineCore` + `Lcu::Game`, never `Lcu::Engine`/
`Lcu::Platform` (SDL) or bgfx — enforced by the CMake target graph,
verified via `ldd`. `engine/rendering::Renderer` is the only place besides
`engine/platform` allowed to include bgfx/SDL headers.

## Important Decisions

See `DECISIONS.md` for full rationale, including two added this session:
the exact system packages bgfx.cmake needs on Linux, and the
`Init::swapChain`-based bgfx API shape at our pinned tag (differs from
older bgfx examples/tutorials — don't "fix" it back). Headlines: bgfx for
rendering, SDL3 for windowing/input, CMake FetchContent pinned to exact
tags for all dependencies, own minimal math library instead of GLM,
server built as a genuinely separate target with no GPU/window
dependency.
