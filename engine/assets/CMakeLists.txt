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
        src/procedural_textures.cpp
        src/font_atlas.cpp
        src/skin_texture.cpp
        src/skin_catalog.cpp
    )
    add_library(Lcu::Assets ALIAS LcuAssets)

    target_include_directories(LcuAssets PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    # skin_catalog.cpp real-decodes uploaded skin PNGs via stb_image
    # (declarations only here - Lcu::StbImageImpl is the one real
    # translation unit that compiles its implementation, see
    # third_party/CMakeLists.txt).
    target_link_libraries(LcuAssets PUBLIC Lcu::Core PRIVATE Lcu::Stb Lcu::StbImageImpl)

    lcu_apply_common_options(LcuAssets)
endif()
