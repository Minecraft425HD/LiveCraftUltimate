find_package(Threads REQUIRED)

add_library(LcuJobs STATIC
    src/job_system.cpp
)
add_library(Lcu::Jobs ALIAS LcuJobs)

target_include_directories(LcuJobs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuJobs PUBLIC Lcu::Core Threads::Threads)

lcu_apply_common_options(LcuJobs)
