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

## 2026-09-09 — Block-break item drop: a direct mapping in VoxelClient, not a loot-table system

**Context:** Phase 5 added `engine/items`, and block-break needed a
real consumer of it (an `Inventory` nobody ever adds anything to is
dead code the same way `RecipeRegistry` still is without a crafting-UI
caller - see below). A general loot-table system (drop-rate rolls,
multiple possible drops per block, tool-dependent drops, fortune-style
multipliers) is exactly the kind of thing brief section 98 warns
against building ahead of need: there is exactly one droppable block
type (`game:stone`) in the entire game right now.

**Decision:** `VoxelClient`'s break handler checks `hit->block ==
stone_id` directly and calls `player_inventory.add_item(...)` with a
hardcoded `game:stone` item stack of 1 - a plain `if`, not a registry
or table. This is real (not a placeholder / not faked) but intentionally
not generalized. Revisit once a second droppable block type exists and
a hardcoded `if`/`else if` chain would actually start hurting -
likely alongside Phase 9's block/item content expansion, where a
proper `BlockId -> ItemStack[]` drop table (probably living on
`BlockDefinition` itself, mirroring `light_emission`) becomes the
obviously right shape because there's real data to shape it around.

## 2026-09-09 — Shaped recipe matching: single orientation, no mirroring

**Context:** `RecipeRegistry`'s shaped matching trims the queried
crafting grid to its bounding box and compares it against a recipe's
stored pattern. Some crafting systems (Minecraft's default recipe type
among them) also try a horizontally-mirrored version of the pattern, so
an asymmetric recipe matches regardless of which way the player happened
to arrange it.

**Decision:** Not implemented. Every recipe this project has actually
needed so far (none shipped yet beyond unit-test fixtures) is either
symmetric or doesn't care about position at all (shapeless), so mirror
matching has no real recipe to validate it against - building it now
would be guessing at behavior brief section 98 says not to guess at.
Adding it later is a small, contained change (try `matches_shaped`
again against a horizontally-flipped copy of the trimmed grid) if an
asymmetric recipe design ever actually needs it.

## 2026-09-09 — Failed block placement refunds the consumed inventory item

**Context:** Wiring `Inventory` into `VoxelClient`'s place-block path,
the natural order is "remove the item from inventory, then place the
block" (so a short-circuiting `if` can bail out early if the player
doesn't have the item). But the block placement itself can still fail
afterward - specifically, if the raycast hit a block right at the edge
of the loaded chunk area and the neighbor chunk the placement would
land in isn't loaded, `world.chunk_at_mutable()` returns null.
Left as originally written, that path would have consumed the player's
stone item and placed nothing - a silent item loss caught by re-reading
the code before considering the feature done, not by a test failing.

**Decision:** The chunk-not-loaded branch calls
`player_inventory.add_item(item_registry, {stone_item_id, 1})` to
refund exactly what was taken. This keeps the inventory's item count
accurate under a failure this build can actually hit (see "Known
Limitations": `VoxelClient` loads a static area, so walking to its edge
and aiming outward reproduces this today), rather than only under
failures that happen to not matter yet.

## 2026-09-09 — Lighting is single-chunk scoped: no cross-chunk bleed, no lateral sky spread

**Context:** `engine/lighting`'s block light BFS and sky light column
fill both only ever step within the `ChunkStorage<EdgeLength>` they're
given - they have no way to ask "what does the neighboring chunk look
like" the way `VoxelClient`'s greedy-mesh neighbor-remesh glue does for
geometry. Real light doesn't stop at chunk boundaries: a torch one
block from a chunk edge should light cells in the next chunk over, and
whether a chunk's own top layer gets full sky light depends on whether
the chunk above it is open sky or a solid roof.

**Decision:** Not implemented yet. `compute_block_light`/
`propagate_added_block_light`/`unpropagate_block_light` treat a
chunk's own boundary as the edge of the world (light simply stops
there, neither read from nor written to a neighbor); `compute_sky_light`
assumes every chunk column is open to the sky above it, regardless of
what chunk actually sits there. Extending this correctly needs the same
kind of neighbor-awareness `VoxelClient`'s break/place handler already
has for meshing (`neighbors_sharing_boundary`), but for light it's
harder: a chunk edit's light effect can, in principle, propagate many
chunks away (light travels up to 15 steps), not just into the
immediately adjacent chunk the way a single mesh face does. That's real
added complexity (a cross-chunk propagation queue, `World` needing to
answer "is this chunk coordinate loaded and what's in it" from inside
the lighting code) with no caller stressing it yet - `VoxelClient` loads
a small enough area, and has no visible torches/light-emitting content
placed anywhere, that the seams wouldn't be observable even with a
display. Revisit once a real light-emitting block is added to the game
content and the seams become an actual visible defect, not a
theoretical one.

Sky light also only fills straight down each column - no lateral spread
under overhangs (real sunlight leaks a little sideways beneath a ledge).
Also deferred for the same reason: nothing in this sandbox can see the
difference, and adding it now would be guessing at how much bleed
"looks right" with no display to check against (brief section 98).

## 2026-09-09 — AI wander system takes an explicit RNG, not a hidden global one

**Decision:** `game::systems::update_ai_wander` takes `std::mt19937&`
as a parameter rather than reaching for a static/global random engine.
Same reasoning as `engine/world::worldgen`'s noise functions: a system
whose behavior depends on randomness stays deterministic and testable
as long as the randomness source is explicit and caller-supplied - a
unit test can seed it and assert exact resulting positions/timers (see
`AIWanderSystem.ArrivalPicksANewTargetWithinWanderRadiusAndStartsIdling`,
which pins the idle-duration range to a single value specifically so
the test doesn't need to tolerate a range of acceptable outcomes).
`VoxelClient` seeds one fixed RNG (`kAiRngSeed`) for its AI entities for
the same reason every other piece of this vertical slice is
deterministic - a headless run's output is reproducible, not "probably
similar every time."

## 2026-09-09 — DayNightCycle: a real, ticking system with no renderer consumer yet

**Context:** Brief section 60 asks for a day/night cycle. Built as a
small, real, unit-tested system (`game::systems::DayNightCycle`) that
tracks elapsed time and produces a sky light scale via a cosine curve -
but nothing multiplies `engine/lighting`'s sky light values by it, and
nothing tints a rendered sky, since there is no persistent visible
scene to observe either change (no display in this sandbox, and the
chunk shader has no time-of-day uniform yet).

**Decision:** Ship the system now, wire it to actual light/render output
later. `VoxelClient` ticks it every frame and logs its state
(`time_of_day`/`sky_light_scale`) so the logic is exercised by a real
call site, not just unit tests - the same "real code, deferred
integration, honestly documented" pattern as `RecipeRegistry` (tested,
no crafting-UI caller yet). Wiring `sky_light_scale()` into
`engine/lighting`'s sky light values (multiplying every
`LightStorage::sky_light` read by it, most naturally at the point a
renderer samples light for shading) is the obvious next step once a
renderer actually samples per-voxel light at all - not done
speculatively now with nothing to visually verify it against.

## 2026-09-09 — Fixed: player/AI spawned embedded in the ground

**Context:** Found while adding the sky-light verification log for
Phase 6: `terrain_height()` returns the Y of the topmost *solid* block
(`worldgen.cpp`: `world_y <= height` is solid - confirmed by
`worldgen_test.cpp`'s own `expect_solid = ly <= height` assertion, so
this is `terrain_height`'s intended, tested contract, not a bug in
`terrain_height` itself). `client/main.cpp`'s player spawn code
(written in Phase 4) set the player's feet Y directly to
`terrain_height(...)`, i.e., to the topmost solid block's own Y rather
than the first open-air cell above it (`terrain_height(...) + 1`) - so
the player's AABB started overlapping the top layer of solid ground
instead of resting on its surface.

**Decision:** Fixed at the one call site that mattered:
`spawn_ground_y` in `client/main.cpp` is now
`terrain_height(kWorldSeed, 0, 0) + 1`, with a comment explaining the
off-by-one so it isn't reintroduced. Re-verified `LCU_VERIFY_BREAK_PLACE`
still round-trips correctly (break/place happen relative to a raycast
from the corrected spawn position, so the exact world coordinates in
the log shifted but the round-trip property held) and all existing
Phase 4/5 tests still pass unchanged - this was a `VoxelClient`
integration bug, not a defect in `engine/physics`/`engine/world`
themselves, which is why no engine-level test caught it.

## 2026-09-09 — UDP transport with a hand-rolled channel protocol, not TCP or a third-party library

**Context:** Phase 7 needed `engine/network` to deliver on
`ARCHITECTURE.md`'s four committed channel semantics
(`RELIABLE_ORDERED`, `RELIABLE_UNORDERED`, `UNRELIABLE`,
`UNRELIABLE_SEQUENCED`). Real options: (a) TCP for the reliable case and
raw UDP for the unreliable case, bolted together as two separate
transports; (b) a third-party reliable-UDP library (ENet,
GameNetworkingSockets, yojimbo); (c) a small hand-rolled ack/retransmit
protocol on top of one UDP socket per peer.

**Decision:** (c). (a) was rejected because TCP's own in-order,
head-of-line-blocking byte stream can't actually express "unreliable"
or "reliable but unordered" - layering two unrelated transports per
peer is also just more moving parts (two sockets, two failure modes)
for a game that has no reliable-vs-unreliable traffic split motivating
it yet. (b) was rejected for the same reason this project already
avoids GLM/a noise library/spdlog where a small, well-scoped
implementation is tractable and testable on its own: none of those
libraries are in the brief's dependency list, and the actual algorithm
(sequence numbers + an ack/retransmit loop + a reorder buffer) is
well-documented, bounded in scope, and - critically - fully verifiable
without any external dependency, including under real simulated packet
loss over real loopback sockets (see
`tests/network/loopback_integration_test.cpp`). A production MMO-scale
server might reasonably reach for (b) once profiling or real
multi-hundred-player traffic demands optimizations (congestion control,
bandwidth-aware packet coalescing) this hand-rolled version doesn't
have - see the next entry.

## 2026-09-09 — Reliable channel has no RTT estimation or congestion control yet

**Context:** `Connection`'s reliable channels (`ReliableOrdered`/
`ReliableUnordered`) retransmit an unacknowledged packet on a fixed
timer (`kDefaultRetransmitInterval`, 200ms) rather than an RTT-adaptive
one, and have no congestion control, bandwidth shaping, or maximum
resend count - a permanently unreachable peer's reliable packets are
retried forever.

**Decision:** Ship the simple, fixed-interval version now; it is
provably correct under real (tested) packet loss, which is the actual
Phase 7 requirement. RTT-adaptive timing and congestion control are
real optimizations with no real traffic pattern to tune them against
yet - brief section 76's "revisit only if profiling shows need" applies
directly here, same reasoning as `JobSystem`'s single mutex+condvar
design. A max-resend cutoff (eventually treating an unresponsive peer as
disconnected) is a real gap worth closing before Phase 8 needs to reason
about connection lifecycle/timeout, not before.

## 2026-09-09 — VoxelServer's connection model has no authentication

**Context:** `VoxelServer` treats the first UDP datagram it ever
receives from a given `Address` as a new client connecting - there is
no handshake secret, token, or any other proof of identity involved.

**Decision:** Acceptable for this phase's actual scope (proving real
client-server messages flow over the transport, in a sandbox with no
real network exposure) but explicitly not something to carry into any
real deployment - flagged here and in `PROJECT_STATE.md`/`NETWORKING.md`
so it isn't mistaken for a finished feature later. A real system needs
this addressed before Phase 9's mod/server-browser work, if not sooner;
revisit once player identity (an account/profile system) exists to
authenticate against at all - there's nothing to check credentials
against yet, so building an auth handshake now would be securing a door
with nothing behind it.

## 2026-09-09 — Chunk streaming deferred: fragmentation prerequisite

**Context:** `engine/serialization::chunk_serializer` already produces
zstd-compressed chunk bytes (Phase 3) - reusing it to send a chunk to a
connecting client looked, at first glance, like a small Phase 8 addition
(the compression work is already done). It isn't: a compressed 16^3
chunk is realistically a few KB, and `engine/network::kMaxDatagramSize`
is 1200 bytes - a single chunk doesn't fit in one UDP datagram.
`Connection` has no concept of "one logical message split across
several packets and reassembled in order," only whole-packet
channels.

**Decision:** Not implemented this phase. Building message fragmentation
correctly (splitting, numbering fragments, detecting a complete set,
reassembling, handling a fragment lost mid-transfer) is a real,
independent protocol feature - the kind of thing worth getting right
with its own focused test coverage, the same rigor already given to
`Connection`'s ack/retransmit logic, not bolted on hastily as a means to
an unrelated end (chunk streaming). Both `VoxelClient` and `VoxelServer`
currently generate their own local copy of the world from the same
hardcoded seed instead, which is sufficient for this phase's actual
goal (proving prediction/interpolation/interest-management work over a
real connection) without needing chunk data to cross the wire at all.
Revisit when fragmentation is built for its own sake (a natural
Phase 8-follow-up or Phase 9 item), then chunk streaming becomes a
straightforward consumer of it.

## 2026-09-09 — Server-authoritative player physics trusts client dt (clamped)

**Context:** `VoxelServer` now runs real physics
(`apply_gravity`/`integrate_player`) per `PlayerInput` it receives,
using the `dt` the client itself reports for that input. A more
rigorous authoritative server would derive its own timestep from
message arrival timing rather than trusting a client-supplied number at
all (a malicious client could report a huge `dt` to move far in one
input).

**Decision:** Clamp `dt` to `kMaxAcceptedInputDt` (0.25s) rather than
building real server-side input-timing derivation or a full movement
validator (speed/acceleration limits, raycasting the claimed path
against solid blocks, etc.). The clamp closes the most obvious abuse
(an absurd single-frame jump) with one line; the deeper anti-cheat
problem (a client that sends many small, individually-plausible but
cumulatively-wrong inputs) is real and unaddressed, appropriate for a
sandbox with no untrusted network exposure and not yet for a public
deployment - see `PROJECT_STATE.md` "Known Limitations". Revisit once
there's an actual adversarial testing need (a real deployment, or
Phase 9's modding surface making server trust boundaries matter more).

## 2026-09-09 — VoxelClient doesn't yet use the server's replicated world seed

**Context:** `VoxelServer`'s `Welcome` message carries a `world_seed` -
the intent (see `ROADMAP.md`'s vertical-slice thinking) is that a
connecting client should generate the *same* world the server is
running, from that seed, rather than assuming both sides happen to
agree on a hardcoded constant. `VoxelClient`'s current structure builds
its `World` (and loads/meshes/lights every starting chunk) well before
the main loop - and thus well before any network round-trip to receive
a `Welcome` could possibly complete.

**Decision:** Not restructured this phase. `VoxelClient` still calls its
own compile-time `kWorldSeed` (1337) for `World` construction, and
merely logs the received `Welcome.world_seed` to confirm the message
itself decodes correctly - a real, verified round-trip of the *message*,
just not yet acted on. Both client and server hardcode the same 1337
today, so there is no observable mismatch to motivate the restructuring
under real testing pressure yet. Deferring `World` construction until
after a network round-trip is a real, somewhat invasive structural
change (every piece of `VoxelClient` startup that currently runs
synchronously and unconditionally would need to become conditional on
"networked or not, and if networked, has Welcome arrived yet") - proportionate
to do once there's an actual reason two peers' seeds would differ (e.g.
Phase 9 server-side world configuration a client can't already guess),
not preemptively.

## 2026-09-09 — Lua 5.4 (official upstream), embedded via its own amalgamation

**Context:** Phase 9 needs an embeddable scripting language for mods
(brief section 84). Candidates considered: Lua 5.4 itself, LuaJIT, and a
C++ binding layer on top of either (sol2, LuaBridge). `github.com/lua/
lua` (the official upstream mirror) ships no CMake support at all - just
a plain Makefile C project - so pulling it in via `FetchContent_
MakeAvailable` (this repo's usual pattern for every other dependency)
doesn't work.

**Decision:** Plain Lua 5.4.7, not LuaJIT, and no binding library (raw
C API, not sol2/LuaBridge). Lua 5.4 over LuaJIT: LuaJIT's last release
targets Lua 5.1 semantics and its maintenance status is a real concern
for a project meant to last; this project's scripting workload (mod
registration calls at startup, occasional event handlers) has no
performance profile that needs a JIT. No binding library: sol2/LuaBridge
buy convenience (automatic type marshalling, RAII wrappers) at the cost
of a template-heavy header-only dependency and another abstraction layer
between engine code and the actual Lua C API - for the handful of
binding functions this phase needs (`register_block`, `register_item`,
`lcu_subscribe`), the raw
`lua_pushlightuserdata`/`lua_pushcclosure`/`luaL_check*` pattern is a
few lines each and keeps the dependency surface to just Lua itself (no
overengineering ahead of need, brief section 98).

Fetched via `FetchContent_Declare` + `FetchContent_GetProperties`/
`FetchContent_Populate` (not `FetchContent_MakeAvailable`, since that
requires the populated source to have its own `CMakeLists.txt`) at tag
`v5.4.7`, then built by hand: `add_library(LuaLib STATIC
${lua_SOURCE_DIR}/onelua.c)` compiled with `-DMAKE_LIB`. `onelua.c` is
Lua's own official single-translation-unit amalgamation (it
`#include`s every other `.c` file in the distribution); `MAKE_LIB`
selects the branch that omits `lua.c`'s `main()`, producing just the
embeddable library - confirmed by cloning the real upstream repo and
inspecting `onelua.c`'s preprocessor guards directly before writing any
CMake code, not assumed from documentation. No `LUA_USE_LINUX`/
`LUA_USE_POSIX` platform define is set (portable ANSI C mode) - the only
thing that trades away is `package.loadlib` (dynamic C-module loading
from Lua), which nothing here needs since mods are pure Lua scripts, not
compiled C extensions. Root `CMakeLists.txt` gained `LANGUAGES CXX C`
because `onelua.c` is a C file and CMake's C toolchain isn't configured
otherwise (a real configure-time error surfaced this, not a proactive
change) - documented inline as being needed only for Lua, since every
other target in this repo is C++.

## 2026-09-09 — engine/scripting sandboxes the standard library

**Context:** A full Lua VM opened with `luaL_openlibs()` gives a mod
script `io`/`os`/`package` - arbitrary file I/O, process execution
(`os.execute`), and dynamic native-library loading. `example_mod` (and
any future third-party mod) should be able to register content and
react to events without also being able to read/write arbitrary files
or shell out.

**Decision:** `engine/scripting::LuaState`'s constructor opens only
`base`/`table`/`string`/`math` via individual `luaL_requiref` calls, not
`luaL_openlibs()`. `base` still includes `print`, so a mod can log
output for debugging despite `io` being absent. This is a real,
enforced boundary (verified by a unit test asserting `io`/`os`/
`package`/`require` are all `nil` from Lua's perspective), not a
documented convention a mod could route around - there is no
alternative code path to those libraries once the VM is constructed
this way. It does not address CPU/memory/time resource limits (an
infinite Lua loop still hangs the host process) - deferred until a real
need for it exists (untrusted third-party mods, not just this repo's
own `example_mod`) - see `PROJECT_STATE.md` "Known Limitations".

## 2026-09-09 — EventBus exposed on the server even though nothing emits through it yet

**Context:** `mods/example_mod/init.lua` is one script shared, unmodified,
between `VoxelClient` and `VoxelServer` (both load `mods/` independently
at startup - see "VoxelClient doesn't yet use the server's replicated
world seed" above for the broader pattern of both sides agreeing by
construction rather than by sync). It unconditionally calls
`lcu.subscribe("block_broken", ...)`. The server has no source of
`block_broken` events today - block edits aren't replicated
(`NETWORKING.md`), so nothing server-side ever calls
`emit_block_broken()`.

**Decision:** Construct a real `EventBus` on the server and call
`expose_to_lua()` on it anyway, even though `emit_block_broken()` is
never called from `server/main.cpp`. Verified this was a real bug, not
a hypothetical one: without it, `example_mod`'s `init.lua` threw
`attempt to index a nil value (global 'lcu')` on the server and the
entire mod failed to load (including its otherwise-successful
`register_block`/`register_item` calls before that line) - see
`CHANGELOG.md`/git history for the exact error message hit while
verifying this phase. The alternative (making the example mod
defensive - `if lcu and lcu.subscribe then ... end`) would work but
pushes a host-capability-detection burden onto every mod author for
something that should just be a uniform part of the modding API surface
across client and server, the same way `register_block`/`register_item`
already are. This is not premature - a shared Lua API that silently
differs between hosts is a real correctness trap for any mod, not a
speculative one; the corresponding `ItemRegistry` was added to the
server for the identical reason (its absence broke the same mod's
`register_item` call the same way, caught first during this same
verification pass).

## 2026-09-09 — QualityProfile lives in engine/core, not engine/platform

**Context:** Phase 10 needs a device-performance-tier concept (brief
section 60's MOBILE_LOW/MEDIUM/HIGH) that scales how much world gets
streamed. The obvious home is `engine/platform`, next to `InputState`/
`TouchInputBackend` - except `engine/platform` is only built when
`LCU_BUILD_CLIENT` is on, and `server/CMakeLists.txt` explicitly
forbids `VoxelServer` from linking `Lcu::Platform` (see ARCHITECTURE.md
"Server has zero GPU/window dependency", enforced and `ldd`-verified
since Phase 7). `VoxelServer` needs the same chunk-load-radius scaling
`VoxelClient` does - a dedicated server for a mobile-heavy player base
plausibly wants smaller regions too - so a platform-gated home would be
wrong.

**Decision:** `lcu::core::QualityProfile`/`chunk_load_settings_for`/
`parse_quality_profile` live in `engine/core` instead - the one module
every target in this repo already links unconditionally. The type
itself has nothing platform-specific about it (it's an enum and a
struct of integers); only the *name* "quality profile" evokes
`engine/platform`. `Desktop` is defined to numerically match this
project's pre-existing hardcoded `kLoadRadiusXZ=1`/`kMinChunkY=0`/
`kMaxChunkY=3` exactly, so introducing the whole profile system changes
zero default behavior - confirmed via a real run showing "Loaded 36
chunks" unchanged before and after this phase.

## 2026-09-09 — No Android Gradle project / iOS Xcode project this phase

**Context:** Phase 10 nominally includes "real Android Gradle/NDK
project structure, real iOS Xcode project generation" (see
`TASK_QUEUE.md`'s original phase description). This sandbox is
Linux-only with no Android NDK and no Xcode/macOS host - `cmake
--preset android-arm64` was actually run to confirm this, and fails
exactly at CMake's own NDK-detection step, not from any error in this
repo's CMake.

**Decision:** Do not write a Gradle project (`build.gradle.kts`,
`AndroidManifest.xml`, a `SDLActivity`-based entry point, NDK
`CMakeLists.txt` glue beyond what already exists) or an Xcode project/
scheme/`Info.plist` this phase. Both would be substantial, genuinely
untestable code in this environment - not "harder to verify," but
*impossible* to configure, build, or run here, meaning any bug in it
(a wrong Gradle DSL call, a missing NDK ABI filter, a malformed
Info.plist key) would go undetected indefinitely and could sit in the
repository looking finished while being silently broken. That is
precisely what brief section 96 ("never claim done beyond what was
verified") and this project's running precedent (bgfx's real GPU
backend, SDL relative-mouse-mode, chunk network streaming - all
deferred at points where this sandbox genuinely cannot verify them)
say not to do. What *is* real and delivered this phase - the
`CMakePresets.json` entries (pre-existing, re-verified reachable) and
the touch-input/quality-profile abstractions the mobile app would
eventually use - carries its own weight without a hollow project shell
wrapped around it. Revisit once an actual NDK/Xcode toolchain (a CI
runner or a developer's machine) is available to build and exercise a
real mobile project against.

## 2026-09-09 — Google Benchmark for tools/benchmark, opt-in via LCU_BUILD_TOOLS

**Context:** Phase 11 needs real, repeatable micro-benchmarks (brief
section 96/98: measure before optimizing, no guessed numbers). A
hand-rolled `std::chrono` timing loop would work but reinvents warm-up
handling, statistical iteration-count selection, and reporting that a
mature library already solves correctly.

**Decision:** Google Benchmark, FetchContent-pinned to v1.9.1, mirroring
exactly how GoogleTest is already fetched (same vendor, same pattern) -
no new justification burden the way a less-established dependency would
need. Declared inside `if(LCU_BUILD_TOOLS)` in `third_party/
CMakeLists.txt`, so a default build (where `LCU_BUILD_TOOLS` is OFF)
never fetches or builds it - matches `LCU_BUILD_SHADER_TOOLS`'s existing
opt-in-for-extra-build-time precedent. `BENCHMARK_DOWNLOAD_DEPENDENCIES`
is forced OFF - benchmark's own CMake would otherwise try to fetch a
second, independently-pinned copy of GoogleTest for its own test suite,
duplicating the one this repo already fetches; `BENCHMARK_ENABLE_TESTING`
is also OFF since this repo doesn't run benchmark's own tests.

Benchmarked the real engine functions client/server actually call
(`mesh_chunk_greedy`, `compute_block_light`, `move_and_collide`, ...),
not toy re-implementations - a benchmark against a fake stand-in would
measure the wrong thing and give false confidence. Ran in both this
project's default `Development` build type and a real `Release` build:
`Development` sets no optimizer flags (it exists for fast
iteration/debuggability, not speed), so Google Benchmark's own "Library
was built as DEBUG" warning on that run is correct and expected, not a
bug - the `Release` numbers are the ones worth comparing future changes
against, and both are recorded in `BUILD_STATUS.md` for that reason.

## 2026-09-09 — Procedurally generated audio content instead of a WAV pipeline

**Context:** Phase 12 needs `engine/audio` to actually play something, not
just prove an `SDL_AudioStream` can be opened. This project has no
audio-asset loading pipeline (no WAV/OGG decoder, no asset directory
convention for sounds), and brief section 12 requires GPL-3.0/own-IP
content only - no Minecraft or other third-party assets, checked in or
otherwise.

**Decision:** `engine/audio::generate_sine_wave` synthesizes a pure tone
in code at runtime instead. This is real, immediately playable audio
content - not a placeholder silence buffer standing in for a future
asset - and it is trivially this project's own work, sidestepping both
gaps at once (no decoder needed, no licensing question to resolve).
`VoxelClient` uses two fixed tones (220Hz/330Hz for break/place) as a
real, working demonstration, mirroring how `example_mod` (Phase 9) is a
real working mod rather than loader infrastructure with nothing loaded
into it. A real sound-effect content pipeline (loading authored audio
files) is a separate, larger piece of work, deferred until actual game
content creates a reason for it - seen the same way as the texture
atlas gap noted throughout Phase 2-6's decisions.

## 2026-09-09 — Positional audio is pan + linear falloff, not HRTF/3D audio

**Context:** "Positional audio" (brief section 96, Phase 12) covers a
huge range of real techniques, from a simple stereo pan up through full
head-related-transfer-function binaural rendering, reverb zones, and
occlusion. This project has exactly two sound-emitting moments (a block
break, a block place) and no simultaneous multi-source mixing scenario
yet to design against.

**Decision:** `compute_stereo_pan` (lateral angle to the listener's
right vector, mapped to a left/right gain pair) and
`distance_attenuation` (linear falloff to silence at a fixed max
distance) - both plain, hand-verifiable math, fully unit tested with
exact expected gains at cardinal angles (directly ahead, fully left/
right, 45 degrees) rather than approximate/fuzzy assertions. This is a
real, working positional cue (a sound left of the player is audibly
quieter in the right ear) without inventing HRTF filters, reverb
convolution, or an audio-occlusion raycast against the voxel world -
none of which has a concrete use case yet (no multiple simultaneous
sources, no indoor/outdoor acoustic distinction in any existing
content). Revisit once real gameplay content needs more than "which
direction and how far."

## 2026-09-09 — Touch button layout promoted to a shared header for engine/ui

**Context:** Phase 10's `TouchInputBackend` defined its six on-screen
button hit-regions (`Jump`/`Interact`/`PlaceBlock`/`Sprint`/`Crouch`/
`Inventory`) as a private `constexpr` array inside `touch_input.cpp`.
Phase 12 needs to actually draw those buttons on screen (closing the
"a player would currently be dragging/tapping blind" limitation
recorded when `TouchInputBackend` was built) - which needs the same
rects and labels `TouchInputBackend` hit-tests against.

**Decision:** Promoted the array to `lcu::platform::kTouchButtonLayout`
in a new public header, `touch_control_layout.h`, alongside
`touch_input.h` in `engine/platform`. `TouchInputBackend::update()` and
`engine/ui::draw_debug_overlay` both read from this single definition -
there is no way for a button's hit-test rect and its drawn position to
independently drift apart, since there is only one rect. This mirrors
the project's established "one authoritative definition" pattern for
anything two independently-evolving pieces of code both need to agree
on exactly (e.g. `game::systems::protocol`'s shared wire messages,
Phase 8 - see that phase's `DECISIONS.md`/`CHANGELOG.md` entries for the
same reasoning applied to client/server message encoding).

## 2026-09-09 — engine/ui draws through Renderer, never bgfx directly

**Context:** ARCHITECTURE.md restricts bgfx-header inclusion to
`engine/rendering` (mirrored by `engine/scripting`'s Lua-header rule and
`engine/audio`'s SDL-audio-header rule, both established earlier).
`engine/ui::draw_debug_overlay` needs to put text on screen, and bgfx's
own debug-text API (`bgfx::dbgTextPrintf`/`dbgTextClear`) is the
mechanism available without building a font/texture-atlas renderer from
scratch (no atlas exists yet - see the "procedurally generated audio
content" decision above for the same reasoning applied to sound).

**Decision:** Added `Renderer::draw_debug_text`/`clear_debug_text` -
thin wrappers, not a new abstraction layer - and `engine/ui` calls
those instead of touching `<bgfx/bgfx.h>` itself. `draw_debug_text`
takes the text as `const std::string&` and passes it to
`bgfx::dbgTextPrintf` via a fixed `"%s"` format string, not the caller's
string as the format argument directly - text can come from data this
codebase doesn't fully control (e.g. a future mod-registered label), and
printf-family functions treat their format argument as executable-ish
(a stray `%s`/`%n` embedded in it would misbehave or crash).
