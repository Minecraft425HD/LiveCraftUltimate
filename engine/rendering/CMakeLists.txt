add_library(LcuRendering STATIC
    src/renderer.cpp
    src/chunk_mesh_upload.cpp
    src/shader_program.cpp
    src/frustum.cpp
)
add_library(Lcu::Rendering ALIAS LcuRendering)

target_include_directories(LcuRendering PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# bgfx is PUBLIC (not PRIVATE): chunk_mesh_upload.h exposes bgfx handle
# types in its own public interface, so consumers of Lcu::Rendering need
# bgfx's include directories too, not just this library's own .cpp files.
# Lcu::Physics (Phase 68) is PUBLIC for the same reason: frustum.h
# exposes lcu::physics::AABB in Frustum::contains_aabb's own signature.
target_link_libraries(LcuRendering
    PUBLIC Lcu::EngineCore Lcu::Platform Lcu::Assets Lcu::Physics bgfx
)

lcu_apply_common_options(LcuRendering)
