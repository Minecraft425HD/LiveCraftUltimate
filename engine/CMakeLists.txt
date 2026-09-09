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

add_library(LcuEngineCore INTERFACE)
add_library(Lcu::EngineCore ALIAS LcuEngineCore)
target_link_libraries(LcuEngineCore INTERFACE
    Lcu::Core Lcu::Math Lcu::Debug Lcu::Voxel Lcu::Jobs Lcu::World Lcu::Serialization Lcu::Physics Lcu::Items Lcu::Ecs
    Lcu::Lighting Lcu::Network Lcu::Replication
)

if(LCU_BUILD_CLIENT)
    add_subdirectory(platform)
    add_subdirectory(player)

    add_library(LcuEngine INTERFACE)
    add_library(Lcu::Engine ALIAS LcuEngine)
    target_link_libraries(LcuEngine INTERFACE Lcu::EngineCore Lcu::Platform Lcu::Player)

    if(LCU_ENABLE_BGFX)
        add_subdirectory(rendering)
        target_link_libraries(LcuEngine INTERFACE Lcu::Rendering)
        target_compile_definitions(LcuEngine INTERFACE LCU_ENABLE_BGFX)
    endif()
endif()
