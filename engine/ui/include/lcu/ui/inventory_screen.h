#pragma once

#include <array>

#include "lcu/core/types.h"
#include "lcu/math/vec4.h"
#include "lcu/ui/hud.h"

namespace lcu::ui {

// Real Minecraft-shaped inventory screen layout (Phase 49.1): a 2x2
// crafting grid + result slot on top, the 3x9 main storage grid below
// it, and the same 9 hotbar slots hud.h already lays out shown again at
// the bottom - one physical hotbar, displayed in two places, same as
// Minecraft's own inventory screen. Pure layout math only (no rendering,
// no mouse handling) - mirrors hud.h's own split from its *_renderer.h
// drawing half, so this is unit-testable without bgfx/SDL.
constexpr u32 kInventoryMainCols = kHotbarSlotCount;  // 9 - same row width as the hotbar itself.
constexpr u32 kInventoryMainRows = 3;
constexpr u32 kInventoryMainSlotCount = kInventoryMainRows * kInventoryMainCols;  // 27
constexpr u32 kCraftGridEdge = 2;                                                // 2x2
constexpr u32 kCraftGridSlotCount = kCraftGridEdge * kCraftGridEdge;             // 4

constexpr f32 kInventorySlotSize = kHotbarSlotSize;
constexpr f32 kInventorySlotGap = kHotbarSlotGap;
// Extra vertical breathing room between the crafting area, the main
// grid, and the hotbar row - Minecraft's own screen visually separates
// these three sections rather than running them together.
constexpr f32 kInventorySectionGap = 8.0f;
// Horizontal gap between the crafting grid and its result slot - a
// stand-in for the arrow icon Minecraft draws there (no icon atlas
// exists yet - see DECISIONS.md, the same reasoning every other
// flat-color/no-texture UI choice in this project already follows).
constexpr f32 kCraftResultGap = 12.0f;
constexpr f32 kInventoryTopMargin = 12.0f;
// How many main-grid columns the crafting grid is offset from the
// panel's left edge - a real, fixed visual choice (roughly centered
// above the main grid), not a computed "ideal" position.
constexpr u32 kCraftGridStartCol = 3;

struct InventorySlotRect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 size = kInventorySlotSize;
};

struct InventoryScreenLayout {
    std::array<InventorySlotRect, kCraftGridSlotCount> craft_input{};
    InventorySlotRect craft_result{};
    std::array<InventorySlotRect, kInventoryMainSlotCount> main_slots{};
    std::array<InventorySlotRect, kHotbarSlotCount> hotbar_slots{};
};

InventoryScreenLayout inventory_screen_layout(u32 screen_width, u32 screen_height);

// Which section of the screen a mouse click landed in, and its index
// within that section (craft_input: 0-3 row-major; main_slots: 0-26
// row-major; hotbar_slots: 0-8 left-to-right; unused/0 for kNone and
// kCraftResult, which has only one slot). The real caller in
// client/main.cpp maps kHotbar/kMainInventory indices onto the single
// 36-slot player Inventory (hotbar 0-8 -> inventory slots 0-8,
// main_slots 0-26 -> inventory slots 9-35) and kCraftInput onto the
// separate 5-slot craft grid Inventory.
enum class InventoryScreenRegion {
    kNone,
    kCraftInput,
    kCraftResult,
    kMainInventory,
    kHotbar,
};

struct InventoryScreenHit {
    InventoryScreenRegion region = InventoryScreenRegion::kNone;
    u32 index = 0;
};

InventoryScreenHit hit_test_inventory_screen(const InventoryScreenLayout& layout, f32 mouse_x, f32 mouse_y);

// One slot's real display data for the renderer - same flat-color-icon +
// count convention hud.h's own HotbarItem already established (see
// ItemDefinition::icon_color's doc comment), reused here rather than
// inventing a second representation.
struct InventorySlotDisplay {
    bool has_item = false;
    math::Vec4 icon_color{1.0f, 1.0f, 1.0f, 1.0f};
    u32 count = 0;
};

// All real inventory-screen state for one frame, gathered in
// client/main.cpp from the real player Inventory + craft grid Inventory
// + item cursor and handed to inventory_screen_renderer.h's drawing
// functions - the same split hud.h/hud_renderer.h already established.
struct InventoryScreenState {
    std::array<InventorySlotDisplay, kCraftGridSlotCount> craft_input{};
    InventorySlotDisplay craft_result{};
    std::array<InventorySlotDisplay, kInventoryMainSlotCount> main_slots{};
    std::array<InventorySlotDisplay, kHotbarSlotCount> hotbar_slots{};
    // The stack currently picked up by the mouse cursor (Phase 49.2's
    // drag/drop), drawn following the real mouse position rather than
    // snapped to any slot.
    InventorySlotDisplay cursor{};
    f32 cursor_x = 0.0f;
    f32 cursor_y = 0.0f;
};

}  // namespace lcu::ui
