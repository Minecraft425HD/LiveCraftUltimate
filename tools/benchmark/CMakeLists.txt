add_executable(VoxelBenchmarks
    main.cpp
)

target_link_libraries(VoxelBenchmarks PRIVATE
    Lcu::EngineCore
    Lcu::Game
    benchmark::benchmark
)

lcu_apply_common_options(VoxelBenchmarks)
