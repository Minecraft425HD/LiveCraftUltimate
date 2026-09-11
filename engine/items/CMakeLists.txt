add_library(LcuItems STATIC
    src/item_registry.cpp
    src/inventory.cpp
    src/inventory_ops.cpp
    src/recipe_registry.cpp
)
add_library(Lcu::Items ALIAS LcuItems)

target_include_directories(LcuItems PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuItems PUBLIC Lcu::Core Lcu::Math)

lcu_apply_common_options(LcuItems)
