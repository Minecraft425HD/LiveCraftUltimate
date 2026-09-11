#include "lcu/platform/options.h"

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

using lcu::platform::Action;
using lcu::platform::Options;

namespace {

// Same real-temp-file pattern tests/serialization/chunk_serializer_test.cpp
// already established.
std::string temp_file_path(const std::string& name) {
    return (std::filesystem::temp_directory_path() / ("lcu_test_options_" + name)).string();
}

}  // namespace

TEST(Options, DefaultsMatchTheRealHardcodedConstantsItReplaces) {
    // Phase 45's own defaults should match what client/main.cpp already
    // hardcoded before this phase (kMouseSensitivity=0.0022, no change
    // in observed behavior for anyone who never opens an options menu).
    const Options options;
    EXPECT_FLOAT_EQ(options.mouse_sensitivity, 0.0022f);
    EXPECT_EQ(options.fov, 70);
    EXPECT_TRUE(options.hud_enabled);
    EXPECT_FALSE(options.debug_overlay_enabled);
}

TEST(Options, LoadFromMissingFileReturnsFalseAndKeepsDefaults) {
    Options options;
    const bool loaded = options.load(temp_file_path("does_not_exist.txt"));

    EXPECT_FALSE(loaded);
    EXPECT_FLOAT_EQ(options.mouse_sensitivity, 0.0022f);
    EXPECT_EQ(options.fov, 70);
}

TEST(Options, SaveThenLoadRoundTripsScalarFields) {
    const std::string path = temp_file_path("roundtrip_scalars.txt");

    Options saved;
    saved.mouse_sensitivity = 0.01f;
    saved.fov = 90;
    saved.hud_enabled = false;
    saved.debug_overlay_enabled = true;
    saved.save(path);

    Options loaded;
    ASSERT_TRUE(loaded.load(path));
    EXPECT_FLOAT_EQ(loaded.mouse_sensitivity, 0.01f);
    EXPECT_EQ(loaded.fov, 90);
    EXPECT_FALSE(loaded.hud_enabled);
    EXPECT_TRUE(loaded.debug_overlay_enabled);

    std::filesystem::remove(path);
}

TEST(Options, SaveThenLoadRoundTripsRebindings) {
    const std::string path = temp_file_path("roundtrip_bindings.txt");

    Options saved;
    saved.key_bindings.bind(Action::Jump, 0, lcu::platform::parse_physical_key("K"));
    saved.key_bindings.bind(Action::Escape, 1, lcu::platform::kUnboundKey);
    saved.save(path);

    Options loaded;
    ASSERT_TRUE(loaded.load(path));
    EXPECT_TRUE(loaded.key_bindings.triggers(Action::Jump, lcu::platform::parse_physical_key("K")));
    EXPECT_FALSE(loaded.key_bindings.triggers(Action::Jump, lcu::platform::parse_physical_key("Space")))
        << "rebinding should have replaced the default, not added to it";

    std::filesystem::remove(path);
}

TEST(Options, LoadIgnoresCorruptLinesAndKeepsLoadingTheRest) {
    const std::string path = temp_file_path("corrupt.txt");
    {
        std::ofstream file(path, std::ios::trunc);
        file << "# a real comment line\n";
        file << "this line has no equals sign at all\n";
        file << "mouse_sensitivity=not_a_number\n";
        file << "fov=90\n";  // this real line must still load despite the garbage lines around it.
        file << "\n";        // blank line, also skipped.
        file << "key.jump=K\n";
    }

    Options options;
    ASSERT_TRUE(options.load(path));

    // The corrupt mouse_sensitivity line left the default untouched...
    EXPECT_FLOAT_EQ(options.mouse_sensitivity, 0.0022f);
    // ...but real lines before and after it still applied.
    EXPECT_EQ(options.fov, 90);
    EXPECT_TRUE(options.key_bindings.triggers(Action::Jump, lcu::platform::parse_physical_key("K")));

    std::filesystem::remove(path);
}

TEST(Options, LoadIgnoresUnrecognizedActionAndPhysicalKeyNames) {
    const std::string path = temp_file_path("unrecognized_names.txt");
    {
        std::ofstream file(path, std::ios::trunc);
        file << "key.not_a_real_action=W\n";
        file << "key.jump=NotARealKeyEither\n";
    }

    Options options;
    ASSERT_TRUE(options.load(path));

    // Jump's real default (Space) survives a garbage value for it.
    EXPECT_TRUE(options.key_bindings.triggers(Action::Jump, lcu::platform::parse_physical_key("Space")));

    std::filesystem::remove(path);
}

TEST(Options, SaveWritesAnAltLineOnlyWhenTheSecondSlotIsBound) {
    const std::string path = temp_file_path("alt_slot.txt");

    Options options;
    options.key_bindings.bind(Action::Jump, 1, lcu::platform::parse_physical_key("K"));
    options.save(path);

    std::ifstream file(path);
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("key.jump.alt=K"), std::string::npos);
    // Escape's own default second slot (Tab) should also appear...
    EXPECT_NE(contents.find("key.escape.alt="), std::string::npos);
    // ...but an action with only a default single binding shouldn't get
    // a spurious .alt line.
    EXPECT_EQ(contents.find("key.move_forward.alt="), std::string::npos);

    std::filesystem::remove(path);
}
