# Only built when LCU_BUILD_SCRIPTING pulled in Lua (see
# third_party/CMakeLists.txt) - the same conditional guard, so this
# subdirectory is simply skipped rather than failing to find LuaLib when
# scripting is disabled.
if(LCU_BUILD_SCRIPTING)
    add_library(LcuScripting STATIC
        src/lua_state.cpp
    )
    add_library(Lcu::Scripting ALIAS LcuScripting)

    target_include_directories(LcuScripting PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    # PUBLIC, not PRIVATE: LuaState::raw() intentionally hands out the
    # underlying lua_State* for binding-function implementations
    # elsewhere (see engine/modding) - those consumers need Lua's own
    # headers too, so LuaLib's include path/link should propagate to
    # anything linking Lcu::Scripting, not stop here.
    target_link_libraries(LcuScripting PUBLIC Lcu::Core LuaLib)

    lcu_apply_common_options(LcuScripting)
endif()
