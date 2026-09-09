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
