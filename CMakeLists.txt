cmake_minimum_required(VERSION 3.24)

project(LiveCraftUltimate
    VERSION 0.0.1
    DESCRIPTION "Cross-platform voxel sandbox game engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Development" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(LCU_BUILD_CLIENT "Build VoxelClient (requires SDL3 + bgfx)" ON)
option(LCU_BUILD_SERVER "Build VoxelServer (headless, no GPU deps)" ON)
option(LCU_BUILD_TOOLS  "Build tools/ executables" OFF)
option(LCU_BUILD_TESTS  "Build VoxelTests (GoogleTest)" ON)
option(LCU_ENABLE_BGFX  "Enable bgfx rendering backend for the client" ON)
option(LCU_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

message(STATUS "LiveCraftUltimate configuration:")
message(STATUS "  LCU_BUILD_CLIENT     = ${LCU_BUILD_CLIENT}")
message(STATUS "  LCU_BUILD_SERVER     = ${LCU_BUILD_SERVER}")
message(STATUS "  LCU_BUILD_TOOLS      = ${LCU_BUILD_TOOLS}")
message(STATUS "  LCU_BUILD_TESTS      = ${LCU_BUILD_TESTS}")
message(STATUS "  LCU_ENABLE_BGFX      = ${LCU_ENABLE_BGFX}")
message(STATUS "  CMAKE_BUILD_TYPE     = ${CMAKE_BUILD_TYPE}")
message(STATUS "  CMAKE_SYSTEM_NAME    = ${CMAKE_SYSTEM_NAME}")
message(STATUS "  CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")

# ---------------------------------------------------------------------------
# Shared compile options (applied via lcu_apply_common_options())
# ---------------------------------------------------------------------------
include(LcuCompileOptions)

add_subdirectory(third_party)
add_subdirectory(engine)
add_subdirectory(game)

if(LCU_BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(LCU_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(LCU_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(LCU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
