# Procedural texture/font content (Phase 53-57) - pure logic, zero SDL/
# bgfx dependency (same reasoning engine/ui/CMakeLists.txt gives for
# MenuStack/Hud): tile UV math and RGBA pixel generation need no GPU
# context at all, so this builds and is unit-tested in both the bgfx
# and non-bgfx configs. engine/rendering::Renderer::create_texture_from_
# pixels (bgfx-only) is the only place any of this content actually
# reaches a real GPU texture.
if(LCU_BUILD_CLIENT)
    add_library(LcuAssets STATIC
        src/texture_atlas.cpp
    )
    add_library(Lcu::Assets ALIAS LcuAssets)

    target_include_directories(LcuAssets PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    target_link_libraries(LcuAssets PUBLIC Lcu::Core)

    lcu_apply_common_options(LcuAssets)
endif()
