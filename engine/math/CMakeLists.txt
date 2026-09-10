# Header-only. Kept as an INTERFACE target (rather than reaching into
# engine/math/include from consumers directly) so future non-header-only
# additions (e.g. a SIMD-optimized .cpp) don't change how it's consumed.
add_library(LcuMath INTERFACE)
add_library(Lcu::Math ALIAS LcuMath)

target_include_directories(LcuMath INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuMath INTERFACE Lcu::Core)
target_compile_features(LcuMath INTERFACE cxx_std_20)
