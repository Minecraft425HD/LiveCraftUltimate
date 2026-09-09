# Project State

Read this file first in every new session, then `TASK_QUEUE.md`,
`ROADMAP.md`, `BUILD_STATUS.md`, `ARCHITECTURE.md`, `DECISIONS.md`, in that
order, before touching code. The repository is the source of truth, not
this file's prose if the two disagree — if in doubt, run the build and
tests and trust what actually happens (see `BUILD_STATUS.md` for the exact
commands).

## Current Phase

Phase 0 complete. Phase 1 (SDL3 + bgfx + window + game loop + input)
functionally complete for what this headless sandbox can verify: window,
event loop, bgfx rendering bootstrap, action-based input, minimal FPS
debug overlay all done and tested. Mouse-look is deliberately deferred
(no camera to control yet — see `TASK_QUEUE.md`).

## Current Task

None in flight. Next up per `TASK_QUEUE.md`: **Phase 2**, starting with
`engine/voxel` chunk storage (16x16x16 default, compact block state
encoding).

## Last Completed Task

Added `engine/platform::InputState`/`KeyboardInputBackend` (action-based
input: `MoveForward`/`Jump`/`Interact`/... decoupled from raw SDL
scancodes, brief section 27) and `engine/debug::FrameStats` (minimal
FPS/frame-time accumulator, brief section 60). Wired both into
`VoxelClient`'s loop. Verified: `ctest` 18/18 passing (6 new cases), and a
real 2-second `SDL_VIDEODRIVER=dummy` run produced actual
`fps=60165.4 frame_ms=0.02 total_frames=60166` output from ~120k real
loop iterations — not a stub, an actually-executing accumulator.

## Build Status

See `BUILD_STATUS.md` for the full target-by-target table. Summary: core
engine + platform + rendering(bgfx) + client + server + tests all
**TESTED** in this Linux sandbox, headlessly (no display/GPU here — a
real Vulkan/GL backend actually presenting to a screen is **not**
verified; someone with a desktop needs to confirm that). Windows/macOS/
Android/iOS builds are **BLOCKED here** for lack of the relevant
toolchain/host, not because the CMake presets are known-broken.

## Test Status

`ctest --test-dir build/dev-bgfx` (or `build/dev-nobgfx`): 18/18 passing
(Log, Vec3, Mat4, FrameStats, InputState unit tests). No integration tests
yet (no networking/save system exists yet to integration-test).

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
- Input abstraction covers keyboard only (`KeyboardInputBackend`); no
  mouse-look, gamepad or touch backend yet — none has a consumer to drive
  until a camera/player exists (Phase 4) or mobile work starts (Phase 10).
- Debug overlay is a log line, not an on-screen overlay — needs
  `engine/ui`/text rendering (later phase) to actually draw on screen.

## Next Task

1. Phase 2: `engine/voxel` chunk storage — 16x16x16 default (brief
   section 15), configurable chunk size, compact block state encoding
   (brief section 17), no per-block C++ instance (brief section 15).
   Thoroughly unit test indexing/encoding math — this is exactly the kind
   of pure-logic work this sandbox can fully verify without a GPU.
2. BlockRegistry once there's at least one block type to register (brief
   section 16) — decide its exact home (`engine/voxel` vs
   `engine/modding`) and record in `DECISIONS.md` when starting.
3. Update state docs and commit after each step, same as every prior one.

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
