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
