include(GoogleTest)

add_executable(VoxelTests
    core/log_test.cpp
    core/quality_profile_test.cpp
    math/vec3_test.cpp
    math/mat4_test.cpp
    debug/frame_stats_test.cpp
    voxel/chunk_test.cpp
    voxel/chunk_coord_test.cpp
    voxel/block_registry_test.cpp
    voxel/greedy_mesher_test.cpp
    voxel/break_progress_test.cpp
    jobs/job_system_test.cpp
    world/world_test.cpp
    world/worldgen_test.cpp
    serialization/chunk_serializer_test.cpp
    physics/raycast_test.cpp
    physics/collision_test.cpp
    items/item_registry_test.cpp
    items/inventory_test.cpp
    items/inventory_ops_test.cpp
    items/recipe_registry_test.cpp
    ecs/registry_test.cpp
    lighting/propagation_test.cpp
    lighting/world_light_test.cpp
    game/systems/ai_wander_system_test.cpp
    game/systems/day_night_cycle_test.cpp
    game/systems/replication_protocol_test.cpp
    game/systems/item_entity_system_test.cpp
    game/items/block_item_mapping_test.cpp
    network/sequence_test.cpp
    network/packet_header_test.cpp
    network/connection_test.cpp
    network/udp_socket_test.cpp
    network/loopback_integration_test.cpp
    network/fragmentation_test.cpp
    replication/position_interpolator_test.cpp
    replication/prediction_buffer_test.cpp
)

# Only built when LCU_BUILD_SCRIPTING pulled in Lua - see
# engine/scripting/CMakeLists.txt and engine/modding/CMakeLists.txt.
if(TARGET Lcu::Scripting)
    target_sources(VoxelTests PRIVATE
        scripting/lua_state_test.cpp
        modding/event_bus_test.cpp
        modding/registry_bindings_test.cpp
        modding/mod_loader_test.cpp
    )
    target_link_libraries(VoxelTests PRIVATE Lcu::Scripting Lcu::Modding)
endif()

target_link_libraries(VoxelTests PRIVATE
    Lcu::EngineCore
    Lcu::Game
    GTest::gtest_main
)

# InputState is pure data (no SDL needed to test set_down/is_down); only
# built when engine/platform exists (LCU_BUILD_CLIENT=ON), since that's
# the only configuration LcuPlatform (and the SDL3 it links) is built in.
if(TARGET Lcu::Platform)
    target_sources(VoxelTests PRIVATE platform/input_test.cpp platform/touch_input_test.cpp
                                       platform/key_bindings_test.cpp platform/options_test.cpp)
    target_link_libraries(VoxelTests PRIVATE Lcu::Platform)
endif()

# Camera/movement-input math; only built alongside engine/platform (see
# engine/player/CMakeLists.txt).
if(TARGET Lcu::Player)
    target_sources(VoxelTests PRIVATE player/camera_test.cpp player/movement_input_test.cpp)
    target_link_libraries(VoxelTests PRIVATE Lcu::Player)
endif()

# Only built when bgfx is actually enabled (LCU_BUILD_CLIENT and
# LCU_ENABLE_BGFX both ON) - these tests initialize a real (headless
# Noop-backend) bgfx context to upload GPU buffers.
if(TARGET Lcu::Rendering)
    target_sources(VoxelTests PRIVATE rendering/chunk_mesh_upload_test.cpp rendering/quad_batch_2d_test.cpp)
    target_link_libraries(VoxelTests PRIVATE Lcu::Rendering)
endif()

# MenuStack (Phase 46) is pure logic - no bgfx needed to test its
# navigation/layout/hit-testing - so this is built alongside
# Lcu::Platform's own tests, in both the bgfx and non-bgfx configs (see
# engine/ui/CMakeLists.txt).
if(TARGET Lcu::Ui)
    target_sources(VoxelTests PRIVATE ui/menu_stack_test.cpp ui/hud_test.cpp ui/inventory_screen_test.cpp
                                       ui/crafting_table_screen_test.cpp)
    target_link_libraries(VoxelTests PRIVATE Lcu::Ui)
endif()

# Waveform/positional-audio math is pure logic (no real audio device
# needed); only built alongside engine/audio (LCU_BUILD_CLIENT - see
# engine/audio/CMakeLists.txt).
if(TARGET Lcu::Audio)
    target_sources(VoxelTests PRIVATE audio/waveform_test.cpp audio/positional_test.cpp)
    target_link_libraries(VoxelTests PRIVATE Lcu::Audio)
endif()

lcu_apply_common_options(VoxelTests)

gtest_discover_tests(VoxelTests)
