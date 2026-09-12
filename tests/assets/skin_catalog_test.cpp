#include "lcu/assets/skin_catalog.h"

#include <array>
#include <filesystem>
#include <vector>

#include <gtest/gtest.h>
#include <stb_image_write.h>

#include "lcu/assets/skin_texture.h"

using lcu::assets::generate_skin_pixels;
using lcu::assets::kSkinHeight;
using lcu::assets::kSkinWidth;
using lcu::assets::skin_pixel_rect;
using lcu::assets::SkinCatalog;
using lcu::assets::SkinPixelRect;
using lcu::assets::SkinPreset;
using lcu::assets::SkinRegion;

namespace fs = std::filesystem;

namespace {

// Writes a real, valid PNG (via stb_image_write - the same real file
// format SkinCatalog::add_from_file() itself decodes via stb_image) so
// these tests exercise the real encode/decode round trip, not a mocked
// byte buffer.
void write_png(const fs::path& path, int width, int height, const std::vector<lcu::u8>& rgba) {
    const int ok = stbi_write_png(path.string().c_str(), width, height, 4, rgba.data(), width * 4);
    ASSERT_NE(ok, 0) << "stbi_write_png failed for " << path;
}

std::vector<lcu::u8> solid_rgba(int width, int height, std::array<lcu::u8, 4> color) {
    std::vector<lcu::u8> pixels(static_cast<lcu::usize>(width) * static_cast<lcu::usize>(height) * 4);
    for (lcu::usize i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = color[0];
        pixels[i + 1] = color[1];
        pixels[i + 2] = color[2];
        pixels[i + 3] = color[3];
    }
    return pixels;
}

}  // namespace

class SkinCatalogTest : public ::testing::Test {
   protected:
    void SetUp() override {
        std::error_code error;
        skins_dir_ = fs::temp_directory_path(error) / fs::path("lcu_skin_catalog_test");
        fs::remove_all(skins_dir_, error);
        source_dir_ = fs::temp_directory_path(error) / fs::path("lcu_skin_catalog_test_src");
        fs::remove_all(source_dir_, error);
        fs::create_directories(source_dir_, error);
    }

    void TearDown() override {
        std::error_code error;
        fs::remove_all(skins_dir_, error);
        fs::remove_all(source_dir_, error);
    }

    fs::path skins_dir_;
    fs::path source_dir_;
};

TEST_F(SkinCatalogTest, ConstructorListsAllBuiltinPresetsFirstWithNoSkinsDirectoryYet) {
    SkinCatalog catalog(skins_dir_.string());
    ASSERT_EQ(catalog.size(), static_cast<lcu::usize>(SkinPreset::Count));
    for (lcu::u32 i = 0; i < static_cast<lcu::u32>(SkinPreset::Count); ++i) {
        const auto& entry = catalog.entry_at(i);
        EXPECT_TRUE(entry.is_builtin) << "index " << i;
        EXPECT_EQ(entry.preset, static_cast<SkinPreset>(i)) << "index " << i;
    }
}

TEST_F(SkinCatalogTest, PixelsForBuiltinMatchesGenerateSkinPixelsDirectly) {
    SkinCatalog catalog(skins_dir_.string());
    const auto index = catalog.index_of_name("Alex");
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(catalog.pixels_for(catalog.entry_at(*index)), generate_skin_pixels(SkinPreset::Alex));
}

TEST_F(SkinCatalogTest, AddFromFileRejectsAnUnreadableFile) {
    SkinCatalog catalog(skins_dir_.string());
    const auto result = catalog.add_from_file((source_dir_ / "does_not_exist.png").string());
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(catalog.size(), static_cast<lcu::usize>(SkinPreset::Count));
}

TEST_F(SkinCatalogTest, AddFromFileRejectsTheWrongDimensions) {
    const fs::path source = source_dir_ / "too_small.png";
    write_png(source, 32, 32, solid_rgba(32, 32, {200, 50, 50, 255}));

    SkinCatalog catalog(skins_dir_.string());
    const auto result = catalog.add_from_file(source.string());
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(catalog.size(), static_cast<lcu::usize>(SkinPreset::Count));
}

TEST_F(SkinCatalogTest, AddFromFileRejectsANameCollidingWithABuiltinPreset) {
    const fs::path source = source_dir_ / "Alex.png";
    write_png(source, static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight),
              solid_rgba(static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight), {10, 20, 30, 255}));

    SkinCatalog catalog(skins_dir_.string());
    const auto result = catalog.add_from_file(source.string());
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(catalog.size(), static_cast<lcu::usize>(SkinPreset::Count));
}

TEST_F(SkinCatalogTest, AddFromFileAcceptsAReal64x64FileAndSelectsItImmediately) {
    const fs::path source = source_dir_ / "MyCoolSkin.png";
    write_png(source, static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight),
              solid_rgba(static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight), {11, 22, 33, 255}));

    SkinCatalog catalog(skins_dir_.string());
    const auto result = catalog.add_from_file(source.string());
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.entry.name, "MyCoolSkin");
    EXPECT_FALSE(result.entry.is_builtin);
    EXPECT_TRUE(fs::exists(result.entry.file_path));

    EXPECT_EQ(catalog.size(), static_cast<lcu::usize>(SkinPreset::Count) + 1);
    const auto index = catalog.index_of_name("MyCoolSkin");
    ASSERT_TRUE(index.has_value());

    const auto pixels = catalog.pixels_for(catalog.entry_at(*index));
    EXPECT_EQ(pixels[0], 11);
    EXPECT_EQ(pixels[1], 22);
    EXPECT_EQ(pixels[2], 33);
    EXPECT_EQ(pixels[3], 255);
}

TEST_F(SkinCatalogTest, AddFromFileAcceptsARealLegacy64x32File) {
    const fs::path source = source_dir_ / "LegacySkin.png";
    write_png(source, static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight) / 2,
              solid_rgba(static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight) / 2, {44, 55, 66, 255}));

    SkinCatalog catalog(skins_dir_.string());
    const auto result = catalog.add_from_file(source.string());
    ASSERT_TRUE(result.ok) << result.error;

    const auto pixels = catalog.pixels_for(result.entry);
    // Top 32 rows (head/torso/right-arm/right-leg regions) decode
    // directly from the real file.
    EXPECT_EQ(pixels[0], 44);
    EXPECT_EQ(pixels[1], 55);
    EXPECT_EQ(pixels[2], 66);
}

TEST_F(SkinCatalogTest, ReuploadingTheSameStemReplacesTheEntryInPlaceRatherThanDuplicating) {
    SkinCatalog catalog(skins_dir_.string());

    const fs::path first_source = source_dir_ / "SameName.png";
    write_png(first_source, static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight),
              solid_rgba(static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight), {255, 0, 0, 255}));
    const auto first_result = catalog.add_from_file(first_source.string());
    ASSERT_TRUE(first_result.ok) << first_result.error;
    const lcu::usize size_after_first = catalog.size();

    const fs::path second_source = source_dir_ / "SameName.png";  // overwrite in place with new content
    write_png(second_source, static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight),
              solid_rgba(static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight), {0, 0, 255, 255}));
    const auto second_result = catalog.add_from_file(second_source.string());
    ASSERT_TRUE(second_result.ok) << second_result.error;

    EXPECT_EQ(catalog.size(), size_after_first);  // no duplicate entry
    const auto index = catalog.index_of_name("SameName");
    ASSERT_TRUE(index.has_value());
    const auto pixels = catalog.pixels_for(catalog.entry_at(*index));
    EXPECT_EQ(pixels[0], 0);
    EXPECT_EQ(pixels[2], 255);  // real second upload's content, not the first
}

TEST_F(SkinCatalogTest, PixelsForAMissingCustomFileFallsBackToSteveWithoutCrashing) {
    const fs::path source = source_dir_ / "Vanishing.png";
    write_png(source, static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight),
              solid_rgba(static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight), {9, 9, 9, 255}));

    SkinCatalog catalog(skins_dir_.string());
    const auto result = catalog.add_from_file(source.string());
    ASSERT_TRUE(result.ok) << result.error;

    std::error_code error;
    fs::remove(result.entry.file_path, error);  // real file now gone

    const auto pixels = catalog.pixels_for(result.entry);
    EXPECT_EQ(pixels, generate_skin_pixels(SkinPreset::Steve));
}

TEST_F(SkinCatalogTest, RefreshDiscoversAFileDroppedDirectlyIntoTheSkinsDirectory) {
    std::error_code error;
    fs::create_directories(skins_dir_, error);
    write_png(skins_dir_ / "HandPlaced.png", static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight),
              solid_rgba(static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight), {1, 2, 3, 255}));

    SkinCatalog catalog(skins_dir_.string());  // constructor already calls refresh() once
    EXPECT_TRUE(catalog.index_of_name("HandPlaced").has_value());
}

TEST_F(SkinCatalogTest, RefreshSkipsAFileNamedLikeABuiltinPreset) {
    std::error_code error;
    fs::create_directories(skins_dir_, error);
    write_png(skins_dir_ / "Cyan.png", static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight),
              solid_rgba(static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight), {1, 2, 3, 255}));

    SkinCatalog catalog(skins_dir_.string());
    EXPECT_EQ(catalog.size(), static_cast<lcu::usize>(SkinPreset::Count));
}

TEST_F(SkinCatalogTest, LegacyFormatMirrorsRealRightLimbPixelsIntoLeftLimbRegions) {
    std::vector<lcu::u8> pixels =
        solid_rgba(static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight) / 2, {0, 0, 0, 0});

    const auto paint = [&](SkinRegion region, std::array<lcu::u8, 4> color) {
        const SkinPixelRect rect = skin_pixel_rect(region);
        for (lcu::u32 y = rect.y; y < rect.y + rect.h; ++y) {
            for (lcu::u32 x = rect.x; x < rect.x + rect.w; ++x) {
                const lcu::usize offset = (static_cast<lcu::usize>(y) * kSkinWidth + x) * 4;
                pixels[offset + 0] = color[0];
                pixels[offset + 1] = color[1];
                pixels[offset + 2] = color[2];
                pixels[offset + 3] = color[3];
            }
        }
    };
    paint(SkinRegion::RightArmFront, {77, 88, 99, 255});
    paint(SkinRegion::RightLegFront, {21, 32, 43, 255});

    const fs::path source = source_dir_ / "LegacyMirror.png";
    write_png(source, static_cast<int>(kSkinWidth), static_cast<int>(kSkinHeight) / 2, pixels);

    SkinCatalog catalog(skins_dir_.string());
    const auto result = catalog.add_from_file(source.string());
    ASSERT_TRUE(result.ok) << result.error;

    const auto decoded = catalog.pixels_for(result.entry);
    const auto sample = [&](SkinRegion region) {
        const SkinPixelRect rect = skin_pixel_rect(region);
        const lcu::usize offset = (static_cast<lcu::usize>(rect.y) * kSkinWidth + rect.x) * 4;
        return std::array<lcu::u8, 4>{decoded[offset + 0], decoded[offset + 1], decoded[offset + 2],
                                       decoded[offset + 3]};
    };
    EXPECT_EQ(sample(SkinRegion::LeftArmFront), (std::array<lcu::u8, 4>{77, 88, 99, 255}));
    EXPECT_EQ(sample(SkinRegion::LeftLegFront), (std::array<lcu::u8, 4>{21, 32, 43, 255}));
}
