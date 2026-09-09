add_library(LcuEcs STATIC
    src/registry.cpp
)
add_library(Lcu::Ecs ALIAS LcuEcs)

target_include_directories(LcuEcs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuEcs PUBLIC Lcu::Core)

lcu_apply_common_options(LcuEcs)
