#pragma once

#include <array>

#include "lcu/core/types.h"
#include "lcu/ui/inventory_screen.h"

namespace lcu::ui {

// Real Minecraft crafting-table screen layout (Phase 50.3): a 3x3
// crafting grid + result slot on top, the same main storage grid +
// hotbar row inventory_screen.h already lays out below it - built from
// the same shared building blocks (InventorySlotRect/InventorySlotDisplay,
// slot size/gap constants, kInventoryMainCols/Rows, kHotbarSlotCount)
// rather than modifying inventory_screen.h's own already-tested 2x2
// grid, keeping the two screens' layouts independently sized without
// coupling one's grid dimension to the other's.
constexpr u32 kCraftingTableGridEdge = 3;                                             // 3x3
constexpr u32 kCraftingTableGridSlotCount = kCraftingTableGridEdge * kCraftingTableGridEdge;  // 9
// Which main-grid column the 3x3 crafting grid is offset from the
// panel's left edge - a real, fixed visual choice (a 3-wide grid
// roughly centered above the 9-wide main grid), same convention
// kCraftGridStartCol already establishes for the 2x2 screen.
constexpr u32 kCraftingTableGridStartCol = 3;

struct CraftingTableScreenLayout {
    std::array<InventorySlotRect, kCraftingTableGridSlotCount> grid_input{};
    InventorySlotRect result{};
    std::array<InventorySlotRect, kInventoryMainSlotCount> main_slots{};
    std::array<InventorySlotRect, kHotbarSlotCount> hotbar_slots{};
};

CraftingTableScreenLayout crafting_table_screen_layout(u32 screen_width, u32 screen_height);

// Same region/index convention as InventoryScreenHit: kGridInput's
// index is 0-8 row-major, kMainInventory's is 0-26, kHotbar's is 0-8 -
// the real caller in client/main.cpp maps kHotbar/kMainInventory onto
// the same single 36-slot player Inventory the regular inventory screen
// uses (this screen doesn't have its own separate storage), and
// kGridInput onto the workbench's own 10-slot (9 input + 1 result)
// Inventory.
enum class CraftingTableScreenRegion {
    kNone,
    kGridInput,
    kResult,
    kMainInventory,
    kHotbar,
};

struct CraftingTableScreenHit {
    CraftingTableScreenRegion region = CraftingTableScreenRegion::kNone;
    u32 index = 0;
};

CraftingTableScreenHit hit_test_crafting_table_screen(const CraftingTableScreenLayout& layout, f32 mouse_x,
                                                        f32 mouse_y);

struct CraftingTableScreenState {
    std::array<InventorySlotDisplay, kCraftingTableGridSlotCount> grid_input{};
    InventorySlotDisplay result{};
    std::array<InventorySlotDisplay, kInventoryMainSlotCount> main_slots{};
    std::array<InventorySlotDisplay, kHotbarSlotCount> hotbar_slots{};
    InventorySlotDisplay cursor{};
    f32 cursor_x = 0.0f;
    f32 cursor_y = 0.0f;
};

}  // namespace lcu::ui
