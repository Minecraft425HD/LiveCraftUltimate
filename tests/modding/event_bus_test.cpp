#include "lcu/modding/event_bus.h"

#include <gtest/gtest.h>

#include "lcu/scripting/lua_state.h"

namespace lcu::modding {
namespace {

TEST(EventBus, SubscriberCountStartsAtZero) {
    scripting::LuaState lua;
    EventBus bus(lua);
    EXPECT_EQ(bus.subscriber_count("block_broken"), 0u);
}

TEST(EventBus, LuaCanSubscribeViaLcuSubscribe) {
    scripting::LuaState lua;
    EventBus bus(lua);
    bus.expose_to_lua();

    ASSERT_TRUE(lua.run_string(R"(
        lcu.subscribe("block_broken", function(x, y, z, id) end)
    )"));

    EXPECT_EQ(bus.subscriber_count("block_broken"), 1u);
}

TEST(EventBus, MultipleSubscribersToSameEventAreAllTracked) {
    scripting::LuaState lua;
    EventBus bus(lua);
    bus.expose_to_lua();

    ASSERT_TRUE(lua.run_string(R"(
        lcu.subscribe("block_broken", function(x, y, z, id) end)
        lcu.subscribe("block_broken", function(x, y, z, id) end)
        lcu.subscribe("block_broken", function(x, y, z, id) end)
    )"));

    EXPECT_EQ(bus.subscriber_count("block_broken"), 3u);
}

TEST(EventBus, EmitBlockBrokenCallsSubscribedHandlerWithCorrectArguments) {
    scripting::LuaState lua;
    EventBus bus(lua);
    bus.expose_to_lua();

    ASSERT_TRUE(lua.run_string(R"(
        last_x, last_y, last_z, last_id = nil, nil, nil, nil
        lcu.subscribe("block_broken", function(x, y, z, id)
            last_x, last_y, last_z, last_id = x, y, z, id
        end)
    )"));

    bus.emit_block_broken(10, -5, 20, 42);

    ASSERT_TRUE(lua.run_string(R"(
        assert(last_x == 10)
        assert(last_y == -5)
        assert(last_z == 20)
        assert(last_id == 42)
    )"));
}

TEST(EventBus, EmitBlockBrokenCallsAllSubscribersInOrder) {
    scripting::LuaState lua;
    EventBus bus(lua);
    bus.expose_to_lua();

    ASSERT_TRUE(lua.run_string(R"(
        call_order = {}
        lcu.subscribe("block_broken", function(x, y, z, id) table.insert(call_order, "first") end)
        lcu.subscribe("block_broken", function(x, y, z, id) table.insert(call_order, "second") end)
    )"));

    bus.emit_block_broken(0, 0, 0, 0);

    ASSERT_TRUE(lua.run_string(R"(
        assert(#call_order == 2)
        assert(call_order[1] == "first")
        assert(call_order[2] == "second")
    )"));
}

TEST(EventBus, EmitWithNoSubscribersDoesNotCrash) {
    scripting::LuaState lua;
    EventBus bus(lua);
    bus.expose_to_lua();
    bus.emit_block_broken(1, 2, 3, 4);
}

TEST(EventBus, ErroringHandlerDoesNotStopRemainingSubscribers) {
    scripting::LuaState lua;
    EventBus bus(lua);
    bus.expose_to_lua();

    ASSERT_TRUE(lua.run_string(R"(
        second_ran = false
        lcu.subscribe("block_broken", function(x, y, z, id) error("boom") end)
        lcu.subscribe("block_broken", function(x, y, z, id) second_ran = true end)
    )"));

    bus.emit_block_broken(0, 0, 0, 0);

    ASSERT_TRUE(lua.run_string("assert(second_ran == true)"));
}

}  // namespace
}  // namespace lcu::modding
