# Only built when LCU_BUILD_SCRIPTING pulled in Lua/LcuScripting (see
# engine/scripting/CMakeLists.txt) - same conditional guard.
if(LCU_BUILD_SCRIPTING)
    add_library(LcuModding STATIC
        src/event_bus.cpp
        src/registry_bindings.cpp
        src/mod_loader.cpp
    )
    add_library(Lcu::Modding ALIAS LcuModding)

    target_include_directories(LcuModding PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    target_link_libraries(LcuModding PUBLIC
        Lcu::Core Lcu::Scripting Lcu::Voxel Lcu::Items
    )

    lcu_apply_common_options(LcuModding)
endif()
