# Decisions

Record of non-obvious technical decisions, in chronological order. Each
entry: context, decision, rationale, alternatives considered.

## 2026-09-09 — Rendering abstraction: bgfx

**Context:** Need one rendering path across Windows/Linux/macOS/Android/iOS
without hand-writing five backends (D3D12, Vulkan, Metal, GL/GLES).

**Decision:** Use bgfx as the sole GPU abstraction. Game/engine code talks
to `engine/rendering`, never directly to a graphics API, except where an
undocumented exception is later added here.

**Alternatives considered:** raw Vulkan-everywhere (rejected: no Metal
support, big maintenance burden on Apple platforms), Diligent Engine
(viable alternative, heavier and less battle-tested for mobile), five
hand-rolled backends (rejected outright by the project brief — too much
surface to maintain correctly).

## 2026-09-09 — Windowing/input: SDL3

**Decision:** SDL3 owns window creation, keyboard/mouse/touch/gamepad
input, and platform bootstrapping on Windows/Linux/macOS/iOS/Android.
bgfx is initialized against the native window handle SDL3 provides
(`SDL_GetWindowSizeInPixels` + platform-specific handle via
`SDL_GetPointerProperty` / `SDL_PROP_WINDOW_*` properties in SDL3).

**Alternatives considered:** GLFW (no mobile support — rejected), writing
our own platform layer (rejected — SDL3 already solves this correctly and
is BSD-licensed, effectively zero cost to depend on).

## 2026-09-09 — Dependency fetch strategy: CMake FetchContent, pinned tags

**Decision:** All third-party dependencies are pulled via
`FetchContent_Declare(... GIT_TAG <pinned commit or release tag>)` from
`third_party/CMakeLists.txt`, never vendored as committed source, never
fetched by floating branch. Verified from this sandbox that outbound
`https://github.com/...` git clones succeed (`api.github.com` REST calls
are blocked by the network policy here, but `git` over https and
`raw.githubusercontent.com` are reachable).

**Rationale:** Reproducible builds, easy version bumps, avoids repo bloat.
Documented per section 66/12 of the project brief: version, license,
source and build method tracked in `third_party/README.md`.

## 2026-09-09 — bgfx CMake integration: bkaradzic/bgfx.cmake wrapper

**Context:** Upstream bgfx/bx/bimg build via GENie/premake, not native
CMake.

**Decision:** Use the community-maintained `bkaradzic/bgfx.cmake` wrapper,
which vendors CMake build rules for bx/bimg/bgfx and is the de facto
standard way to consume bgfx from a CMake superproject.

**Risk noted:** This is a third-party CMake wrapper, not upstream-official.
Pin to an exact commit and verify build output before relying on it in
CI/release builds; re-evaluate if the wrapper lags upstream bgfx
significantly.

## 2026-09-09 — Server has zero GPU/window dependency

**Decision:** `VoxelEngine` is built twice conceptually: the headless
subset used by `VoxelServer` excludes `engine/platform` (SDL3) and
`engine/rendering` (bgfx) entirely via CMake target composition — the
server links a `VoxelEngineCore` object library that never references
those directories, rather than linking `VoxelEngine` and hoping unused
symbols get stripped. This is enforced by the server target simply not
depending on those subdirectories' targets, not by preprocessor guards.

## 2026-09-09 — Own math library, no GLM dependency

**Decision:** `engine/math` implements the minimal set of vector/matrix
primitives needed (float3, float4, mat4, quaternion) rather than adding
GLM or another math dependency. The project brief's dependency list
(bgfx, SDL3, Lua, GoogleTest/Catch2, fmt/spdlog, compression, physics
"only if required") does not include a math library, and GLM is a single
well-scoped enough problem to own directly and keep dependency count down
per section 66.

## 2026-09-09 — bgfx.cmake build environment requirements (Linux sandbox)

**Context:** First attempt to configure `LCU_ENABLE_BGFX=ON` failed CMake's
`find_package(OpenGL)` check inside bgfx.cmake (missing dev headers), and
after that was fixed, the final `VoxelClient` link failed with
`cannot find -lwayland-egl`.

**Decision/action:** Installed `libgl1-mesa-dev`, `libglu1-mesa-dev`,
`mesa-common-dev` (provides OpenGL dev headers + `libGL.so` symlink for
`FindOpenGL`) and `libwayland-dev` (provides the `libwayland-egl.so`
unversioned symlink the linker needs; the runtime `.so.1` was already
present as a transitive dependency) via `apt-get` in this sandbox. These
are build-time/link-time only — bgfx still picks its actual runtime
backend (Vulkan/GL/Noop) at `bgfx::init` time, this doesn't force GL.

**Follow-up for other environments/CI:** any Linux build host building the
client with `LCU_ENABLE_BGFX=ON` needs the same OpenGL + Wayland dev
packages installed, even if the target renderer is Vulkan — bgfx's CMake
still probes for GL/Wayland as part of its multi-backend build. Document
this in a future `BUILDING.md` rather than assuming a bare toolchain image
suffices.

## 2026-09-09 — bgfx API surface at the pinned version (v1.159.9485-575)

**Context:** Wrote `engine/rendering/Renderer` against the bgfx API
documented in older tutorials/examples (`bgfx::Init::resolution`,
`bgfx::Init::platformData.nwh/ndt`, `bgfx::reset(width, height, flags)`).
The pinned bgfx tag has since refactored multi-window support: the native
window handle, display type, and swap-chain width/height now live under
`bgfx::Init::swapChain` (a `bgfx::SwapChain`), not a flat
`resolution`/`platformData` pair; `bgfx::PlatformData` only carries
`context`/`queue`/`type` now; and `bgfx::reset()` takes `(flags,
swapChain*)`, not `(width, height, flags)`.

**Decision:** Adapted `Renderer::init`/`resize` to the current
`Init::swapChain` shape rather than pinning an older bgfx tag to match
outdated examples — the newer API is the one actually shipped at our
pinned commit and multi-window support is directly useful for Phase 1
tooling (debug windows, etc.) later. Recorded here so nobody "fixes" this
back to the older shape while copying an outdated bgfx example.

**Verification:** `VoxelClient` built with `LCU_ENABLE_BGFX=ON`, run under
`SDL_VIDEODRIVER=dummy` (no native window handle available -> `Renderer`
falls back to `bgfx::RendererType::Noop` automatically) completes 5 frames
of `bgfx::init` -> `setViewClear`/`setViewRect`/`touch`/`frame` ->
`bgfx::shutdown` cleanly. Real GPU backend (Vulkan/GL) selection and an
actual on-screen frame are **not** verified — no display/GPU in this
sandbox; needs confirmation on a machine with one.

## 2026-09-09 — Logging: fmt (not spdlog) for Phase 0

**Decision:** Start with `fmt` only for formatted logging output, add a
minimal `engine/core/log.h` wrapper. Defer pulling in spdlog until actual
need for async/file sinks, threaded logging appears (Phase 6+/7 once
server logging requirements are concrete). Wrapper API is designed so
swapping the backend later doesn't touch call sites.
