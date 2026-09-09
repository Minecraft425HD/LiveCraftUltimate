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

## 2026-09-09 — BlockRegistry lives in engine/voxel, not engine/modding

**Context:** The brief's directory layout (section 7) puts "registries"
conceptually near modding, but `BlockRegistry` is needed by
`engine/voxel` itself (chunk storage stores `BlockId`s that only mean
anything relative to a registry) well before Phase 9's Lua/mod-loading
work exists.

**Decision:** `BlockRegistry` and `BlockDefinition` live in
`engine/voxel` now. `engine/modding` (Phase 9) will add mod-facing
registration (Lua bindings, `mod.json` parsing, hot reload) on top of
this same registry rather than owning a separate one - the registry
itself doesn't need to move, only gain a scripting-facing API layer.
Revisit only if that layering turns out to be awkward once Phase 9
starts.

## 2026-09-09 — Chunk storage: flat array now, no palette compression yet

**Context:** Minecraft-style engines often use per-chunk palette
compression (small run-length/indexed encoding) to cut memory for
mostly-uniform chunks (e.g. all-stone underground).

**Decision:** `ChunkStorage<EdgeLength>` starts as a flat, contiguous
`std::array<BlockId, Volume>` - simplest correct thing, already
cache-friendly and allocation-free. Deferred: palette compression. Brief
section 76 requires baseline -> profile -> optimize, in that order;
there is no baseline yet to know whether this matters, and adding
compression now would be optimizing before measuring (brief section 98).
Revisit once Phase 3 world streaming gives real chunk-memory numbers to
profile.

## 2026-09-09 — Block state (rotation/orientation/etc.) not encoded yet

**Context:** Brief section 17 wants compact block state (rotation,
powered, open, age, variant, waterlogged) alongside the block type.

**Decision:** `Chunk` currently stores only `BlockId` per cell, no
packed state bits. No block in the registry yet needs state (there are
no blocks registered at all outside unit tests) - adding a state-packing
scheme now would be speculative. When the first stateful block is
needed (a directional block, a candidate for the Phase 9 example mod),
extend the per-cell storage then, informed by what that block actually
needs to encode.

## 2026-09-09 — Job system: single mutex+condvar, correctness first

**Context:** Brief section 19 asks for a job system with priority,
dependencies, cancellation, worker threads - needed (section 18) before
chunk meshing can run off the main thread. Job systems are a classic
place to over-engineer prematurely (lock-free queues, work-stealing
per-thread deques, etc.).

**Decision:** `engine/jobs::JobSystem` uses one `std::mutex` +
`std::condition_variable` guarding all scheduling bookkeeping (the
job/dependency graph, the ready queue). Job *bodies* run unlocked - only
scheduling decisions are serialized, not the work itself. This is not
the fastest possible design (a lock-free MPMC queue or per-worker
work-stealing deques would scale better under heavy contention), but it
is straightforward to reason about and verify correct, which matters
more when nothing depends on its throughput yet (brief section 76:
baseline -> profile -> optimize, in that order; section 98: no
overengineering). Revisit only once Phase 11 profiling on real workloads
(chunk generation/meshing at actual world-streaming volumes) shows
scheduling contention is an actual bottleneck.

**Cancellation semantics:** `cancel()` only prevents jobs that haven't
started (`Pending`/`Ready`) from running, and cascades to their
dependents (a job whose dependency is cancelled can never satisfy its
precondition, so it's cancelled too). It cannot preempt a `Running` job -
no consumer has needed that yet, and cooperative-preemption support
(checking a cancellation flag inside long-running job bodies) is easy to
add later for whichever job type first needs it (likely worldgen/chunk
generation when a player moves away before generation finishes).

**Verification approach:** unit tests alone are not strong evidence for
concurrent code (a data race can pass every run and still be UB). Ran
the job-system test suite 200x in the normal build and 50x under GCC
ThreadSanitizer (`-fsanitize=thread`, Clang's TSan runtime wasn't
installed in this sandbox) - zero failures, zero race reports either
way. See `BUILDING.md` "Testing under ThreadSanitizer" for the exact
commands; re-run this whenever `engine/jobs` changes.

## 2026-09-09 — Greedy meshing: transparent-vs-transparent never draws a face yet

**Context:** Standard opaque-culling logic (draw a face where exactly one
side is opaque) means two adjacent transparent blocks - even of
*different* materials, e.g. glass touching water - never get a face
between them. A "complete" transparent-layer mesher would need
same-material vs. different-material rules (glass-glass: no face;
glass-water: draw a face) to look right.

**Decision:** Ship opaque-layer meshing now, with `ChunkMesh::transparent`
and `::water` present as real (currently always-empty) members so the
layer separation the brief asks for (section 18) already exists
structurally. Defer transparent-vs-transparent face rules until an
actual transparent block is registered somewhere (Phase 9's example mod
is the likely first case) - there is no way to validate that logic
correctly today with zero transparent content, and building it
speculatively risks guessing the wrong rules. Documented explicitly (not
silently) via a `TwoAdjacentTransparentBlocksProduceNoOpaqueFaces` test
and this entry, per brief section 96 (no fake completion).

## 2026-09-09 — Shader compilation: build bgfx's shaderc, opt-in

**Context:** An actual `bgfx::submit()` draw call needs a compiled
shader program; bgfx has no runtime shader compilation, only offline
compilation via its `shaderc` tool. Building `shaderc` pulls in
glslang, SPIRV-Tools, SPIRV-Cross, and Dawn/Tint (WGSL) - a meaningfully
heavier build than bgfx's runtime library alone (~700 additional build
steps in this sandbox).

**Decision:** Added `LCU_BUILD_SHADER_TOOLS` (default OFF) rather than
folding shader compilation into the default `LCU_ENABLE_BGFX` path.
Verified feasible first (a scratch build succeeded, ~700 steps, no
dependency failures) before committing to it as the real path, per
brief section 76 (baseline before committing time). When ON,
`third_party/CMakeLists.txt` builds only `BGFX_BUILD_TOOLS_SHADER`
(not geometryc/texturec, which nothing here needs) and includes
bgfx.cmake's `bgfxToolUtils.cmake` (not auto-included via FetchContent
the way `find_package(bgfx)` would) to get the `bgfx_compile_shaders()`
CMake helper, plus manually sets `BGFX_SHADER_INCLUDE_PATH` (also only
auto-set by the installed-package path).

**Follow-up for other environments/CI:** a full clean build with
`LCU_BUILD_SHADER_TOOLS=ON` takes real, non-trivial time (tint/dawn
alone is substantial). CI machines building the client with real shaders
should budget for this; the default `LCU_ENABLE_BGFX=ON` path without
shader tools stays fast for iterating on non-rendering code.

## 2026-09-09 — Chunk shaders: minimal placeholder, no texturing

**Decision:** `client/shaders/{vs_chunk,fs_chunk}.sc` implement the
simplest correct thing: transform position, pass the vertex normal
through, shade with one fixed directional light plus ambient. No
texture sampling - there is no texture atlas (Phase 12) or per-block
color/material data flowing through yet. `MeshVertex.u`/`.v` already
carry quad-local UVs (see `greedy_mesher.h`) for whenever texturing
lands, so the vertex format won't need to change, only the shaders and
the material/texture binding around them.

**bgfx_compile_shaders() gotcha recorded for future reference:** its
`VARYING_DEF` argument is NOT resolved to an absolute path internally
(unlike `SHADERS`, which is), but the generated custom command runs
with the CMake *build* directory as its working directory. A
source-relative `VARYING_DEF` path therefore silently fails to parse at
build time with confusing HLSL-parser errors about unknown variables,
not a "file not found" error. Always pass it as an absolute path (e.g.
via `CMAKE_CURRENT_SOURCE_DIR`).

## 2026-09-09 — Chunk save compression: zstd

**Context:** Brief sections 6/44 want "a suitable compression library"
for the save system. Candidates: zlib/miniz (ubiquitous, weaker
ratio/speed), LZ4 (very fast, weaker ratio), zstd (strong ratio *and*
speed, has a built-in per-frame content checksum - directly useful for
corruption detection, brief section 87).

**Decision:** zstd. The built-in checksum (`ZSTD_c_checksumFlag`) is
what actually detects corrupted chunk files in
`load_chunk_from_file` - not a bespoke CRC we'd have had to write and
verify ourselves. BSD-3-Clause licensed (Facebook dual-licenses it
BSD/GPLv2; using the BSD terms), actively maintained, and its CMake
lives at `build/cmake` in the repo (not the root), so
`FetchContent_Declare` needs `SOURCE_SUBDIR build/cmake` - recorded here
since it's an easy thing to omit and get a confusing "no CMakeLists.txt"
error instead. Static-lib-only build (`ZSTD_BUILD_PROGRAMS`/`_TESTS`/
`_SHARED` all off) - nothing here needs the zstd CLI or its own test
suite. The CMake target it exposes is `libzstd_static` (not `zstd::libzstd`
or similar - verified by building it, not assumed).

## 2026-09-09 — World streams a 3D cube, not yet a horizontal disc

**Context:** Real voxel games typically stream a horizontal disc/square
around the player plus a bounded vertical range (you don't need chunks
far above/below you even at long render distance), not a full 3D sphere/
cube by Euclidean/Chebyshev distance.

**Decision:** `World::update_streaming` streams a full 3D cube by
Chebyshev distance for now - simpler, and correct as far as it goes
(brief section 22's actual requirement, "never load the whole world",
is satisfied either way). There is no camera/player yet to supply a
meaningful "horizontal" plane or view direction to weight against
(brief section 22's priority list: player position, view direction,
movement direction, visibility, distance - only the last is implemented
today). Revisit once Phase 4 gives `World` a real caller with an actual
camera/movement vector to stream against; building the disc-shaped
version now would be guessing at parameters (world height bounds, disc
vs. cube) with nothing to validate them against.

## 2026-09-09 — Worldgen: own value-noise implementation, no library

**Decision:** `engine/world::worldgen::terrain_height` implements a
small seeded hash + 4-octave value noise directly rather than adding a
noise library (e.g. FastNoise2, libnoise). The brief's dependency list
doesn't include one, and a deterministic, testable noise function is a
small, well-scoped, self-contained problem - same reasoning as `engine/math`
not depending on GLM (see the "own minimal math library" entry above).
Only continental+terrain (brief section 21's first two pipeline stages)
are implemented; climate/biome/caves/ores/structures/vegetation/
decoration are later stages with no consumer yet (no biomes or
structure types registered anywhere), so building them now would be
speculative rather than driven by an actual need.

## 2026-09-09 — Logging: fmt (not spdlog) for Phase 0

**Decision:** Start with `fmt` only for formatted logging output, add a
minimal `engine/core/log.h` wrapper. Defer pulling in spdlog until actual
need for async/file sinks, threaded logging appears (Phase 6+/7 once
server logging requirements are concrete). Wrapper API is designed so
swapping the backend later doesn't touch call sites.

## 2026-09-09 — Player grounding: a dedicated ground-probe, not the movement collision result

**Context:** While implementing `engine/physics::integrate_player`, an
early draft set `state.grounded = result.grounded` where `result` comes
straight from that frame's `move_and_collide` call, and
`CollisionResult::grounded` is defined as "movement was blocked while
moving downward" (`delta.y < 0.0f && hit_y`). Traced through by hand
before writing any test: a player standing motionless on solid ground,
with no fall and no jump that particular frame, has `delta.y == 0.0f`,
so `hit_y` is never even evaluated as a downward block - `result.grounded`
comes out `false` even though the player is plainly resting on the
ground. A caller driving animation/jump-eligibility/fall-damage off
`state.grounded` would see it flicker false every frame the player
doesn't happen to be actively falling, which is most frames.

**Decision:** Added `probe_grounded()` - a small dedicated
`move_and_collide` call with a fixed tiny downward delta
(`kGroundProbeDistance = 0.05f`), called once after the real movement is
resolved, regardless of what that movement's own delta.y was. `grounded`
is now "is there solid ground within 5cm below me right now", decoupled
from "did I collide moving down this specific frame". Costs one extra
(cheap) collision sweep per `integrate_player` call.

**Verification:** caught by hand-tracing the logic against a concrete
scenario before writing the test that would have caught it live - see
`PlayerPhysics.StationaryGroundedPlayerStaysGroundedWithoutFalling` in
`tests/physics/collision_test.cpp`, added specifically to pin this down
as a regression test.

## 2026-09-09 — Camera look input: arrow keys as an interim control scheme

**Context:** Phase 4 needed the first-person camera to actually turn
somehow. Real mouse-look needs SDL3 relative-mouse-mode plumbing
(`SDL_SetWindowRelativeMouseMode` + accumulating per-frame mouse deltas
through the `InputState`/`Action` abstraction, brief section 27) that
doesn't exist yet, and is also not meaningfully verifiable in this
display-less sandbox even if built (a synthetic mouse-delta event still
wouldn't prove anything about how mouse-look feels).

**Decision:** Added `Action::LookUp/LookDown/LookLeft/LookRight`, bound
to the arrow keys, and drove `FirstPersonCamera::add_yaw_pitch` from
them in `VoxelClient`'s frame loop (scaled by a fixed `kLookSpeed`
radians/s and real per-frame delta time). This is a real, immediately
usable control scheme - not a placeholder that silently does nothing -
and it exercises the exact same `InputState`/`Action` path a mouse-look
backend would plug into later; only the backend producing the deltas
changes. Revisit once SDL relative-mouse-mode is worth the plumbing
(likely alongside Phase 10's touch input, which needs its own delta
source anyway).

## 2026-09-09 — Block mutation remeshes the edited chunk and any neighbor sharing the boundary

**Context:** `mesh_chunk_greedy` treats any position outside a chunk's
own bounds as air (`detail::block_or_air`) when deciding whether a
boundary face is visible - so a chunk's mesh, once built, embeds an
assumption about what was on the *other* side of each of its six faces
at meshing time. Editing a block at local coordinate 0 or `EdgeLength-1`
on any axis changes what that assumption should have been for the
chunk on the other side of that boundary too, not just the edited
chunk.

**Decision:** `VoxelClient`'s break/place handling computes which
already-loaded neighbor chunks (if any - up to three, at a chunk corner)
share the mutated block's boundary, and remeshes/re-uploads each of them
alongside the primary edited chunk. This keeps face culling correct
across chunk seams without a general "dirty chunk" propagation system -
proportionate to a single-block edit's blast radius, not a queue/graph
solving a problem this doesn't have yet. Revisit if/when edits start
happening in bulk (explosions, world-edit tools) and the "remesh every
touched neighbor synchronously" approach shows up as a real cost.

## 2026-09-09 — Headless break/place verification via a synthetic input hook

**Context:** `VoxelClient`'s Phase 4 break/place logic is real
edge-detected `InputState` handling (`Action::Interact`/`PlaceBlock`
transitioning from up to down), but this sandbox has no real keyboard
to press - `SDL_GetKeyboardState` always reports everything up under
`SDL_VIDEODRIVER=dummy`. Without some way to drive a keypress, the
mutate-world -> remesh -> re-upload pipeline would be unverified code,
which brief section 96 rules out claiming as done.

**Decision:** Added `LCU_VERIFY_BREAK_PLACE`, an env var that - only
when set - overwrites `input`'s `Interact`/`PlaceBlock` bits directly
for one frame each (frame 3 and frame 6) after `KeyboardInputBackend`
has already run, before the edge-detection logic reads them. This is
indistinguishable, from the edge-detection code's point of view, from a
real single-frame key press and release; it exercises the actual
production code path (not a separate test-only branch) end to end. Real
interactive runs never set this env var, so it has zero effect outside
verification. Confirmed via a real run: a block is broken and logged,
then the next frame's raycast (now reaching the block below) is used to
place a new block back at the exact same world coordinate the broken
one occupied.
