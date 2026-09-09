#include "lcu/scripting/lua_state.h"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace lcu::scripting {
namespace {

TEST(LuaState, RunStringExecutesValidCode) {
    LuaState lua;
    EXPECT_TRUE(lua.run_string("x = 1 + 2"));
}

TEST(LuaState, RunStringReturnsFalseOnSyntaxError) {
    LuaState lua;
    EXPECT_FALSE(lua.run_string("this is not lua("));
}

TEST(LuaState, RunStringReturnsFalseOnRuntimeError) {
    LuaState lua;
    EXPECT_FALSE(lua.run_string("error('boom')"));
}

TEST(LuaState, BaseTableStringMathLibrariesAreAvailable) {
    LuaState lua;
    // print (base), table.insert, string.upper, math.floor - one call each
    // from every sandboxed library, to confirm all four actually opened.
    EXPECT_TRUE(lua.run_string(R"(
        assert(type(print) == "function")
        local t = {}
        table.insert(t, 1)
        assert(#t == 1)
        assert(string.upper("a") == "A")
        assert(math.floor(1.9) == 1)
    )"));
}

TEST(LuaState, IoOsPackageAreNotAvailable) {
    LuaState lua;
    EXPECT_TRUE(lua.run_string("assert(io == nil)"));
    EXPECT_TRUE(lua.run_string("assert(os == nil)"));
    EXPECT_TRUE(lua.run_string("assert(package == nil)"));
    EXPECT_TRUE(lua.run_string("assert(require == nil)"));
}

TEST(LuaState, RunFileReturnsFalseForMissingFile) {
    LuaState lua;
    EXPECT_FALSE(lua.run_file("/nonexistent/path/does_not_exist.lua"));
}

int add_one_binding(lua_State* state) {
    auto value = static_cast<int>(luaL_checkinteger(state, 1));
    lua_pushinteger(state, value + 1);
    return 1;
}

TEST(LuaState, RegisterFunctionExposesNativeFunctionToLua) {
    LuaState lua;
    lua.register_function("add_one", add_one_binding, nullptr);
    ASSERT_TRUE(lua.run_string("result = add_one(41)"));

    lua_getglobal(lua.raw(), "result");
    EXPECT_EQ(lua_tointeger(lua.raw(), -1), 42);
    lua_pop(lua.raw(), 1);
}

int read_upvalue_binding(lua_State* state) {
    auto* value = static_cast<int*>(lua_touserdata(state, lua_upvalueindex(1)));
    lua_pushinteger(state, *value);
    return 1;
}

TEST(LuaState, RegisterFunctionPassesUserDataAsUpvalue) {
    LuaState lua;
    int captured = 1337;
    lua.register_function("read_captured", read_upvalue_binding, &captured);
    ASSERT_TRUE(lua.run_string("result = read_captured()"));

    lua_getglobal(lua.raw(), "result");
    EXPECT_EQ(lua_tointeger(lua.raw(), -1), 1337);
    lua_pop(lua.raw(), 1);
}

}  // namespace
}  // namespace lcu::scripting
