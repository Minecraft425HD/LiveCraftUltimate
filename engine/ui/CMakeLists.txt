# Only meaningful with a real Renderer to draw through - bgfx's debug-text
# API (see debug_overlay.cpp) only exists once bgfx is actually enabled.
if(LCU_BUILD_CLIENT AND LCU_ENABLE_BGFX)
    add_library(LcuUi STATIC
        src/debug_overlay.cpp
    )
    add_library(Lcu::Ui ALIAS LcuUi)

    target_include_directories(LcuUi PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    # Never bgfx directly (see debug_overlay.h's doc comment) - only
    # Lcu::Rendering touches bgfx; Lcu::Platform for the shared
    # touch-control-layout data TouchInputBackend also hit-tests against.
    target_link_libraries(LcuUi PUBLIC Lcu::Core Lcu::Platform Lcu::Rendering)

    lcu_apply_common_options(LcuUi)
endif()
