add_library(LcuPhysics STATIC
    src/raycast.cpp
    src/collision.cpp
)
add_library(Lcu::Physics ALIAS LcuPhysics)

target_include_directories(LcuPhysics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuPhysics PUBLIC Lcu::Core Lcu::Math Lcu::Voxel Lcu::World)

lcu_apply_common_options(LcuPhysics)
