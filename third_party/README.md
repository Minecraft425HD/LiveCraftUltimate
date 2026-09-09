# Third-party dependencies

All dependencies are fetched via CMake `FetchContent` from
`third_party/CMakeLists.txt`, pinned to an exact tag. Nothing here is
vendored as committed source.

| Dependency | Version/Tag | License | Source | Build method | Used by |
|---|---|---|---|---|---|
| fmt | 11.0.2 | MIT | github.com/fmtlib/fmt | CMake (FetchContent) | `engine/core` logging |
| SDL3 | release-3.2.10 | zlib | github.com/libsdl-org/SDL | CMake (FetchContent) | `engine/platform`, `client` |
| GoogleTest | v1.15.2 | BSD-3-Clause | github.com/google/googletest | CMake (FetchContent) | `tests` |
| bgfx.cmake (bx/bimg/bgfx) | v1.159.9485-575 | BSD-2-Clause (bgfx/bx/bimg) | github.com/bkaradzic/bgfx.cmake | CMake (FetchContent), community wrapper around upstream GENie build | `engine/rendering`, `client` |
| zstd | v1.5.7 | BSD-3-Clause (dual-licensed, also GPLv2) | github.com/facebook/zstd | CMake (FetchContent, `SOURCE_SUBDIR build/cmake`), static lib only (`ZSTD_BUILD_PROGRAMS`/`_TESTS`/`_SHARED` off) | `engine/serialization` chunk save/load |

Planned, not yet added (pulled in when the system that needs them is
actually implemented — see `DECISIONS.md`):

| Dependency | Planned for | Notes |
|---|---|---|
| Lua | Phase 9 (scripting/modding) | sandboxed API surface only |
| A physics library | Only if `engine/physics` AABB/voxel collision proves insufficient | brief section 6: "only if truly required" |

## Verifying network reachability

This environment's outbound network policy allows `git clone` over
`https://github.com/...` and `https://raw.githubusercontent.com/...`, but
blocks `https://api.github.com/...` (403). `FetchContent` uses plain git
clones, so it works from here. If a future environment blocks git access
entirely, dependencies would need to be pre-mirrored or vendored — not yet
necessary.
