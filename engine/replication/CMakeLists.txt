add_library(LcuReplication STATIC
    src/position_interpolator.cpp
)
add_library(Lcu::Replication ALIAS LcuReplication)

target_include_directories(LcuReplication PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# PredictionBuffer<State, Input> is header-only (templated, like
# mesh_chunk_greedy/engine/lighting) - only PositionInterpolator has a
# .cpp, so this stays a real STATIC library rather than INTERFACE.
target_link_libraries(LcuReplication PUBLIC Lcu::Core Lcu::Math)

lcu_apply_common_options(LcuReplication)
