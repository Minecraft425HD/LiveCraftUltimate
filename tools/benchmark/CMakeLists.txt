add_executable(VoxelBenchmarks
    main.cpp
)

target_link_libraries(VoxelBenchmarks PRIVATE
    Lcu::EngineCore
    Lcu::Game
    benchmark::benchmark
)

# Only linked when bgfx is actually enabled (Lcu::Rendering, and its own
# OcclusionCuller/Frustum, only build under LCU_ENABLE_BGFX - see
# engine/rendering/CMakeLists.txt) - main.cpp's own BM_Render_
# OcclusionCulling benchmark is `#if defined(LCU_ENABLE_BGFX)`-guarded
# to match, the same convention client/main.cpp already uses.
if(TARGET Lcu::Rendering)
    target_link_libraries(VoxelBenchmarks PRIVATE Lcu::Rendering)
    # VoxelBenchmarks links Lcu::EngineCore (not the heavier Lcu::Engine
    # aggregate, which also pulls in platform/audio/assets/ui this tool
    # doesn't need), so LCU_ENABLE_BGFX isn't defined for it automatically
    # the way it is for VoxelClient - define it directly here, scoped to
    # this one target, purely to gate main.cpp's own BM_Render_
    # OcclusionCulling benchmark's includes/code.
    target_compile_definitions(VoxelBenchmarks PRIVATE LCU_ENABLE_BGFX)
endif()

lcu_apply_common_options(VoxelBenchmarks)
