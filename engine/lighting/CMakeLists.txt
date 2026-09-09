# Header-only (like engine/math and engine/voxel's greedy_mesher.h) -
# LightStorage/propagation are templated on chunk edge length, same
# reason mesh_chunk_greedy is header-only. Kept as its own INTERFACE
# target rather than folded into engine/voxel so a future non-template
# addition (a lighting job dispatched through engine/jobs, say) has an
# obvious home without reshaping engine/voxel's target.
add_library(LcuLighting INTERFACE)
add_library(Lcu::Lighting ALIAS LcuLighting)

target_include_directories(LcuLighting INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuLighting INTERFACE Lcu::Core Lcu::Voxel)
target_compile_features(LcuLighting INTERFACE cxx_std_20)
