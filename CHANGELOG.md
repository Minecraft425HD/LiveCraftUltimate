# Changelog

All notable changes to this project are recorded here, newest first.

## Unreleased — Phase 0 / Phase 1

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
