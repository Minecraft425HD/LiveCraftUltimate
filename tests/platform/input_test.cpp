#include "lcu/platform/input.h"

#include <gtest/gtest.h>

using lcu::platform::Action;
using lcu::platform::InputState;

TEST(InputState, DefaultsToAllUp) {
    InputState state;
    EXPECT_FALSE(state.is_down(Action::MoveForward));
    EXPECT_FALSE(state.is_down(Action::Jump));
    EXPECT_FALSE(state.is_down(Action::Interact));
}

TEST(InputState, SetDownIsIndependentPerAction) {
    InputState state;
    state.set_down(Action::MoveForward, true);

    EXPECT_TRUE(state.is_down(Action::MoveForward));
    EXPECT_FALSE(state.is_down(Action::MoveBackward));
    EXPECT_FALSE(state.is_down(Action::Jump));
}

TEST(InputState, SetDownCanBeReleased) {
    InputState state;
    state.set_down(Action::Sprint, true);
    ASSERT_TRUE(state.is_down(Action::Sprint));

    state.set_down(Action::Sprint, false);
    EXPECT_FALSE(state.is_down(Action::Sprint));
}
