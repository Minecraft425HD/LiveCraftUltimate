# Engine subsystems. Each subdirectory below is only added once it has
# real content — see PROJECT_STATE.md / TASK_QUEUE.md for what's
# implemented vs. still a directory on disk waiting for its phase.
#
# LcuEngineCore aggregates the subset of the engine with zero SDL/bgfx
# dependency, so VoxelServer can link it without ever seeing a GPU/window
# symbol (see ARCHITECTURE.md "Server has zero GPU/window dependency").

add_subdirectory(core)
add_subdirectory(math)
add_subdirectory(debug)
add_subdirectory(voxel)
add_subdirectory(jobs)
add_subdirectory(world)
add_subdirectory(serialization)
add_subdirectory(physics)
add_subdirectory(items)
add_subdirectory(ecs)
add_subdirectory(lighting)
add_subdirectory(network)
add_subdirectory(replication)
add_subdirectory(scripting)
add_subdirectory(modding)

add_library(LcuEngineCore INTERFACE)
add_library(Lcu::EngineCore ALIAS LcuEngineCore)
target_link_libraries(LcuEngineCore INTERFACE
    Lcu::Core Lcu::Math Lcu::Debug Lcu::Voxel Lcu::Jobs Lcu::World Lcu::Serialization Lcu::Physics Lcu::Items Lcu::Ecs
    Lcu::Lighting Lcu::Network Lcu::Replication
)
if(LCU_BUILD_SCRIPTING)
    target_link_libraries(LcuEngineCore INTERFACE Lcu::Scripting Lcu::Modding)
    target_compile_definitions(LcuEngineCore INTERFACE LCU_ENABLE_SCRIPTING)
endif()

if(LCU_BUILD_CLIENT)
    add_subdirectory(platform)
    add_subdirectory(player)
    add_subdirectory(audio)
    add_subdirectory(assets)

    # rendering (if enabled) before ui - engine/ui/CMakeLists.txt's own
    # bgfx-only sources link Lcu::Rendering, so that target must already
    # exist by the time ui's CMakeLists.txt runs.
    if(LCU_ENABLE_BGFX)
        add_subdirectory(rendering)
    endif()

    # MenuStack (Phase 46) is pure logic with zero SDL/bgfx dependency -
    # see engine/ui/CMakeLists.txt - so it's added here alongside
    # platform/player/audio, not gated behind LCU_ENABLE_BGFX the way it
    # used to be when engine/ui only held the bgfx-dependent debug
    # overlay.
    add_subdirectory(ui)

    add_library(LcuEngine INTERFACE)
    add_library(Lcu::Engine ALIAS LcuEngine)
    target_link_libraries(LcuEngine INTERFACE Lcu::EngineCore Lcu::Platform Lcu::Player Lcu::Audio Lcu::Assets Lcu::Ui)

    if(LCU_ENABLE_BGFX)
        target_link_libraries(LcuEngine INTERFACE Lcu::Rendering)
        target_compile_definitions(LcuEngine INTERFACE LCU_ENABLE_BGFX)
    endif()
endif()
