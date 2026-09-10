#include "lcu/modding/mod_loader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "lcu/scripting/lua_state.h"

namespace lcu::modding {
namespace {

namespace fs = std::filesystem;

// Creates a fresh scratch directory (mods root) for one test and removes
// it on destruction, so tests don't leak files into the real filesystem or
// interfere with each other.
class ScratchModsDirectory {
   public:
    ScratchModsDirectory() : path_(fs::temp_directory_path() / fs::path("lcu_mod_loader_test_" + unique_suffix())) {
        fs::create_directories(path_);
    }
    ~ScratchModsDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    void write_mod(const std::string& mod_name, const std::string& init_lua_source) {
        fs::path mod_dir = path_ / mod_name;
        fs::create_directories(mod_dir);
        std::ofstream file(mod_dir / "init.lua");
        file << init_lua_source;
    }

    void create_empty_mod_directory(const std::string& mod_name) { fs::create_directories(path_ / mod_name); }

    const fs::path& path() const { return path_; }

   private:
    static std::string unique_suffix() {
        static int counter = 0;
        return std::to_string(reinterpret_cast<uintptr_t>(&counter)) + "_" + std::to_string(++counter);
    }

    fs::path path_;
};

TEST(ModLoader, MissingModsDirectoryLoadsZeroMods) {
    scripting::LuaState lua;
    ModLoader loader(lua);
    EXPECT_EQ(loader.load_all("/nonexistent/mods/directory"), 0u);
}

TEST(ModLoader, EmptyModsDirectoryLoadsZeroMods) {
    ScratchModsDirectory scratch;
    scripting::LuaState lua;
    ModLoader loader(lua);
    EXPECT_EQ(loader.load_all(scratch.path().string()), 0u);
}

TEST(ModLoader, LoadsSingleModAndRunsItsInitScript) {
    ScratchModsDirectory scratch;
    scratch.write_mod("greeter", "greeted = true");

    scripting::LuaState lua;
    ModLoader loader(lua);
    EXPECT_EQ(loader.load_all(scratch.path().string()), 1u);
    EXPECT_TRUE(lua.run_string("assert(greeted == true)"));
}

TEST(ModLoader, LoadsMultipleModsSharingOneLuaState) {
    ScratchModsDirectory scratch;
    scratch.write_mod("mod_a", "mod_a_ran = true");
    scratch.write_mod("mod_b", "mod_b_ran = true");

    scripting::LuaState lua;
    ModLoader loader(lua);
    EXPECT_EQ(loader.load_all(scratch.path().string()), 2u);
    EXPECT_TRUE(lua.run_string("assert(mod_a_ran == true) assert(mod_b_ran == true)"));
}

TEST(ModLoader, ModDirectoryWithoutInitLuaIsSkipped) {
    ScratchModsDirectory scratch;
    scratch.create_empty_mod_directory("empty_mod");

    scripting::LuaState lua;
    ModLoader loader(lua);
    EXPECT_EQ(loader.load_all(scratch.path().string()), 0u);
}

TEST(ModLoader, ModWithErroringInitScriptIsSkippedButOthersStillLoad) {
    ScratchModsDirectory scratch;
    scratch.write_mod("broken_mod", "error('this mod is broken')");
    scratch.write_mod("good_mod", "good_mod_ran = true");

    scripting::LuaState lua;
    ModLoader loader(lua);
    EXPECT_EQ(loader.load_all(scratch.path().string()), 1u);
    EXPECT_TRUE(lua.run_string("assert(good_mod_ran == true)"));
}

}  // namespace
}  // namespace lcu::modding
