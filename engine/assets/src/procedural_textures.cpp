#include "lcu/assets/procedural_textures.h"

#include <algorithm>
#include <cmath>

#include "lcu/core/assert.h"

namespace lcu::assets {

namespace {

constexpr u32 kSize = kTileSize;

void set_pixel(TilePixels& px, u32 x, u32 y, u8 r, u8 g, u8 b, u8 a) {
    const usize i = (static_cast<usize>(y) * kSize + x) * 4;
    px[i + 0] = r;
    px[i + 1] = g;
    px[i + 2] = b;
    px[i + 3] = a;
}

u8 clamp_u8(f32 v) {
    return static_cast<u8>(std::clamp(v, 0.0f, 255.0f));
}

// Deterministic per-pixel hash noise (Phase 54.1's own "deterministic
// RNG per texture, fixed seed per block type" requirement) - a pure
// function of (seed, x, y), so every generate_* function below can stay
// a plain stateless function with no RNG-engine state to thread through
// or reseed; the same real "no hidden global state, reproducible byte-
// for-byte" property this project's own worldgen already established
// for `kWorldSeed`. Not cryptographic, doesn't need to be - only real
// requirement is "looks like noise, same seed always gives the same
// pixels".
f32 pixel_noise(u32 seed, u32 x, u32 y) {
    u32 h = seed * 374761393u + x * 668265263u + y * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return static_cast<f32>(h % 10000u) / 10000.0f;  // [0, 1)
}

// One noisy color channel: `base` jittered by up to +/-`amount` (a
// fraction, e.g. 0.15 for Phase 54.2's own "Noise +/-15%" grass-top
// spec), using a distinct `channel_salt` per R/G/B call so the three
// channels of the same pixel don't all jitter identically (which would
// just look like the base color scaled up/down, not real per-pixel
// noise).
u8 noisy_channel(u8 base, f32 amount, u32 seed, u32 x, u32 y, u32 channel_salt) {
    const f32 n = pixel_noise(seed + channel_salt, x, y) * 2.0f - 1.0f;  // [-1, 1)
    return clamp_u8(static_cast<f32>(base) * (1.0f + n * amount));
}

}  // namespace

TilePixels generate_grass_top() {
    TilePixels px{};
    constexpr u32 kSeed = 101;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            set_pixel(px, x, y, noisy_channel(85, 0.15f, kSeed, x, y, 1), noisy_channel(130, 0.15f, kSeed, x, y, 2),
                       noisy_channel(60, 0.15f, kSeed, x, y, 3), 255);
        }
    }
    return px;
}

TilePixels generate_grass_side() {
    TilePixels px{};
    constexpr u32 kSeed = 103;
    for (u32 x = 0; x < kSize; ++x) {
        // Real jagged green/dirt boundary (Minecraft's own grass-side
        // texture isn't a flat horizontal line) - a deterministic
        // per-column 0-2 row jitter around a real 3-row-deep green cap.
        const u32 jag = static_cast<u32>(pixel_noise(kSeed + 50, x, 0) * 3.0f);
        const u32 green_rows = 3 + jag;
        for (u32 y = 0; y < kSize; ++y) {
            if (y < green_rows) {
                set_pixel(px, x, y, noisy_channel(80, 0.15f, kSeed, x, y, 1), noisy_channel(140, 0.15f, kSeed, x, y, 2),
                           noisy_channel(60, 0.15f, kSeed, x, y, 3), 255);
            } else {
                set_pixel(px, x, y, noisy_channel(120, 0.12f, kSeed, x, y, 4),
                           noisy_channel(85, 0.12f, kSeed, x, y, 5), noisy_channel(55, 0.12f, kSeed, x, y, 6), 255);
            }
        }
    }
    return px;
}

TilePixels generate_dirt() {
    TilePixels px{};
    constexpr u32 kSeed = 102;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            u8 r = noisy_channel(120, 0.12f, kSeed, x, y, 1);
            u8 g = noisy_channel(85, 0.12f, kSeed, x, y, 2);
            u8 b = noisy_channel(55, 0.12f, kSeed, x, y, 3);
            // Real darker "clumps" - a deterministic ~12% chance per
            // pixel of a darkened blotch (Phase 54.2's own "dunklere
            // Erdklumpen").
            if (pixel_noise(kSeed + 9, x, y) < 0.12f) {
                r = clamp_u8(static_cast<f32>(r) * 0.6f);
                g = clamp_u8(static_cast<f32>(g) * 0.6f);
                b = clamp_u8(static_cast<f32>(b) * 0.6f);
            }
            set_pixel(px, x, y, r, g, b, 255);
        }
    }
    return px;
}

TilePixels generate_stone() {
    TilePixels px{};
    constexpr u32 kSeed = 104;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            u8 r = noisy_channel(130, 0.08f, kSeed, x, y, 1);
            u8 g = noisy_channel(130, 0.08f, kSeed, x, y, 2);
            u8 b = noisy_channel(130, 0.08f, kSeed, x, y, 3);
            // Real darker "veins" - a deterministic ~8% chance per
            // pixel of a darkened speckle.
            if (pixel_noise(kSeed + 9, x, y) < 0.08f) {
                r = clamp_u8(static_cast<f32>(r) * 0.55f);
                g = clamp_u8(static_cast<f32>(g) * 0.55f);
                b = clamp_u8(static_cast<f32>(b) * 0.55f);
            }
            set_pixel(px, x, y, r, g, b, 255);
        }
    }
    return px;
}

TilePixels generate_sand() {
    TilePixels px{};
    constexpr u32 kSeed = 105;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            set_pixel(px, x, y, noisy_channel(220, 0.05f, kSeed, x, y, 1), noisy_channel(205, 0.05f, kSeed, x, y, 2),
                       noisy_channel(150, 0.05f, kSeed, x, y, 3), 255);
        }
    }
    return px;
}

TilePixels generate_snow() {
    TilePixels px{};
    constexpr u32 kSeed = 106;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            set_pixel(px, x, y, noisy_channel(240, 0.03f, kSeed, x, y, 1), noisy_channel(245, 0.03f, kSeed, x, y, 2),
                       noisy_channel(250, 0.03f, kSeed, x, y, 3), 255);
        }
    }
    return px;
}

TilePixels generate_water() {
    TilePixels px{};
    constexpr u32 kSeed = 107;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            set_pixel(px, x, y, noisy_channel(50, 0.1f, kSeed, x, y, 1), noisy_channel(90, 0.1f, kSeed, x, y, 2),
                       noisy_channel(180, 0.1f, kSeed, x, y, 3), 180);
        }
    }
    return px;
}

TilePixels generate_wood_side() {
    TilePixels px{};
    constexpr u32 kSeed = 108;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            // Real vertical darker bark lines every 4th column.
            const bool stripe = (x % 4 == 0);
            set_pixel(px, x, y, noisy_channel(stripe ? 95 : 120, 0.08f, kSeed, x, y, 1),
                       noisy_channel(stripe ? 65 : 85, 0.08f, kSeed, x, y, 2),
                       noisy_channel(stripe ? 38 : 50, 0.08f, kSeed, x, y, 3), 255);
        }
    }
    return px;
}

TilePixels generate_wood_top() {
    TilePixels px{};
    constexpr u32 kSeed = 109;
    constexpr f32 kCenter = (static_cast<f32>(kSize) - 1.0f) / 2.0f;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            const f32 dx = static_cast<f32>(x) - kCenter;
            const f32 dy = static_cast<f32>(y) - kCenter;
            const f32 dist = std::sqrt(dx * dx + dy * dy);
            // Real concentric rings (Phase 54.2's own "Jahresringe") -
            // alternating bands every ~2 pixels of radius.
            const bool ring = (static_cast<u32>(dist * 2.0f) % 2u) == 0u;
            set_pixel(px, x, y, noisy_channel(ring ? 125 : 100, 0.08f, kSeed, x, y, 1),
                       noisy_channel(ring ? 90 : 68, 0.08f, kSeed, x, y, 2),
                       noisy_channel(ring ? 55 : 40, 0.08f, kSeed, x, y, 3), 255);
        }
    }
    return px;
}

TilePixels generate_leaves() {
    TilePixels px{};
    constexpr u32 kSeed = 110;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            // Real alpha-0 "holes" (Phase 54.2's own "alpha 0 fuer
            // Loecher") - a deterministic ~15% chance per pixel.
            const bool hole = pixel_noise(kSeed + 7, x, y) < 0.15f;
            set_pixel(px, x, y, noisy_channel(45, 0.2f, kSeed, x, y, 1), noisy_channel(90, 0.2f, kSeed, x, y, 2),
                       noisy_channel(35, 0.2f, kSeed, x, y, 3), hole ? 0 : 255);
        }
    }
    return px;
}

namespace {

// Shared by coal/iron ore below - a handful of real, deterministic 2x2
// pixel-blob "veins" over a stone base (real ore reads as small blobs,
// not one-pixel speckle - see each caller for the real color used).
void stamp_ore_clusters(TilePixels& px, u32 seed, u8 r, u8 g, u8 b) {
    constexpr u32 kClusterCount = 4;
    for (u32 c = 0; c < kClusterCount; ++c) {
        const auto cx = static_cast<u32>(pixel_noise(seed, c, 0) * static_cast<f32>(kSize - 2));
        const auto cy = static_cast<u32>(pixel_noise(seed, c, 1) * static_cast<f32>(kSize - 2));
        for (u32 dy = 0; dy < 2; ++dy) {
            for (u32 dx = 0; dx < 2; ++dx) {
                set_pixel(px, cx + dx, cy + dy, r, g, b, 255);
            }
        }
    }
}

}  // namespace

TilePixels generate_coal_ore() {
    TilePixels px = generate_stone();
    stamp_ore_clusters(px, 111, 25, 25, 25);
    return px;
}

TilePixels generate_iron_ore() {
    TilePixels px = generate_stone();
    stamp_ore_clusters(px, 112, 200, 120, 60);
    return px;
}

TilePixels generate_torch() {
    TilePixels px{};  // Zero-initialized = fully transparent background.
    constexpr u32 kSeed = 113;
    // Real thin vertical stem, columns 7-8 (centered), rows 6-15.
    for (u32 y = 6; y < kSize; ++y) {
        for (u32 x = 7; x <= 8; ++x) {
            set_pixel(px, x, y, noisy_channel(110, 0.1f, kSeed, x, y, 1), noisy_channel(75, 0.1f, kSeed, x, y, 2),
                       noisy_channel(45, 0.1f, kSeed, x, y, 3), 255);
        }
    }
    // Real yellow/orange flame, rows 1-5, columns 5-10 - a brighter
    // inner core (columns 6-9, rows 2+) inside a darker orange outline.
    for (u32 y = 1; y < 6; ++y) {
        for (u32 x = 5; x < 11; ++x) {
            const bool inner = (x >= 6 && x <= 9 && y >= 2);
            set_pixel(px, x, y, inner ? 255 : 220, inner ? 200 : 120, inner ? 60 : 20, 255);
        }
    }
    return px;
}

TilePixels generate_crafting_table_top() {
    TilePixels px = generate_wood_top();
    // Real "tool symbol" marks (Phase 54.2's own directive) - a simple
    // crossed-lines silhouette in one corner, a stand-in for a real
    // saw/axe icon (this project ships no real icon content, only
    // procedurally generated pixels - see DECISIONS.md).
    for (u32 i = 0; i < 6; ++i) {
        set_pixel(px, 2 + i, 2 + i, 40, 40, 40, 255);
        set_pixel(px, 7 - i, 2 + i, 40, 40, 40, 255);
    }
    return px;
}

TilePixels generate_cactus() {
    TilePixels px{};
    constexpr u32 kSeed = 115;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            const bool spine = (x % 5 == 2);
            set_pixel(px, x, y, noisy_channel(spine ? 35 : 55, 0.1f, kSeed, x, y, 1),
                       noisy_channel(spine ? 110 : 135, 0.1f, kSeed, x, y, 2),
                       noisy_channel(spine ? 35 : 50, 0.1f, kSeed, x, y, 3), 255);
        }
    }
    return px;
}

TilePixels generate_compost() {
    TilePixels px{};
    constexpr u32 kSeed = 116;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            set_pixel(px, x, y, noisy_channel(70, 0.15f, kSeed, x, y, 1), noisy_channel(45, 0.15f, kSeed, x, y, 2),
                       noisy_channel(25, 0.15f, kSeed, x, y, 3), 255);
        }
    }
    return px;
}

TilePixels generate_planks() {
    TilePixels px{};
    constexpr u32 kSeed = 117;
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            // Real horizontal darker "seams" every 4th row.
            const bool seam = (y % 4 == 0);
            set_pixel(px, x, y, noisy_channel(seam ? 100 : 150, 0.08f, kSeed, x, y, 1),
                       noisy_channel(seam ? 70 : 112, 0.08f, kSeed, x, y, 2),
                       noisy_channel(seam ? 40 : 65, 0.08f, kSeed, x, y, 3), 255);
        }
    }
    return px;
}

TilePixels generate_crack(u32 stage) {
    TilePixels px{};  // Zero-initialized = fully transparent base.
    LCU_ASSERT(stage < 10);
    constexpr u32 kSeed = 200;
    // Real, monotonically-growing crack coverage: every stage samples
    // the SAME per-pixel noise field (same kSeed, same x/y), only the
    // threshold against it grows with `stage` - so stage N's real crack
    // pixels are always a real superset of stage N-1's (nothing that
    // was cracked ever "uncracks" going up a stage), the same real
    // growing-damage look Minecraft's own break-stage overlay has,
    // without needing 10 independently hand-designed crack patterns.
    const f32 threshold = 0.06f + static_cast<f32>(stage) * 0.055f;  // stage 0: 6%, stage 9: 55.5%.
    for (u32 y = 0; y < kSize; ++y) {
        for (u32 x = 0; x < kSize; ++x) {
            if (pixel_noise(kSeed, x, y) < threshold) {
                const u8 shade = noisy_channel(20, 0.3f, kSeed + 1, x, y, 7);
                set_pixel(px, x, y, shade, shade, shade, 255);
            }
        }
    }
    return px;
}

TilePixels generate_tile(TileId tile) {
    switch (tile) {
        case TileId::GrassTop:
            return generate_grass_top();
        case TileId::GrassSide:
            return generate_grass_side();
        case TileId::Dirt:
            return generate_dirt();
        case TileId::Stone:
            return generate_stone();
        case TileId::Sand:
            return generate_sand();
        case TileId::Snow:
            return generate_snow();
        case TileId::Water:
            return generate_water();
        case TileId::WoodSide:
            return generate_wood_side();
        case TileId::WoodTop:
            return generate_wood_top();
        case TileId::Leaves:
            return generate_leaves();
        case TileId::CoalOre:
            return generate_coal_ore();
        case TileId::IronOre:
            return generate_iron_ore();
        case TileId::Torch:
            return generate_torch();
        case TileId::CraftingTableTop:
            return generate_crafting_table_top();
        case TileId::Cactus:
            return generate_cactus();
        case TileId::Compost:
            return generate_compost();
        case TileId::Planks:
            return generate_planks();
        case TileId::Crack0:
        case TileId::Crack1:
        case TileId::Crack2:
        case TileId::Crack3:
        case TileId::Crack4:
        case TileId::Crack5:
        case TileId::Crack6:
        case TileId::Crack7:
        case TileId::Crack8:
        case TileId::Crack9:
            return generate_crack(static_cast<u32>(tile) - static_cast<u32>(TileId::Crack0));
        case TileId::Count:
            break;
    }
    LCU_ASSERT(false);  // Unreachable - every real TileId above is handled.
    return TilePixels{};
}

std::vector<u8> build_block_atlas_pixels() {
    std::vector<u8> atlas(static_cast<usize>(kAtlasSize) * kAtlasSize * 4, 0);
    for (u32 i = 0; i < static_cast<u32>(TileId::Count); ++i) {
        const TilePixels tile = generate_tile(static_cast<TileId>(i));
        const u32 tx = i % kTilesPerRow;
        const u32 ty = i / kTilesPerRow;
        for (u32 y = 0; y < kTileSize; ++y) {
            for (u32 x = 0; x < kTileSize; ++x) {
                const usize src = (static_cast<usize>(y) * kTileSize + x) * 4;
                const usize dst_x = static_cast<usize>(tx) * kTileSize + x;
                const usize dst_y = static_cast<usize>(ty) * kTileSize + y;
                const usize dst = (dst_y * kAtlasSize + dst_x) * 4;
                atlas[dst + 0] = tile[src + 0];
                atlas[dst + 1] = tile[src + 1];
                atlas[dst + 2] = tile[src + 2];
                atlas[dst + 3] = tile[src + 3];
            }
        }
    }
    return atlas;
}

}  // namespace lcu::assets
