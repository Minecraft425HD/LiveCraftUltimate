# Camera/movement-input math depends on engine/platform's InputState, so
# (like platform/rendering) this only exists when LCU_BUILD_CLIENT is on -
# the dedicated server has no camera and never will.
add_library(LcuPlayer STATIC
    src/camera.cpp
    src/movement_input.cpp
)
add_library(Lcu::Player ALIAS LcuPlayer)

target_include_directories(LcuPlayer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuPlayer PUBLIC Lcu::Core Lcu::Math Lcu::Platform Lcu::Physics)

lcu_apply_common_options(LcuPlayer)
