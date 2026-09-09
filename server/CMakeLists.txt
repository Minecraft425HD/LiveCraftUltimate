add_executable(VoxelServer
    main.cpp
)

# Deliberately links only the headless engine subset. Never add Lcu::Engine,
# Lcu::Platform, SDL3, or bgfx here — see ARCHITECTURE.md "Server has zero
# GPU/window dependency".
target_link_libraries(VoxelServer PRIVATE
    Lcu::EngineCore
    Lcu::Game
)

lcu_apply_common_options(VoxelServer)

# See client/CMakeLists.txt's identical mods/ copy for why.
if(LCU_BUILD_SCRIPTING)
    add_custom_command(TARGET VoxelServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/mods
            $<TARGET_FILE_DIR:VoxelServer>/mods
    )
endif()
