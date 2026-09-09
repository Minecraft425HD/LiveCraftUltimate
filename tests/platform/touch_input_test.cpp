#include "lcu/platform/touch_input.h"

#include <gtest/gtest.h>

namespace lcu::platform {
namespace {

TEST(TouchInputBackend, NoTouchesLeavesEverythingUp) {
    TouchInputBackend backend;
    InputState state;
    backend.update({}, state);

    for (usize i = 0; i < static_cast<usize>(Action::Count); ++i) {
        EXPECT_FALSE(state.is_down(static_cast<Action>(i)));
    }
}

TEST(TouchInputBackend, TouchDownAloneTriggersNoMovement) {
    // First frame of a drag: the finger has an origin but hasn't moved yet,
    // so nothing should register - matches how a real virtual joystick
    // behaves (dead center = no input).
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.2f, 0.5f}}, state);

    EXPECT_FALSE(state.is_down(Action::MoveForward));
    EXPECT_FALSE(state.is_down(Action::MoveBackward));
    EXPECT_FALSE(state.is_down(Action::MoveLeft));
    EXPECT_FALSE(state.is_down(Action::MoveRight));
}

TEST(TouchInputBackend, DraggingLeftHalfUpwardTriggersMoveForward) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.2f, 0.5f}}, state);  // touch down, left half
    backend.update({{1, 0.2f, 0.5f - TouchInputBackend::kDragDeadZone - 0.01f}}, state);  // drag up

    EXPECT_TRUE(state.is_down(Action::MoveForward));
    EXPECT_FALSE(state.is_down(Action::MoveBackward));
    EXPECT_FALSE(state.is_down(Action::MoveLeft));
    EXPECT_FALSE(state.is_down(Action::MoveRight));
}

TEST(TouchInputBackend, DraggingLeftHalfDiagonallyTriggersTwoDirections) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.2f, 0.5f}}, state);
    const f32 offset = TouchInputBackend::kDragDeadZone + 0.01f;
    backend.update({{1, 0.2f + offset, 0.5f + offset}}, state);  // drag down-right

    EXPECT_TRUE(state.is_down(Action::MoveRight));
    EXPECT_TRUE(state.is_down(Action::MoveBackward));
    EXPECT_FALSE(state.is_down(Action::MoveLeft));
    EXPECT_FALSE(state.is_down(Action::MoveForward));
}

TEST(TouchInputBackend, DragBelowDeadZoneTriggersNothing) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.2f, 0.5f}}, state);
    backend.update({{1, 0.2f, 0.5f - (TouchInputBackend::kDragDeadZone * 0.5f)}}, state);  // small drag

    EXPECT_FALSE(state.is_down(Action::MoveForward));
    EXPECT_FALSE(state.is_down(Action::MoveBackward));
}

TEST(TouchInputBackend, ReleasingTheDragFingerClearsMovement) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.2f, 0.5f}}, state);
    backend.update({{1, 0.2f, 0.5f - TouchInputBackend::kDragDeadZone - 0.01f}}, state);
    ASSERT_TRUE(state.is_down(Action::MoveForward));

    backend.update({}, state);  // finger lifted
    EXPECT_FALSE(state.is_down(Action::MoveForward));
}

TEST(TouchInputBackend, RightHalfDragDrivesLookNotMovement) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{2, 0.55f, 0.3f}}, state);  // right half, above the button cluster
    backend.update({{2, 0.55f + TouchInputBackend::kDragDeadZone + 0.01f, 0.3f}}, state);

    EXPECT_TRUE(state.is_down(Action::LookRight));
    EXPECT_FALSE(state.is_down(Action::MoveRight));
}

TEST(TouchInputBackend, IndependentMovementAndLookDragsAtTheSameTime) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.2f, 0.5f}, {2, 0.55f, 0.3f}}, state);
    backend.update(
        {
            {1, 0.2f, 0.5f - TouchInputBackend::kDragDeadZone - 0.01f},  // left finger drags up
            {2, 0.55f + TouchInputBackend::kDragDeadZone + 0.01f, 0.3f},  // right finger drags right
        },
        state);

    EXPECT_TRUE(state.is_down(Action::MoveForward));
    EXPECT_TRUE(state.is_down(Action::LookRight));
}

TEST(TouchInputBackend, JumpButtonRegisters) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.95f, 0.85f}}, state);  // inside the Jump rect
    EXPECT_TRUE(state.is_down(Action::Jump));
}

TEST(TouchInputBackend, InteractButtonRegisters) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.79f, 0.85f}}, state);  // inside the Interact rect
    EXPECT_TRUE(state.is_down(Action::Interact));
}

TEST(TouchInputBackend, ButtonTouchDoesNotAlsoStartADrag) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.95f, 0.85f}}, state);  // Jump button, right half of screen
    backend.update({{1, 0.95f - TouchInputBackend::kDragDeadZone - 0.01f, 0.85f}}, state);

    // Still just the button - a button touch never becomes a look drag,
    // even if it later "moves" while staying inside/near the button.
    EXPECT_TRUE(state.is_down(Action::Jump));
    EXPECT_FALSE(state.is_down(Action::LookLeft));
}

TEST(TouchInputBackend, ReleasedButtonClears) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.95f, 0.85f}}, state);
    ASSERT_TRUE(state.is_down(Action::Jump));

    backend.update({}, state);
    EXPECT_FALSE(state.is_down(Action::Jump));
}

TEST(TouchInputBackend, SecondFingerInSameHalfIsIgnoredForDragging) {
    TouchInputBackend backend;
    InputState state;
    backend.update({{1, 0.2f, 0.5f}}, state);  // first finger claims the movement drag
    backend.update({{1, 0.2f, 0.5f}, {3, 0.1f, 0.1f}}, state);  // second finger also lands in the left half

    // The drag stays bound to finger 1's origin - finger 3 landing doesn't
    // reset or hijack it.
    const f32 offset = TouchInputBackend::kDragDeadZone + 0.01f;
    backend.update({{1, 0.2f, 0.5f - offset}, {3, 0.1f, 0.1f}}, state);
    EXPECT_TRUE(state.is_down(Action::MoveForward));
}

}  // namespace
}  // namespace lcu::platform
