#include "lcu/ui/crafting_table_screen.h"

namespace lcu::ui {

namespace {

bool point_in_rect(f32 x, f32 y, const InventorySlotRect& rect) {
    return x >= rect.x && x < rect.x + rect.size && y >= rect.y && y < rect.y + rect.size;
}

}  // namespace

CraftingTableScreenLayout crafting_table_screen_layout(u32 screen_width, u32 screen_height) {
    CraftingTableScreenLayout layout{};

    const f32 panel_width = static_cast<f32>(kInventoryMainCols) * kInventorySlotSize +
                             static_cast<f32>(kInventoryMainCols - 1) * kInventorySlotGap;
    const f32 panel_start_x = (static_cast<f32>(screen_width) - panel_width) * 0.5f;

    const f32 grid_height = static_cast<f32>(kCraftingTableGridEdge) * kInventorySlotSize +
                             static_cast<f32>(kCraftingTableGridEdge - 1) * kInventorySlotGap;
    const f32 main_height = static_cast<f32>(kInventoryMainRows) * kInventorySlotSize +
                             static_cast<f32>(kInventoryMainRows - 1) * kInventorySlotGap;
    const f32 hotbar_height = kInventorySlotSize;
    const f32 panel_height =
        kInventoryTopMargin + grid_height + kInventorySectionGap + main_height + kInventorySectionGap + hotbar_height;
    const f32 panel_start_y = (static_cast<f32>(screen_height) - panel_height) * 0.5f;

    const f32 grid_start_x =
        panel_start_x + static_cast<f32>(kCraftingTableGridStartCol) * (kInventorySlotSize + kInventorySlotGap);
    const f32 grid_start_y = panel_start_y + kInventoryTopMargin;
    for (u32 row = 0; row < kCraftingTableGridEdge; ++row) {
        for (u32 col = 0; col < kCraftingTableGridEdge; ++col) {
            const u32 index = row * kCraftingTableGridEdge + col;
            layout.grid_input[index] = {
                grid_start_x + static_cast<f32>(col) * (kInventorySlotSize + kInventorySlotGap),
                grid_start_y + static_cast<f32>(row) * (kInventorySlotSize + kInventorySlotGap),
                kInventorySlotSize,
            };
        }
    }
    const f32 result_x = grid_start_x +
                          static_cast<f32>(kCraftingTableGridEdge) * (kInventorySlotSize + kInventorySlotGap) +
                          kCraftResultGap;
    const f32 result_y = grid_start_y + (grid_height - kInventorySlotSize) * 0.5f;
    layout.result = {result_x, result_y, kInventorySlotSize};

    const f32 main_start_y = grid_start_y + grid_height + kInventorySectionGap;
    for (u32 row = 0; row < kInventoryMainRows; ++row) {
        for (u32 col = 0; col < kInventoryMainCols; ++col) {
            const u32 index = row * kInventoryMainCols + col;
            layout.main_slots[index] = {
                panel_start_x + static_cast<f32>(col) * (kInventorySlotSize + kInventorySlotGap),
                main_start_y + static_cast<f32>(row) * (kInventorySlotSize + kInventorySlotGap),
                kInventorySlotSize,
            };
        }
    }

    const f32 hotbar_y = main_start_y + main_height + kInventorySectionGap;
    for (u32 col = 0; col < kHotbarSlotCount; ++col) {
        layout.hotbar_slots[col] = {
            panel_start_x + static_cast<f32>(col) * (kInventorySlotSize + kInventorySlotGap),
            hotbar_y,
            kInventorySlotSize,
        };
    }

    return layout;
}

CraftingTableScreenHit hit_test_crafting_table_screen(const CraftingTableScreenLayout& layout, f32 mouse_x,
                                                        f32 mouse_y) {
    for (u32 i = 0; i < kCraftingTableGridSlotCount; ++i) {
        if (point_in_rect(mouse_x, mouse_y, layout.grid_input[i])) {
            return {CraftingTableScreenRegion::kGridInput, i};
        }
    }
    if (point_in_rect(mouse_x, mouse_y, layout.result)) {
        return {CraftingTableScreenRegion::kResult, 0};
    }
    for (u32 i = 0; i < kInventoryMainSlotCount; ++i) {
        if (point_in_rect(mouse_x, mouse_y, layout.main_slots[i])) {
            return {CraftingTableScreenRegion::kMainInventory, i};
        }
    }
    for (u32 i = 0; i < kHotbarSlotCount; ++i) {
        if (point_in_rect(mouse_x, mouse_y, layout.hotbar_slots[i])) {
            return {CraftingTableScreenRegion::kHotbar, i};
        }
    }
    return {CraftingTableScreenRegion::kNone, 0};
}

}  // namespace lcu::ui
