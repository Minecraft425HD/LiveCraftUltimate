include(GoogleTest)

add_executable(VoxelTests
    core/log_test.cpp
    math/vec3_test.cpp
    math/mat4_test.cpp
)

target_link_libraries(VoxelTests PRIVATE
    Lcu::EngineCore
    GTest::gtest_main
)

lcu_apply_common_options(VoxelTests)

gtest_discover_tests(VoxelTests)
