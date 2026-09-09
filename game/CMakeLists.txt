# Gameplay layer (game/blocks, game/items, ...). Empty placeholder library
# until Phase 2+ populates it with real block/item/entity code — see
# TASK_QUEUE.md. Kept as a real (if currently empty) target now so
# client/server CMakeLists.txt can link Lcu::Game unconditionally instead
# of guarding every consumer on "does game/ have content yet".
add_library(LcuGame INTERFACE)
add_library(Lcu::Game ALIAS LcuGame)
target_link_libraries(LcuGame INTERFACE Lcu::EngineCore)
