add_library(LcuNetwork STATIC
    src/address.cpp
    src/udp_socket.cpp
    src/packet_header.cpp
    src/connection.cpp
    src/fragmentation.cpp
)
add_library(Lcu::Network ALIAS LcuNetwork)

target_include_directories(LcuNetwork PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(LcuNetwork PUBLIC Lcu::Core)

# Winsock (untested in this Linux-only sandbox - see BUILD_STATUS.md).
if(WIN32)
    target_link_libraries(LcuNetwork PUBLIC ws2_32)
endif()

lcu_apply_common_options(LcuNetwork)
