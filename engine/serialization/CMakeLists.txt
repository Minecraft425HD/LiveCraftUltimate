add_library(LcuSerialization STATIC
    src/chunk_serializer.cpp
)
add_library(Lcu::Serialization ALIAS LcuSerialization)

target_include_directories(LcuSerialization PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuSerialization
    PUBLIC Lcu::Core Lcu::Voxel
    PRIVATE libzstd_static
)

lcu_apply_common_options(LcuSerialization)
