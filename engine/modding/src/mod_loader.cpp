#include "lcu/modding/mod_loader.h"

#include <filesystem>

#include "lcu/core/log.h"

namespace lcu::modding {

namespace fs = std::filesystem;

ModLoader::ModLoader(scripting::LuaState& lua) : lua_(lua) {}

usize ModLoader::load_all(const std::string& mods_directory) {
    std::error_code error;
    if (!fs::is_directory(mods_directory, error)) {
        LCU_LOG_WARN("Mods directory '{}' doesn't exist - no mods loaded.", mods_directory);
        return 0;
    }

    usize loaded_count = 0;
    for (const auto& entry : fs::directory_iterator(mods_directory, error)) {
        if (!entry.is_directory()) {
            continue;
        }

        const fs::path init_script = entry.path() / "init.lua";
        if (!fs::is_regular_file(init_script)) {
            LCU_LOG_WARN("Mod '{}' has no init.lua - skipped.", entry.path().filename().string());
            continue;
        }

        LCU_LOG_INFO("Loading mod '{}'...", entry.path().filename().string());
        if (lua_.run_file(init_script.string())) {
            ++loaded_count;
        } else {
            LCU_LOG_WARN("Mod '{}' failed to load (see Lua error above) - skipped.",
                         entry.path().filename().string());
        }
    }

    LCU_LOG_INFO("Loaded {} mod(s) from '{}'.", loaded_count, mods_directory);
    return loaded_count;
}

}  // namespace lcu::modding
