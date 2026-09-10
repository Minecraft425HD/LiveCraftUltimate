# Gameplay layer (game/components, game/systems, ...) sitting above
# engine/ecs per ARCHITECTURE.md's GAME -> ENGINE layering - gameplay
# component definitions (Position, AIWander) and the systems that drive
# them (wandering AI, day/night cycle) live here, not in engine/ecs
# itself, which stays generic storage with no gameplay opinions. No
# SDL/bgfx dependency (engine-core level), so VoxelServer can use this
# too once it simulates entities. game/blocks, game/items, game/recipes,
# game/biomes, game/structures, game/worldgen, game/entities,
# game/gameplay are still empty placeholder directories - populated once
# their respective phases give them real content (see TASK_QUEUE.md).
add_library(LcuGame STATIC
    systems/src/ai_wander_system.cpp
    systems/src/day_night_cycle.cpp
    systems/src/replication_protocol.cpp
)
add_library(Lcu::Game ALIAS LcuGame)

target_include_directories(LcuGame PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/components/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/systems/include>
)

target_link_libraries(LcuGame PUBLIC Lcu::EngineCore)

lcu_apply_common_options(LcuGame)
