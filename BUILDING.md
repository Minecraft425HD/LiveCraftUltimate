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
(Vulkan), `glsl` (desktop OpenGL) and `essl` (OpenGL ES) profiles.
`VoxelClient` picks the profile matching `bgfx::getRendererType()` at
runtime, falling back to `glsl` for anything else (including `Noop`,
which never actually samples the shader bytecode).

## Platform status

See `BUILD_STATUS.md` for the up-to-date, verified-vs-untested matrix.
Windows/macOS/Android/iOS presets exist in `CMakePresets.json` but have
not been exercised from this Linux-only sandbox — they need their native
toolchain (MSVC, Xcode, Android NDK respectively).
