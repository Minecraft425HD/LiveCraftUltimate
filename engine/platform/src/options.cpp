#include "lcu/platform/options.h"

#include <cstdlib>
#include <fstream>

#include <SDL3/SDL.h>

#include "lcu/core/log.h"

namespace lcu::platform {

namespace {

std::string trim(const std::string& s) {
    const usize begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const usize end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

// Real "parse or leave unchanged" helpers - a corrupt/non-numeric value
// (Phase 45's own "Korrupte Zeile -> ignorieren" requirement) never
// produces NaN/garbage, it just leaves whatever `out` already held.
bool parse_float(const std::string& s, f32& out) {
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    const f32 value = std::strtof(s.c_str(), &end);
    if (end != s.c_str() + s.size()) {
        return false;
    }
    out = value;
    return true;
}

bool parse_int(const std::string& s, i32& out) {
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    const long value = std::strtol(s.c_str(), &end, 10);
    if (end != s.c_str() + s.size()) {
        return false;
    }
    out = static_cast<i32>(value);
    return true;
}

bool parse_bool(const std::string& s, bool& out) {
    if (s == "true") {
        out = true;
        return true;
    }
    if (s == "false") {
        out = false;
        return true;
    }
    return false;
}

}  // namespace

bool Options::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string raw_line;
    while (std::getline(file, raw_line)) {
        const std::string line = trim(raw_line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const usize eq = line.find('=');
        if (eq == std::string::npos) {
            continue;  // corrupt line (no '=') - skip, keep loading the rest.
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        if (key == "mouse_sensitivity") {
            parse_float(value, mouse_sensitivity);
        } else if (key == "fov") {
            parse_int(value, fov);
        } else if (key == "hud_enabled") {
            parse_bool(value, hud_enabled);
        } else if (key == "debug_overlay_enabled") {
            parse_bool(value, debug_overlay_enabled);
        } else if (key.rfind("key.", 0) == 0) {
            std::string action_part = key.substr(4);
            usize slot = 0;
            constexpr const char* kAltSuffix = ".alt";
            constexpr usize kAltSuffixLen = 4;
            if (action_part.size() > kAltSuffixLen &&
                action_part.compare(action_part.size() - kAltSuffixLen, kAltSuffixLen, kAltSuffix) == 0) {
                slot = 1;
                action_part = action_part.substr(0, action_part.size() - kAltSuffixLen);
            }
            Action action{};
            if (parse_action_name(action_part, action)) {
                // A genuinely unrecognized key *name* (not "UNBOUND"
                // itself) is a corrupt value, not an intentional unbind
                // - leave this slot at whatever it already was rather
                // than force it to kUnboundKey.
                const PhysicalKey key_value = parse_physical_key(value);
                if (key_value != kUnboundKey || value == "UNBOUND") {
                    key_bindings.bind(action, slot, key_value);
                }
            }
        }
        // Any other key name: forward-compatible unknown setting, skip.
    }
    return true;
}

void Options::save(const std::string& path) const {
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        LCU_LOG_WARN("Options::save: could not open \"{}\" for writing", path);
        return;
    }

    file << "# LiveCraftUltimate options\n";
    file << "mouse_sensitivity=" << mouse_sensitivity << "\n";
    file << "fov=" << fov << "\n";
    file << "hud_enabled=" << (hud_enabled ? "true" : "false") << "\n";
    file << "debug_overlay_enabled=" << (debug_overlay_enabled ? "true" : "false") << "\n";

    for (usize i = 0; i < static_cast<usize>(Action::Count); ++i) {
        const auto action = static_cast<Action>(i);
        const auto& slots = key_bindings.bindings_for(action);
        file << "key." << action_name(action) << "=" << physical_key_name(slots[0]) << "\n";
        if (slots[1] != kUnboundKey) {
            file << "key." << action_name(action) << ".alt=" << physical_key_name(slots[1]) << "\n";
        }
    }
}

std::string Options::default_path() {
    char* pref = SDL_GetPrefPath("LiveCraftUltimate", "LiveCraftUltimate");
    if (pref == nullptr) {
        LCU_LOG_WARN("Options::default_path: SDL_GetPrefPath failed ({}), falling back to a CWD-relative path",
                     SDL_GetError());
        return "options.txt";
    }
    std::string path(pref);
    SDL_free(pref);
    return path + "options.txt";
}

}  // namespace lcu::platform
