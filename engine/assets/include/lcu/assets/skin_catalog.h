#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "lcu/assets/skin_texture.h"
#include "lcu/core/types.h"

namespace lcu::assets {

// One real, selectable skin (Phase 62) - either one of the 5 always-
// available procedural SkinPreset values (Phase 58/62, see
// skin_texture.h) or a real user-uploaded PNG file living in this
// catalog's own skins directory (brief section 62.3's "Load own
// skin..." button). `name` is the one real identifier used everywhere
// a skin is referred to - a builtin's own skin_preset_name(), or a
// custom skin's uploaded-file stem (filename without ".png") - and is
// exactly what gets persisted as `skin=<name>` in options.txt (see
// lcu::platform::Options).
struct SkinEntry {
    std::string name;
    bool is_builtin = true;
    SkinPreset preset = SkinPreset::Steve;  // valid only if is_builtin
    std::string file_path;                  // valid only if !is_builtin
};

// Real, on-disk-backed list of every skin actually available to pick
// from (Phase 62) - the 5 builtin SkinPreset values, always first and
// always present at construction, followed by every real "*.png" file
// sitting in `skins_directory` (this project's own "Load own skin..."
// destination), discovered via a real std::filesystem scan - not a
// hardcoded list, so a skin dropped into that folder by hand (no
// upload dialog needed) shows up too. Same real-directory-scan
// precedent as lcu::modding::ModLoader::load_all(mods_directory) (see
// DECISIONS.md).
class SkinCatalog {
   public:
    explicit SkinCatalog(std::string skins_directory);

    // Re-scans `skins_directory` for "*.png" files, adding any new one
    // (by filename stem) as a real custom SkinEntry and leaving already
    // -known entries (builtin or custom) untouched - safe to call
    // repeatedly (e.g. every time a real Skins menu screen opens), and
    // never removes an entry even if its backing file later disappears
    // (see pixels_for()'s own fallback for that case). A stem that
    // collides with a builtin preset's own name is skipped with a
    // logged warning (see add_from_file()'s own real rejection of the
    // same case at upload time).
    void refresh();

    usize size() const { return entries_.size(); }
    const SkinEntry& entry_at(usize index) const { return entries_[index]; }

    // Real linear name lookup (a few dozen entries at most - no index
    // needed). Returns std::nullopt for an unknown name; the caller
    // (options.txt load, at startup) is expected to fall back to
    // SkinPreset::Steve itself, not this class.
    std::optional<usize> index_of_name(std::string_view name) const;

    // Real RGBA8 pixel data for `entry` - generate_skin_pixels(entry.
    // preset) for a builtin, or a real decoded PNG (via stb_image, see
    // skin_catalog.cpp) for a custom one. A custom entry whose backing
    // file is missing, unreadable, or fails to decode never crashes -
    // it's logged and this returns Steve's own default pixels instead
    // (the same real "kein Absturz bei ungueltiger Datei" requirement
    // add_from_file below also honors).
    std::array<u8, static_cast<usize>(kSkinWidth) * kSkinHeight * 4> pixels_for(const SkinEntry& entry) const;

    struct AddResult {
        bool ok = false;
        std::string error;  // human-readable reason, only set if !ok
        SkinEntry entry;    // only meaningful if ok
    };

    // Real Phase 62.3 "Load own skin..." implementation: validates
    // `source_path` (must decode via stb_image and be exactly 64x64 or
    // 64x32 pixels - the two real Minecraft skin dimensions), then
    // copies it into `skins_directory` as "<stem>.png" (creating that
    // directory first if it doesn't exist yet) and adds/updates a real
    // SkinEntry for it (re-uploading the same stem replaces the old
    // file and entry in place rather than growing a duplicate). Never
    // throws or crashes; a decode failure, a wrong-size image, a name
    // colliding with a builtin preset, or a filesystem error all come
    // back as `{ok=false, error=<reason>}`.
    AddResult add_from_file(const std::string& source_path);

   private:
    std::string skins_directory_;
    std::vector<SkinEntry> entries_;
};

}  // namespace lcu::assets
