add_executable(VoxelClient
    main.cpp
)

target_link_libraries(VoxelClient PRIVATE
    Lcu::Engine
    Lcu::Game
)

lcu_apply_common_options(VoxelClient)

# SDL3 (shared lib build, see third_party/CMakeLists.txt) needs to be next
# to the executable at run time on platforms without an rpath/install step
# wired up yet.
if(WIN32)
    add_custom_command(TARGET VoxelClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:VoxelClient>
    )
endif()

# Compiled chunk shaders: only when shaderc was actually built
# (LCU_BUILD_SHADER_TOOLS, opt-in - see BUILDING.md). Without this,
# VoxelClient still runs fine; it just has no program to draw the
# uploaded chunk mesh with (see chunk_mesh_upload.h).
if(LCU_ENABLE_BGFX AND LCU_BUILD_SHADER_TOOLS AND TARGET bgfx::shaderc)
    set(LCU_CHUNK_SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/chunk)
    # bgfx_compile_shaders() resolves SHADERS to an absolute path itself,
    # but passes VARYING_DEF through unresolved into a custom command
    # that runs with the *build* directory as its working directory - a
    # relative path here silently fails to parse (shaderc still runs,
    # just without the varying info it needs, and errors out deep in
    # HLSL parsing instead of "file not found"). Pass it absolute.
    set(LCU_CHUNK_VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying.def.sc)

    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS shaders/vs_chunk.sc
        VARYING_DEF ${LCU_CHUNK_VARYING_DEF}
        OUTPUT_DIR ${LCU_CHUNK_SHADER_OUTPUT_DIR}
        OUT_FILES_VAR LCU_CHUNK_VS_OUTPUTS
    )
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS shaders/fs_chunk.sc
        VARYING_DEF ${LCU_CHUNK_VARYING_DEF}
        OUTPUT_DIR ${LCU_CHUNK_SHADER_OUTPUT_DIR}
        OUT_FILES_VAR LCU_CHUNK_FS_OUTPUTS
    )

    add_custom_target(VoxelClientShaders DEPENDS ${LCU_CHUNK_VS_OUTPUTS} ${LCU_CHUNK_FS_OUTPUTS})
    add_dependencies(VoxelClient VoxelClientShaders)
    target_compile_definitions(VoxelClient PRIVATE LCU_HAS_CHUNK_SHADERS)
endif()
