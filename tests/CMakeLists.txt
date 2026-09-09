include(GoogleTest)

add_executable(VoxelTests
    core/log_test.cpp
    math/vec3_test.cpp
    math/mat4_test.cpp
    debug/frame_stats_test.cpp
    voxel/chunk_test.cpp
    voxel/chunk_coord_test.cpp
    voxel/block_registry_test.cpp
    voxel/greedy_mesher_test.cpp
    jobs/job_system_test.cpp
)

target_link_libraries(VoxelTests PRIVATE
    Lcu::EngineCore
    GTest::gtest_main
)

# InputState is pure data (no SDL needed to test set_down/is_down); only
# built when engine/platform exists (LCU_BUILD_CLIENT=ON), since that's
# the only configuration LcuPlatform (and the SDL3 it links) is built in.
if(TARGET Lcu::Platform)
    target_sources(VoxelTests PRIVATE platform/input_test.cpp)
    target_link_libraries(VoxelTests PRIVATE Lcu::Platform)
endif()

# Only built when bgfx is actually enabled (LCU_BUILD_CLIENT and
# LCU_ENABLE_BGFX both ON) - these tests initialize a real (headless
# Noop-backend) bgfx context to upload GPU buffers.
if(TARGET Lcu::Rendering)
    target_sources(VoxelTests PRIVATE rendering/chunk_mesh_upload_test.cpp)
    target_link_libraries(VoxelTests PRIVATE Lcu::Rendering)
endif()

lcu_apply_common_options(VoxelTests)

gtest_discover_tests(VoxelTests)
