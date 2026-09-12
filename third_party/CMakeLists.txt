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
    set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
    set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
    if(LCU_BUILD_SHADER_TOOLS)
        # shaderc alone (not geometryc/texturec - no use for those yet,
        # and they'd add build time for nothing this repo needs).
        set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
        set(BGFX_BUILD_TOOLS_SHADER ON CACHE BOOL "" FORCE)
        set(BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE BOOL "" FORCE)
        set(BGFX_BUILD_TOOLS_TEXTURE OFF CACHE BOOL "" FORCE)
    else()
        set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    endif()
    FetchContent_MakeAvailable(bgfx.cmake)

    if(LCU_BUILD_SHADER_TOOLS)
        # bgfxToolUtils.cmake (bgfx_compile_shaders() and friends) is only
        # auto-included when bgfx is consumed via find_package() against
        # an installed package; we consume it via FetchContent, so pull it
        # in explicitly. BGFX_SHADER_INCLUDE_PATH is likewise only set by
        # bgfx's installed package config - set it ourselves to bgfx's own
        # src/ dir, where bgfx_shader.sh (the common shader header every
        # .sc file includes) lives.
        include(${bgfx.cmake_SOURCE_DIR}/cmake/bgfxToolUtils.cmake)
        # CACHE (not a plain set()) so this is visible from client/'s
        # CMakeLists.txt too, not just here - CMake directory-variable
        # scoping wouldn't otherwise carry a plain set() across sibling
        # add_subdirectory() calls.
        set(BGFX_SHADER_INCLUDE_PATH ${BGFX_DIR}/src CACHE PATH "bgfx shader include path" FORCE)
    endif()
endif()

# ---------------------------------------------------------------------------
# zstd — chunk save-file compression (engine/serialization, Phase 3). See
# DECISIONS.md for why zstd specifically. Static lib only; no CLI, no
# tests, no multithreading support needed for a library-only consumer.
# ---------------------------------------------------------------------------
FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG        v1.5.7
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  build/cmake
)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)

# ---------------------------------------------------------------------------
# Lua 5.4 — modding/scripting (engine/scripting, Phase 9). Official
# lua.org source via its GitHub mirror; upstream ships no CMakeLists.txt
# (it's a plain-Makefile C project) so FetchContent_Populate() just
# fetches the source, and a library target is added by hand over
# onelua.c (Lua's own supported "amalgamation" build: the whole
# interpreter/library in one translation unit, compiled with -DMAKE_LIB
# to get the embeddable library with no standalone main() - see
# DECISIONS.md). No LUA_USE_LINUX/LUA_USE_POSIX etc. are defined, so
# this builds in Lua's portable ANSI C mode on every platform without
# per-OS flags - dynamic C-module loading (`package.loadlib`) is the
# only capability that trades away, which nothing here needs (mods are
# pure Lua, loaded by our own mod loader, not compiled Lua extensions).
# ---------------------------------------------------------------------------
if(LCU_BUILD_SCRIPTING)
    FetchContent_Declare(
        lua
        GIT_REPOSITORY https://github.com/lua/lua.git
        GIT_TAG        v5.4.7
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(lua)
    if(NOT lua_POPULATED)
        FetchContent_Populate(lua)
    endif()

    add_library(LuaLib STATIC ${lua_SOURCE_DIR}/onelua.c)
    target_include_directories(LuaLib PUBLIC ${lua_SOURCE_DIR})
    target_compile_definitions(LuaLib PRIVATE MAKE_LIB)
    if(NOT WIN32)
        target_link_libraries(LuaLib PUBLIC m)
    endif()
endif()

# ---------------------------------------------------------------------------
# stb (stb_image.h + stb_image_write.h) — real PNG decode for Phase 62 skin
# file uploads (engine/assets::SkinCatalog::add_from_file) and PNG encode
# for this project's own test fixtures (tests/assets/skin_catalog_test.cpp
# writes a real temporary PNG to feed back into the decoder it's testing).
# Public-domain/MIT single-header libraries with no build system of their
# own, so FetchContent_Populate() just fetches the source and a plain
# INTERFACE include-only target exposes it - same pattern as Lua's onelua.c
# above, minus the compiled library (nothing to compile: callers include the
# .h directly and define *_IMPLEMENTATION themselves, exactly once, in
# skin_catalog.cpp). Pinned to master's tip commit (stb has no version tags
# at all - transcribed via `git ls-remote --tags`, confirming none exist).
# ---------------------------------------------------------------------------
if(LCU_BUILD_CLIENT)
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        2c980bb59875b0d32144a71867fbdebb2f77cd20
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(stb)
    if(NOT stb_POPULATED)
        FetchContent_Populate(stb)
    endif()

    add_library(Stb INTERFACE)
    add_library(Lcu::Stb ALIAS Stb)
    target_include_directories(Stb INTERFACE ${stb_SOURCE_DIR})

    # The one real translation unit that actually compiles stb_image's
    # implementation (STB_IMAGE_IMPLEMENTATION), kept in its OWN target
    # deliberately never passed through lcu_apply_common_options() - see
    # stb_image_impl.cpp's own doc comment for why. Every other target
    # only ever sees stb_image.h's declarations (via Lcu::Stb's include
    # dir), which are warning-clean.
    add_library(StbImageImpl STATIC ${CMAKE_CURRENT_SOURCE_DIR}/stb_image_impl.cpp)
    add_library(Lcu::StbImageImpl ALIAS StbImageImpl)
    target_link_libraries(StbImageImpl PUBLIC Stb)

    # Same isolation, for stb_image_write.h - this project's own test
    # fixtures (tests/assets/skin_catalog_test.cpp) encode a real PNG to
    # feed back into the decoder under test, and VoxelClient's own
    # LCU_VERIFY_SKIN headless hook (client/main.cpp) does the same to
    # stand in for a real user-selected file (the real OS file-open
    # dialog can't be scripted in a headless sandbox).
    if(LCU_BUILD_TESTS OR LCU_BUILD_CLIENT)
        add_library(StbImageWriteImpl STATIC ${CMAKE_CURRENT_SOURCE_DIR}/stb_image_write_impl.cpp)
        add_library(Lcu::StbImageWriteImpl ALIAS StbImageWriteImpl)
        target_link_libraries(StbImageWriteImpl PUBLIC Stb)
    endif()
endif()

# ---------------------------------------------------------------------------
# Google Benchmark — micro-benchmarks (tools/benchmark, Phase 11). Same
# vendor/ecosystem as GoogleTest, so it needs no separate justification -
# it's the standard tool for exactly this job. BENCHMARK_ENABLE_TESTING/
# _DOWNLOAD_DEPENDENCIES off: we don't need benchmark's own test suite, and
# it would otherwise try to fetch its own (older-pinned) copy of
# GoogleTest, duplicating the one this repo already fetches above.
# ---------------------------------------------------------------------------
if(LCU_BUILD_TOOLS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.1
        GIT_SHALLOW    TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()
