# Third-party dependencies, fetched reproducibly via FetchContent and
# pinned to an exact tag/commit. See third_party/README.md for the
# license/version/source table required by the project brief (section 12,
# 66). Never vendor source directly into this repo; never float a branch.

include(FetchContent)

# Keep third-party build noise down and avoid building things we don't use.
set(FETCHCONTENT_QUIET ON)

# ---------------------------------------------------------------------------
# fmt — formatted logging (engine/core). Header+source, small, no transitive
# deps. spdlog deferred until we actually need async/file sinks (see
# DECISIONS.md).
# ---------------------------------------------------------------------------
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        11.0.2
    GIT_SHALLOW    TRUE
)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fmt)

# ---------------------------------------------------------------------------
# SDL3 — window/input/platform bootstrap for the client. The dedicated
# server never links this (see ARCHITECTURE.md).
# ---------------------------------------------------------------------------
if(LCU_BUILD_CLIENT)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.2.10
        GIT_SHALLOW    TRUE
    )
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
endif()

# ---------------------------------------------------------------------------
# GoogleTest — unit/integration tests.
# ---------------------------------------------------------------------------
if(LCU_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# ---------------------------------------------------------------------------
# bgfx — rendering abstraction. Fetched via the bgfx.cmake community wrapper
# (see DECISIONS.md). Large build; only pulled when actually enabled.
# ---------------------------------------------------------------------------
if(LCU_BUILD_CLIENT AND LCU_ENABLE_BGFX)
    FetchContent_Declare(
        bgfx.cmake
        GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
        GIT_TAG        v1.159.9485-575
        GIT_SHALLOW    TRUE
    )
    set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
    set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(bgfx.cmake)
endif()

# Lua (scripting, Phase 9) and a compression library (save system, Phase 3)
# are intentionally not fetched yet — added when the systems that need them
# land, per "no dependency without a concrete user" (brief section 66/86).
