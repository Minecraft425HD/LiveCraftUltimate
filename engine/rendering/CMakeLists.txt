add_library(LcuRendering STATIC
    src/renderer.cpp
)
add_library(Lcu::Rendering ALIAS LcuRendering)

target_include_directories(LcuRendering PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuRendering
    PUBLIC Lcu::EngineCore Lcu::Platform
    PRIVATE bgfx
)

lcu_apply_common_options(LcuRendering)
