add_library(LcuVoxel STATIC
    src/chunk_coord.cpp
    src/block_registry.cpp
)
add_library(Lcu::Voxel ALIAS LcuVoxel)

target_include_directories(LcuVoxel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuVoxel PUBLIC Lcu::Core)

lcu_apply_common_options(LcuVoxel)
