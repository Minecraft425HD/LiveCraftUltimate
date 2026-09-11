#pragma once

#include <array>

#include "lcu/core/types.h"
#include "lcu/math/vec4.h"

namespace lcu::ui {

// Real Minecraft-position hotbar (Phase 47, brief section 60's HUD
// overhaul): 9 slots, bottom-center. Pure layout math - no rendering,
// no SDL/bgfx - so it's real and unit-tested in both the bgfx and
// non-bgfx configs, the same split menu_stack.h's layout functions
// already established for the pause menu.
constexpr u32 kHotbarSlotCount = 9;
constexpr f32 kHotbarSlotSize = 20.0f;
constexpr f32 kHotbarSlotGap = 2.0f;
constexpr f32 kHotbarIconSize = 16.0f;
constexpr f32 kHotbarBottomMargin = 6.0f;

struct HotbarSlotRect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 size = kHotbarSlotSize;
};

// One real rect per slot, bottom-center, left-to-right in slot order.
std::array<HotbarSlotRect, kHotbarSlotCount> hotbar_slot_layout(u32 screen_width, u32 screen_height);

// One hotbar slot's real display data - a flat colored icon quad (no
// texture atlas, see ItemDefinition::icon_color's own doc comment) plus
// the real held count from Inventory/placeable_items, not a mock value.
struct HotbarItem {
    bool has_item = false;
    math::Vec4 icon_color{1.0f, 1.0f, 1.0f, 1.0f};
    u32 count = 0;
};

// Real Minecraft-style 10-icon health/hunger bar (Phase 47.3 - visual
// only this phase, hardcoded full; Phase 51 wires real values in).
// Each icon represents 2 points (0..max in half-icon steps), matching
// Minecraft's own half-heart convention.
constexpr u32 kStatBarIconCount = 10;
constexpr f32 kStatBarIconSize = 8.0f;
constexpr f32 kStatBarIconGap = 1.0f;

struct StatBarIconRect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 size = kStatBarIconSize;
    // 0..1 real fill fraction for this specific icon (0.5 == a half
    // heart/drumstick) - computed here so both the empty-background
    // quad and the filled quad's real width come from one place.
    f32 fill_fraction = 1.0f;
};

// `bottom_y` is the top edge of whatever's drawn directly below this
// bar (the hotbar row, or another stat bar) - real stacking, not a
// hardcoded offset guess.
std::array<StatBarIconRect, kStatBarIconCount> stat_bar_layout(u32 screen_width, f32 bottom_y, f32 value,
                                                                f32 max_value);

// All real HUD state for one frame - hotbar contents/selection plus
// health/hunger, gathered once in client/main.cpp and handed to
// hud_renderer.h's drawing functions.
struct HudState {
    std::array<HotbarItem, kHotbarSlotCount> hotbar{};
    usize selected_hotbar_slot = 0;
    f32 health = 20.0f;
    f32 max_health = 20.0f;
    f32 hunger = 20.0f;
    f32 max_hunger = 20.0f;
};

}  // namespace lcu::ui
