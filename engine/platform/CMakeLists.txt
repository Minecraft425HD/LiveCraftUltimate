add_library(LcuPlatform STATIC
    src/window.cpp
    src/native_handle.cpp
    src/input.cpp
)
add_library(Lcu::Platform ALIAS LcuPlatform)

target_include_directories(LcuPlatform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuPlatform
    PUBLIC Lcu::EngineCore
    PRIVATE SDL3::SDL3
)

lcu_apply_common_options(LcuPlatform)
