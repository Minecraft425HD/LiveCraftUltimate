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

TEST(InputState, MouseDeltaDefaultsToZero) {
    InputState state;
    EXPECT_FLOAT_EQ(state.mouse_delta_x(), 0.0f);
    EXPECT_FLOAT_EQ(state.mouse_delta_y(), 0.0f);
}

TEST(InputState, SetMouseDeltaIsReadBackExactly) {
    InputState state;
    state.set_mouse_delta(12.5f, -3.25f);
    EXPECT_FLOAT_EQ(state.mouse_delta_x(), 12.5f);
    EXPECT_FLOAT_EQ(state.mouse_delta_y(), -3.25f);
}

TEST(InputState, SetMouseDeltaDoesNotAffectActionState) {
    InputState state;
    state.set_down(Action::MoveForward, true);
    state.set_mouse_delta(5.0f, 5.0f);
    EXPECT_TRUE(state.is_down(Action::MoveForward));
}
