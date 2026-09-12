# MenuStack/hud (Phase 46/47) are pure logic - no SDL/bgfx dependency at
# all - so they build and are unit-tested in both the bgfx and non-bgfx
# configs. debug_overlay/menu_renderer/hud_renderer are real drawing
# code and need an actual Renderer to draw through, so they (and the
# Lcu::Platform/Lcu::Rendering link) are only meaningful once bgfx is
# actually enabled.
if(LCU_BUILD_CLIENT)
    add_library(LcuUi STATIC
        src/menu_stack.cpp
        src/hud.cpp
        src/inventory_screen.cpp
        src/crafting_table_screen.cpp
    )
    add_library(Lcu::Ui ALIAS LcuUi)

    target_include_directories(LcuUi PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    target_link_libraries(LcuUi PUBLIC Lcu::Core Lcu::Math)

    if(LCU_ENABLE_BGFX)
        target_sources(LcuUi PRIVATE
            src/debug_overlay.cpp
            src/menu_renderer.cpp
            src/hud_renderer.cpp
            src/inventory_screen_renderer.cpp
            src/crafting_table_screen_renderer.cpp
            src/text_renderer.cpp
        )
        # Never bgfx directly (see debug_overlay.h's doc comment) - only
        # Lcu::Rendering touches bgfx; Lcu::Platform for the shared
        # touch-control-layout data TouchInputBackend also hit-tests
        # against.
        target_link_libraries(LcuUi PUBLIC Lcu::Platform Lcu::Rendering)
    endif()

    lcu_apply_common_options(LcuUi)
endif()
