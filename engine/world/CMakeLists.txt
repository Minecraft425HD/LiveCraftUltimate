add_library(LcuWorld STATIC
    src/world.cpp
    src/worldgen.cpp
)
add_library(Lcu::World ALIAS LcuWorld)

target_include_directories(LcuWorld PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuWorld PUBLIC Lcu::Core Lcu::Voxel)

lcu_apply_common_options(LcuWorld)
