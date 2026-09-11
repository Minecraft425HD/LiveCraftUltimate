add_library(LcuVoxel STATIC
    src/chunk_coord.cpp
    src/block_registry.cpp
)
add_library(Lcu::Voxel ALIAS LcuVoxel)

target_include_directories(LcuVoxel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Lcu::Math (Phase 26): BlockDefinition::color and greedy_mesher.h's
# MeshVertex both use math::Vec3 - previously an undeclared transitive
# dependency that happened to work only because every real consumer of
# LcuVoxel also linked Lcu::Math via some other path (e.g. Lcu::EngineCore
# aggregating both); block_registry.cpp itself failed to compile without
# this once BlockDefinition gained a Vec3 field, exposing the gap.
target_link_libraries(LcuVoxel PUBLIC Lcu::Core Lcu::Math)

lcu_apply_common_options(LcuVoxel)
