add_library(LcuDebug STATIC
    src/frame_stats.cpp
)
add_library(Lcu::Debug ALIAS LcuDebug)

target_include_directories(LcuDebug PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuDebug PUBLIC Lcu::EngineCore)

lcu_apply_common_options(LcuDebug)
