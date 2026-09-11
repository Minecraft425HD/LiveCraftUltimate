# Building

## Prerequisites (Linux, verified in this repo's dev sandbox)

- CMake >= 3.24, Ninja, a C++20 compiler (GCC >= 11 or Clang >= 14 tested:
  this sandbox uses GCC 13.3.0 / Clang 18.1.3).
- For `LCU_BUILD_CLIENT=ON` with `LCU_ENABLE_BGFX=ON` (the default): OpenGL
  and Wayland **development** packages, even though bgfx may select
  Vulkan at runtime — its CMake build still probes for these at configure
  and link time.

  ```sh
  sudo apt-get install -y libgl1-mesa-dev libglu1-mesa-dev mesa-common-dev libwayland-dev
  ```

  See `DECISIONS.md` ("bgfx.cmake build environment requirements") for
  why. Without these, configure fails at `find_package(OpenGL)` inside
  bgfx.cmake, or the final client link fails with
  `cannot find -lwayland-egl`.
- Outbound `git clone` access to `github.com` (dependencies are fetched
  via CMake `FetchContent`, see `third_party/README.md`). Nothing is
  vendored.

## Configure + build

```sh
cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Development
cmake --build build/dev -j$(nproc)
ctest --test-dir build/dev --output-on-failure
```

Or via a preset (see `CMakePresets.json`): `cmake --preset linux && cmake --build --preset linux`.

### Useful options

| Option | Default | Effect |
|---|---|---|
| `LCU_BUILD_CLIENT` | ON | Build `VoxelClient` (needs SDL3, optionally bgfx). |
| `LCU_BUILD_SERVER` | ON | Build `VoxelServer` (headless, no SDL/bgfx ever). |
| `LCU_BUILD_TESTS` | ON | Build `VoxelTests` (GoogleTest) and register with ctest. |
| `LCU_BUILD_TOOLS` | OFF | Build `tools/` executables once they exist. |
| `LCU_ENABLE_BGFX` | ON | Fetch+build bgfx and link it into the client. Turning this OFF gives a much faster build with a window that opens but renders nothing — useful when iterating on non-rendering code. |
| `LCU_BUILD_SHADER_TOOLS` | OFF | Build bgfx's `shaderc` and compile `client/shaders/*.sc` into real GPU shader binaries, needed for `VoxelClient` to actually submit a draw call. Pulls in glslang/SPIRV-Tools/SPIRV-Cross/Dawn-Tint — a meaningfully bigger build (~700 extra steps) than bgfx alone, so it's opt-in. Without it, `VoxelClient` still runs (uploads chunk mesh GPU buffers, clears frames) but has no shader program to draw with — logs `Chunk shader program valid=false`. |

`LCU_BUILD_CLIENT=OFF -DLCU_BUILD_SERVER=ON` (or the `linux-server-only`
preset) builds only the dedicated server, with zero SDL/bgfx dependency —
useful for a server-only deployment image.

## Running headlessly (no display/GPU, e.g. CI or this sandbox)

```sh
SDL_VIDEODRIVER=dummy LCU_MAX_FRAMES=5 ./build/dev/bin/VoxelClient
LCU_MAX_TICKS=5 ./build/dev/bin/VoxelServer --world MyWorld --port 25565
```

`LCU_MAX_FRAMES`/`LCU_MAX_TICKS` bound the otherwise-infinite loops for
automated verification; real interactive runs never set them.
`SDL_VIDEODRIVER=dummy` makes SDL3 create a window with no real native
handle, which makes `engine/rendering::Renderer` fall back to bgfx's
`Noop` backend automatically (or force it explicitly with
`LCU_FORCE_HEADLESS_RENDERER=1`).

## Options file (`options.txt`)

`VoxelClient` persists key bindings, mouse sensitivity, FOV, and the
HUD/debug-overlay toggles to a plain-text `options.txt` (Phase 45) -
loaded at startup, saved whenever you leave the Options/Controls screen
or the process exits cleanly. Its path is **not** relative to the
working directory or the executable: it comes from `SDL_GetPrefPath
("LiveCraftUltimate", "LiveCraftUltimate")`, an OS-provided per-user
settings directory, printed to the log on every run (`Loaded options
from "..."` / `Saved options to "..."`). In this project's own
sandbox that resolves to:

```
~/.local/share/LiveCraftUltimate/LiveCraftUltimate/options.txt
```

On a real desktop it resolves per-OS instead - typically
`%APPDATA%\LiveCraftUltimate\LiveCraftUltimate\options.txt` on Windows
and `~/Library/Application Support/LiveCraftUltimate/LiveCraftUltimate/
options.txt` on macOS (see `SDL_GetPrefPath`'s own documentation for
the exact rule on each platform - this project has never independently
verified the Windows/macOS paths, only the Linux one, since this
sandbox has no other OS to run on). If `SDL_GetPrefPath` itself fails,
`Options::default_path` falls back to a plain `options.txt` relative to
the current working directory instead, logged as a warning. Deleting
the file (or the whole directory) is a safe, real way to reset every
persisted option back to code defaults on the next run - `VoxelClient`
handles a missing file the same as a first-time run, not an error.

## Testing under ThreadSanitizer

Concurrent code (currently `engine/jobs::JobSystem`) is additionally
verified under ThreadSanitizer, since a normal test run can pass while
still harboring a data race that only manifests under different
scheduling. Clang's TSan runtime wasn't installed in this sandbox; GCC's
was and works fine:

```sh
cmake -S . -B build/tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLCU_BUILD_CLIENT=OFF -DLCU_BUILD_SERVER=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/tsan -j$(nproc)
./build/tsan/bin/VoxelTests --gtest_filter="JobSystem.*" --gtest_repeat=50
```

`build/tsan` is not checked in (covered by `.gitignore` like every other
`build*` directory) — recreate it with the commands above whenever
touching `engine/jobs` or other concurrent code.

## Building with real shaders (a draw call, not just cleared frames)

```sh
cmake -S . -B build/dev-shaders -G Ninja -DCMAKE_BUILD_TYPE=Development -DLCU_BUILD_SHADER_TOOLS=ON
cmake --build build/dev-shaders -j$(nproc)
SDL_VIDEODRIVER=dummy LCU_MAX_FRAMES=5 ./build/dev-shaders/bin/VoxelClient
```

A successful run logs `Chunk shader program valid=true` before the
frame loop; `bgfx::submit()` is called every frame after that. This
compiles `client/shaders/{vs_chunk,fs_chunk}.sc` (see `DECISIONS.md`)
into `<binary dir>/shaders/chunk/<profile>/{vs,fs}_chunk.sc.bin` via
bgfx.cmake's `bgfx_compile_shaders()` helper, for the `spirv`
(Vulkan), `glsl` (desktop OpenGL) and `essl` (OpenGL ES) profiles -
and, automatically on a macOS host (see the macOS section below),
`metal` too. `VoxelClient` picks the profile matching
`bgfx::getRendererType()` at runtime, falling back to `glsl` for
anything else (including `Noop`, which never actually samples the
shader bytecode).

## macOS (Phase 25 — code-audited in this Linux sandbox, not yet run on a real Mac)

Everything below was verified by reading the actual CMake/FetchContent
config this repo uses (`CMakeLists.txt`, `third_party/CMakeLists.txt`,
`CMakePresets.json`, and the fetched `bgfx.cmake` package's own
`cmake/bgfxToolUtils.cmake`/`cmake/bgfx/bgfx.cmake`), not assumed -
this sandbox has no macOS host to actually run these commands on, so
the result of a real `cmake --build` here is **NOT VERIFIED —
ENVIRONMENT LIMITATION**. One real bug *was* found and fixed as part
of this audit (see below) - the rest of this section documents what
checked out clean.

**Prerequisites** (exact commands, not just package names):

```sh
xcode-select --install          # Apple Clang (C++20) + the system SDK/frameworks bgfx needs (Cocoa, Metal, QuartzCore, IOKit) - no Homebrew equivalent needed for these
brew install cmake ninja        # CMake's own minimum here is 3.24; Homebrew's is newer
```

Every other dependency (SDL3, bgfx, zstd, Lua 5.4, GoogleTest, Google
Benchmark, fmt) is fetched via CMake `FetchContent` from the exact same
`third_party/CMakeLists.txt` the Linux build uses - none of them need a
Homebrew package, and none of their CMake scripts branch into a
Linux-only path that would need one. Confirmed by reading
`bgfx.cmake`'s own macOS linking (`cmake/bgfx/bgfx.cmake`): it links
`-framework Cocoa -framework Metal -framework QuartzCore -framework
IOKit` (all part of the Xcode SDK, not Homebrew) instead of the
`libgl1-mesa-dev`/`libwayland-dev` packages the Linux build needs (see
above) - the macOS equivalent ships with Xcode CLT itself.

**Configure + build** (both paths - `CMakePresets.json`'s `macos`
preset already sets `CMAKE_OSX_ARCHITECTURES=arm64`; pass
`-DCMAKE_OSX_ARCHITECTURES=x86_64` instead, or plain `cmake -S . -B ...`
without the preset, on an Intel Mac):

```sh
# Fast iteration, no bgfx (window opens, renders nothing):
cmake --preset macos -DLCU_ENABLE_BGFX=OFF
cmake --build build/macos -j$(sysctl -n hw.ncpu)
ctest --test-dir build/macos --output-on-failure

# Real rendering, a real draw call (what the user actually wants to see):
cmake --preset macos -DLCU_ENABLE_BGFX=ON -DLCU_BUILD_SHADER_TOOLS=ON
cmake --build build/macos -j$(sysctl -n hw.ncpu)
ctest --test-dir build/macos --output-on-failure
```

**Run:**

```sh
./build/macos/bin/VoxelClient
```

**What should happen:** a real window opens (SDL3's Cocoa backend);
`engine/rendering::Renderer` selects bgfx's Metal backend (its
default/preferred choice on macOS, over the deprecated OpenGL path);
`Renderer::get_native_window_handle` already has a macOS-specific
branch (`SDL_PROP_WINDOW_COCOA_WINDOW_POINTER`, `engine/platform/src/
native_handle.cpp`) that was already correct before this phase - not
something this audit needed to add. The client should log `Chunk
shader program valid=true` and render actual frames, same log-level
proof already confirmed headlessly under bgfx's `Noop` backend on
Linux.

**Real bug found and fixed by this audit** (not merely documented -
see `DECISIONS.md` "Phase 25"): `engine/rendering::
active_shader_profile_dir()` mapped `bgfx::RendererType` to a shader
profile subdirectory for Vulkan/OpenGL/OpenGL ES, but had **no case for
Metal** - it silently fell through to the `default: return "glsl"`
branch. Since bgfx auto-selects Metal as its preferred renderer on
macOS, and `bgfx_compile_shaders()` (see above) already compiles a
real `metal` profile automatically there, a real Mac run would have
loaded the *wrong* shader binary format into a Metal renderer - not a
missing profile, a genuinely wrong one, which `bgfx::createShader`
would have rejected (the code already tolerates that: an invalid
handle just means no draw call, not a crash - `VoxelClient` would still
run and show a cleared-color window with no chunk mesh drawn). Fixed
by adding the missing `case bgfx::RendererType::Metal: return
"metal";` branch. This is a code fix confirmed correct by reading both
sides (the compile-time profile list bgfx.cmake generates for an
`APPLE` host, and the runtime profile bgfx reports via
`getRendererType()`) - actually running it on a Mac is still needed to
call the *visual result* verified, per this repo's "never trust
without verifying" discipline.

**Not verified, environment limitation, no way around it here:**
whether the window actually appears, renders correctly-colored/shaped
geometry, or performs acceptably on real Apple Silicon/Metal hardware.
Someone with a real Mac needs to run the commands above and report
back - see `BUILD_STATUS.md`'s macOS preset row.

## Platform status

See `BUILD_STATUS.md` for the up-to-date, verified-vs-untested matrix.
Windows/Android/iOS presets exist in `CMakePresets.json` but have not
been exercised from this Linux-only sandbox — they need their native
toolchain (MSVC, Android NDK, Xcode-on-macOS respectively). macOS was
code-audited (see above) but still not run on a real Mac from this
sandbox.
