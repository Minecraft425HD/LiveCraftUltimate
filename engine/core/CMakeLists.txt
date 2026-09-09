add_library(LcuCore STATIC
    src/log.cpp
    src/assert.cpp
    src/quality_profile.cpp
)
add_library(Lcu::Core ALIAS LcuCore)

target_include_directories(LcuCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuCore PUBLIC fmt::fmt)

if(CMAKE_BUILD_TYPE STREQUAL "Development")
    target_compile_definitions(LcuCore PUBLIC LCU_DEVELOPMENT_BUILD)
endif()

lcu_apply_common_options(LcuCore)
