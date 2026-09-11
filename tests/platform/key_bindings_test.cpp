#include "lcu/platform/key_bindings.h"

#include <gtest/gtest.h>

using lcu::platform::Action;
using lcu::platform::kMouseLeftKey;
using lcu::platform::kMouseMiddleKey;
using lcu::platform::kMouseRightKey;
using lcu::platform::kUnboundKey;
using lcu::platform::KeyBindings;
using lcu::platform::parse_physical_key;
using lcu::platform::physical_key_name;
using lcu::platform::PhysicalKey;

TEST(KeyBindings, DefaultsBindDistinctRealKeysToEachMovementAction) {
    // Real Minecraft-parity defaults (Phase 43): four distinct,
    // genuinely bound (not kUnboundKey) physical keys, no two movement
    // actions sharing one key. Checked via the actual bound codes
    // (bindings_for) rather than hardcoded SDL scancode-name strings, so
    // this doesn't depend on SDL's exact display text for any one key.
    KeyBindings bindings;
    const PhysicalKey forward = bindings.bindings_for(Action::MoveForward)[0];
    const PhysicalKey backward = bindings.bindings_for(Action::MoveBackward)[0];
    const PhysicalKey left = bindings.bindings_for(Action::MoveLeft)[0];
    const PhysicalKey right = bindings.bindings_for(Action::MoveRight)[0];

    for (const PhysicalKey key : {forward, backward, left, right}) {
        EXPECT_NE(key, kUnboundKey);
    }
    EXPECT_NE(forward, backward);
    EXPECT_NE(forward, left);
    EXPECT_NE(forward, right);
    EXPECT_NE(backward, left);
    EXPECT_NE(backward, right);
    EXPECT_NE(left, right);

    EXPECT_TRUE(bindings.triggers(Action::MoveForward, forward));
    EXPECT_TRUE(bindings.triggers(Action::MoveBackward, backward));
    EXPECT_TRUE(bindings.triggers(Action::MoveLeft, left));
    EXPECT_TRUE(bindings.triggers(Action::MoveRight, right));
}

TEST(KeyBindings, DefaultsPutSprintOnADifferentKeyThanCrouch) {
    // Phase 43's real fix to this project's own pre-existing backwards
    // defaults (Sprint=Shift, Crouch=Ctrl) - see DECISIONS.md. Doesn't
    // assert exact SDL scancode names (Ctrl/Shift's display text is
    // real but not worth hardcoding here); what matters and is checked:
    // they're bound to two different real keys, and each only triggers
    // its own action.
    KeyBindings bindings;
    const PhysicalKey sprint_key = bindings.bindings_for(Action::Sprint)[0];
    const PhysicalKey crouch_key = bindings.bindings_for(Action::Crouch)[0];

    EXPECT_NE(sprint_key, kUnboundKey);
    EXPECT_NE(crouch_key, kUnboundKey);
    EXPECT_NE(sprint_key, crouch_key);
    EXPECT_FALSE(bindings.triggers(Action::Sprint, crouch_key));
    EXPECT_FALSE(bindings.triggers(Action::Crouch, sprint_key));
}

TEST(KeyBindings, DefaultsBindMouseButtonsToBreakPlacePick) {
    KeyBindings bindings;
    EXPECT_TRUE(bindings.triggers(Action::Interact, kMouseLeftKey));
    EXPECT_TRUE(bindings.triggers(Action::PlaceBlock, kMouseRightKey));
    EXPECT_TRUE(bindings.triggers(Action::PickBlock, kMouseMiddleKey));
    // Real, not accidental overlap - each mouse-bound action triggers on
    // exactly its own button, not the others'.
    EXPECT_FALSE(bindings.triggers(Action::Interact, kMouseRightKey));
    EXPECT_FALSE(bindings.triggers(Action::PlaceBlock, kMouseLeftKey));
}

TEST(KeyBindings, DefaultsBindNineDistinctKeysToDirectHotbarSelection) {
    KeyBindings bindings;
    PhysicalKey keys[9];
    for (int i = 0; i < 9; ++i) {
        const auto action = static_cast<Action>(static_cast<lcu::u8>(Action::SelectHotbar1) + i);
        keys[i] = bindings.bindings_for(action)[0];
        EXPECT_NE(keys[i], kUnboundKey) << "slot " << (i + 1);
        EXPECT_TRUE(bindings.triggers(action, keys[i])) << "slot " << (i + 1);
    }
    for (int i = 0; i < 9; ++i) {
        for (int j = i + 1; j < 9; ++j) {
            EXPECT_NE(keys[i], keys[j]) << "slots " << (i + 1) << " and " << (j + 1) << " share a key";
        }
    }
}

TEST(KeyBindings, UnboundKeyNeverTriggersAnything) {
    KeyBindings bindings;
    EXPECT_FALSE(bindings.triggers(Action::MoveForward, kUnboundKey));
    EXPECT_FALSE(bindings.triggers(Action::SwapOffhand, kUnboundKey));
}

TEST(KeyBindings, BindOverwritesInPlace) {
    KeyBindings bindings;
    const PhysicalKey original = bindings.bindings_for(Action::Jump)[0];
    const PhysicalKey replacement = original + 1000;  // a synthetic, real-int, definitely-different code.
    ASSERT_FALSE(bindings.triggers(Action::Jump, replacement));

    bindings.bind(Action::Jump, 0, replacement);

    EXPECT_TRUE(bindings.triggers(Action::Jump, replacement));
    EXPECT_FALSE(bindings.triggers(Action::Jump, original)) << "rebinding slot 0 should replace the old key, not add "
                                                                 "the new one alongside it";
}

TEST(KeyBindings, BindCanAddASecondBindingInAnotherSlot) {
    KeyBindings bindings;
    const PhysicalKey original = bindings.bindings_for(Action::Jump)[0];
    const PhysicalKey extra = original + 1000;

    bindings.bind(Action::Jump, 1, extra);

    // Slot 0's real default survives; slot 1's new binding also triggers
    // the same action - a real multi-binding, not a replace.
    EXPECT_TRUE(bindings.triggers(Action::Jump, original));
    EXPECT_TRUE(bindings.triggers(Action::Jump, extra));
}

TEST(KeyBindings, ResetToDefaultsUndoesRebinding) {
    KeyBindings bindings;
    const PhysicalKey original = bindings.bindings_for(Action::MoveForward)[0];
    const PhysicalKey replacement = original + 1000;
    bindings.bind(Action::MoveForward, 0, replacement);
    ASSERT_FALSE(bindings.triggers(Action::MoveForward, original));

    bindings.reset_to_defaults();

    EXPECT_TRUE(bindings.triggers(Action::MoveForward, original));
    EXPECT_FALSE(bindings.triggers(Action::MoveForward, replacement));
}

TEST(PhysicalKey, MouseButtonNamesRoundTrip) {
    EXPECT_EQ(physical_key_name(kMouseLeftKey), "MOUSE_LEFT");
    EXPECT_EQ(physical_key_name(kMouseRightKey), "MOUSE_RIGHT");
    EXPECT_EQ(physical_key_name(kMouseMiddleKey), "MOUSE_MIDDLE");
    EXPECT_EQ(parse_physical_key("MOUSE_LEFT"), kMouseLeftKey);
    EXPECT_EQ(parse_physical_key("MOUSE_RIGHT"), kMouseRightKey);
    EXPECT_EQ(parse_physical_key("MOUSE_MIDDLE"), kMouseMiddleKey);
}

TEST(PhysicalKey, KeyboardKeyNameRoundTripsForARealDefaultBinding) {
    // Real SDL scancode name (SDL_GetScancodeName/SDL_GetScancodeFromName
    // under the hood, see key_bindings.cpp) for whatever key a fresh
    // KeyBindings actually bound to MoveForward - real round-trip
    // behavior, without this test needing to guess SDL's exact display
    // string for any specific key.
    const PhysicalKey forward_key = KeyBindings().bindings_for(Action::MoveForward)[0];
    ASSERT_NE(forward_key, kUnboundKey);
    const std::string name = physical_key_name(forward_key);
    EXPECT_FALSE(name.empty());
    EXPECT_NE(name, "UNBOUND");
    EXPECT_EQ(parse_physical_key(name), forward_key);
}

TEST(PhysicalKey, UnknownNameParsesAsUnbound) {
    EXPECT_EQ(parse_physical_key("NotARealKeyName"), kUnboundKey);
    EXPECT_EQ(parse_physical_key(""), kUnboundKey);
    EXPECT_EQ(physical_key_name(kUnboundKey), "UNBOUND");
}
