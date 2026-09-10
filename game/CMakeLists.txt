# Gameplay layer (game/components, game/systems, game/items, ...)
# sitting above engine/ecs per ARCHITECTURE.md's GAME -> ENGINE
# layering - gameplay component definitions (Position, AIWander), the
# systems that drive them (wandering AI, day/night cycle), and
# gameplay content wiring (BlockItemMapping, Phase 22) live here, not
# in engine/ecs or engine/items themselves, which stay generic storage/
# registries with no gameplay opinions. No SDL/bgfx dependency
# (engine-core level), so VoxelServer can use this too. game/blocks,
# game/recipes, game/biomes, game/structures, game/worldgen,
# game/entities, game/gameplay are still empty placeholder directories -
# populated once their respective phases give them real content (see
# TASK_QUEUE.md).
add_library(LcuGame STATIC
    systems/src/ai_wander_system.cpp
    systems/src/day_night_cycle.cpp
    systems/src/replication_protocol.cpp
    items/src/block_item_mapping.cpp
)
add_library(Lcu::Game ALIAS LcuGame)

target_include_directories(LcuGame PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/components/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/systems/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/items/include>
)

target_link_libraries(LcuGame PUBLIC Lcu::EngineCore Lcu::Math)

lcu_apply_common_options(LcuGame)
