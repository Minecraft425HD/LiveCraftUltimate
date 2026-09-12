#include "lcu/assets/skin_catalog.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>

#include <stb_image.h>

#include "lcu/core/assert.h"
#include "lcu/core/log.h"

namespace lcu::assets {

namespace {

namespace fs = std::filesystem;

using SkinPixels = std::array<u8, static_cast<usize>(kSkinWidth) * kSkinHeight * 4>;

// Every real Right*<->Left* arm/leg region pair (12 total) - the
// legacy 64x32 skin format (pre-MC-1.8) has no separate left-arm/
// left-leg regions at all (see skin_texture.cpp's own kRegionRects:
// every LeftArm*/LeftLeg* region lives at y>=48, past a 64x32 file's
// real pixel data), so a legacy upload's left limbs are synthesized by
// copying the same file's own already-decoded right-limb pixels here -
// unmirrored (a real, documented simplification; real Minecraft itself
// horizontally flips this copy, see DECISIONS.md).
constexpr std::array<std::pair<SkinRegion, SkinRegion>, 12> kLegacyLimbMirror{{
    {SkinRegion::RightArmTop, SkinRegion::LeftArmTop},
    {SkinRegion::RightArmBottom, SkinRegion::LeftArmBottom},
    {SkinRegion::RightArmRight, SkinRegion::LeftArmRight},
    {SkinRegion::RightArmFront, SkinRegion::LeftArmFront},
    {SkinRegion::RightArmLeft, SkinRegion::LeftArmLeft},
    {SkinRegion::RightArmBack, SkinRegion::LeftArmBack},
    {SkinRegion::RightLegTop, SkinRegion::LeftLegTop},
    {SkinRegion::RightLegBottom, SkinRegion::LeftLegBottom},
    {SkinRegion::RightLegRight, SkinRegion::LeftLegRight},
    {SkinRegion::RightLegFront, SkinRegion::LeftLegFront},
    {SkinRegion::RightLegLeft, SkinRegion::LeftLegLeft},
    {SkinRegion::RightLegBack, SkinRegion::LeftLegBack},
}};

void copy_region(SkinPixels& pixels, SkinRegion from, SkinRegion to) {
    const SkinPixelRect src = skin_pixel_rect(from);
    const SkinPixelRect dst = skin_pixel_rect(to);
    LCU_ASSERT(src.w == dst.w && src.h == dst.h);
    for (u32 row = 0; row < src.h; ++row) {
        const usize src_offset = (static_cast<usize>(src.y + row) * kSkinWidth + src.x) * 4;
        const usize dst_offset = (static_cast<usize>(dst.y + row) * kSkinWidth + dst.x) * 4;
        std::memcpy(&pixels[dst_offset], &pixels[src_offset], static_cast<usize>(src.w) * 4);
    }
}

bool is_valid_skin_size(int width, int height) {
    return width == static_cast<int>(kSkinWidth) &&
           (height == static_cast<int>(kSkinHeight) || height == static_cast<int>(kSkinHeight) / 2);
}

}  // namespace

SkinCatalog::SkinCatalog(std::string skins_directory) : skins_directory_(std::move(skins_directory)) {
    for (u32 i = 0; i < static_cast<u32>(SkinPreset::Count); ++i) {
        SkinEntry entry;
        entry.preset = static_cast<SkinPreset>(i);
        entry.name = skin_preset_name(entry.preset);
        entry.is_builtin = true;
        entries_.push_back(std::move(entry));
    }
    refresh();
}

void SkinCatalog::refresh() {
    std::error_code error;
    if (!fs::is_directory(skins_directory_, error)) {
        return;  // No custom skins yet - real, expected first-run state.
    }

    const usize builtin_count = static_cast<usize>(SkinPreset::Count);
    for (const auto& dir_entry : fs::directory_iterator(skins_directory_, error)) {
        if (error) {
            break;
        }
        if (!dir_entry.is_regular_file()) {
            continue;
        }
        const fs::path& path = dir_entry.path();
        if (path.extension() != ".png") {
            continue;
        }
        const std::string stem = path.stem().string();
        if (stem.empty()) {
            continue;
        }
        SkinPreset unused{};
        if (parse_skin_preset_name(stem, unused)) {
            LCU_LOG_WARN("SkinCatalog::refresh: \"{}\" shares its name with a builtin preset - skipped",
                         path.string());
            continue;
        }
        if (index_of_name(stem).has_value()) {
            continue;  // Already known (from a previous refresh() or add_from_file()).
        }
        SkinEntry entry;
        entry.name = stem;
        entry.is_builtin = false;
        entry.file_path = path.string();
        entries_.push_back(std::move(entry));
    }

    std::sort(entries_.begin() + static_cast<std::ptrdiff_t>(builtin_count), entries_.end(),
              [](const SkinEntry& a, const SkinEntry& b) { return a.name < b.name; });
}

std::optional<usize> SkinCatalog::index_of_name(std::string_view name) const {
    for (usize i = 0; i < entries_.size(); ++i) {
        if (entries_[i].name == name) {
            return i;
        }
    }
    return std::nullopt;
}

SkinPixels SkinCatalog::pixels_for(const SkinEntry& entry) const {
    if (entry.is_builtin) {
        return generate_skin_pixels(entry.preset);
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* decoded = stbi_load(entry.file_path.c_str(), &width, &height, &channels, 4);
    if (decoded == nullptr) {
        LCU_LOG_WARN("SkinCatalog::pixels_for: failed to decode \"{}\" ({}) - falling back to Steve",
                     entry.file_path, stbi_failure_reason());
        return generate_skin_pixels(SkinPreset::Steve);
    }
    if (!is_valid_skin_size(width, height)) {
        LCU_LOG_WARN(
            "SkinCatalog::pixels_for: \"{}\" is no longer a valid {}x{}/{}x{} skin ({}x{}) - falling back to Steve",
            entry.file_path, kSkinWidth, kSkinHeight, kSkinWidth, kSkinHeight / 2, width, height);
        stbi_image_free(decoded);
        return generate_skin_pixels(SkinPreset::Steve);
    }

    SkinPixels pixels{};
    pixels.fill(0);
    const usize decoded_row_count = static_cast<usize>(height);
    for (usize row = 0; row < decoded_row_count; ++row) {
        std::memcpy(&pixels[row * kSkinWidth * 4], &decoded[row * static_cast<usize>(width) * 4], kSkinWidth * 4);
    }
    stbi_image_free(decoded);

    if (height == static_cast<int>(kSkinHeight) / 2) {
        for (const auto& [from, to] : kLegacyLimbMirror) {
            copy_region(pixels, from, to);
        }
    }

    return pixels;
}

SkinCatalog::AddResult SkinCatalog::add_from_file(const std::string& source_path) {
    AddResult result;

    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info(source_path.c_str(), &width, &height, &channels) == 0) {
        result.error = "not a readable image (" + std::string(stbi_failure_reason()) + ")";
        LCU_LOG_WARN("SkinCatalog::add_from_file: \"{}\" {}", source_path, result.error);
        return result;
    }
    if (!is_valid_skin_size(width, height)) {
        result.error = "must be " + std::to_string(kSkinWidth) + "x" + std::to_string(kSkinHeight) + " or " +
                        std::to_string(kSkinWidth) + "x" + std::to_string(kSkinHeight / 2) + " pixels, got " +
                        std::to_string(width) + "x" + std::to_string(height);
        LCU_LOG_WARN("SkinCatalog::add_from_file: \"{}\" {}", source_path, result.error);
        return result;
    }

    const fs::path source(source_path);
    const std::string stem = source.stem().string();
    if (stem.empty()) {
        result.error = "file has no usable name";
        LCU_LOG_WARN("SkinCatalog::add_from_file: \"{}\" {}", source_path, result.error);
        return result;
    }
    SkinPreset unused{};
    if (parse_skin_preset_name(stem, unused)) {
        result.error = "name \"" + stem + "\" conflicts with a builtin skin - rename the file and try again";
        LCU_LOG_WARN("SkinCatalog::add_from_file: {}", result.error);
        return result;
    }

    std::error_code error;
    fs::create_directories(skins_directory_, error);
    const fs::path dest = fs::path(skins_directory_) / (stem + ".png");
    fs::copy_file(source, dest, fs::copy_options::overwrite_existing, error);
    if (error) {
        result.error = "could not copy into \"" + dest.string() + "\" (" + error.message() + ")";
        LCU_LOG_WARN("SkinCatalog::add_from_file: {}", result.error);
        return result;
    }

    SkinEntry entry;
    entry.name = stem;
    entry.is_builtin = false;
    entry.file_path = dest.string();

    const auto existing = std::find_if(entries_.begin(), entries_.end(),
                                        [&](const SkinEntry& e) { return !e.is_builtin && e.name == stem; });
    if (existing != entries_.end()) {
        *existing = entry;
    } else {
        entries_.push_back(entry);
    }

    LCU_LOG_INFO("SkinCatalog::add_from_file: added \"{}\" from \"{}\"", stem, source_path);
    result.ok = true;
    result.entry = std::move(entry);
    return result;
}

}  // namespace lcu::assets
