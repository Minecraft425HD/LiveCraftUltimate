#include "lcu/ui/inventory_screen.h"

namespace lcu::ui {

namespace {

bool point_in_rect(f32 x, f32 y, const InventorySlotRect& rect) {
    return x >= rect.x && x < rect.x + rect.size && y >= rect.y && y < rect.y + rect.size;
}

}  // namespace

InventoryScreenLayout inventory_screen_layout(u32 screen_width, u32 screen_height) {
    InventoryScreenLayout layout{};

    const f32 panel_width = static_cast<f32>(kInventoryMainCols) * kInventorySlotSize +
                             static_cast<f32>(kInventoryMainCols - 1) * kInventorySlotGap;
    const f32 panel_start_x = (static_cast<f32>(screen_width) - panel_width) * 0.5f;

    const f32 craft_height =
        static_cast<f32>(kCraftGridEdge) * kInventorySlotSize + static_cast<f32>(kCraftGridEdge - 1) * kInventorySlotGap;
    const f32 main_height = static_cast<f32>(kInventoryMainRows) * kInventorySlotSize +
                             static_cast<f32>(kInventoryMainRows - 1) * kInventorySlotGap;
    const f32 hotbar_height = kInventorySlotSize;
    const f32 panel_height =
        kInventoryTopMargin + craft_height + kInventorySectionGap + main_height + kInventorySectionGap + hotbar_height;
    const f32 panel_start_y = (static_cast<f32>(screen_height) - panel_height) * 0.5f;

    const f32 craft_start_x =
        panel_start_x + static_cast<f32>(kCraftGridStartCol) * (kInventorySlotSize + kInventorySlotGap);
    const f32 craft_start_y = panel_start_y + kInventoryTopMargin;
    for (u32 row = 0; row < kCraftGridEdge; ++row) {
        for (u32 col = 0; col < kCraftGridEdge; ++col) {
            const u32 index = row * kCraftGridEdge + col;
            layout.craft_input[index] = {
                craft_start_x + static_cast<f32>(col) * (kInventorySlotSize + kInventorySlotGap),
                craft_start_y + static_cast<f32>(row) * (kInventorySlotSize + kInventorySlotGap),
                kInventorySlotSize,
            };
        }
    }
    const f32 result_x = craft_start_x + static_cast<f32>(kCraftGridEdge) * (kInventorySlotSize + kInventorySlotGap) +
                          kCraftResultGap;
    const f32 result_y = craft_start_y + (craft_height - kInventorySlotSize) * 0.5f;
    layout.craft_result = {result_x, result_y, kInventorySlotSize};

    const f32 main_start_y = craft_start_y + craft_height + kInventorySectionGap;
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

InventoryScreenHit hit_test_inventory_screen(const InventoryScreenLayout& layout, f32 mouse_x, f32 mouse_y) {
    for (u32 i = 0; i < kCraftGridSlotCount; ++i) {
        if (point_in_rect(mouse_x, mouse_y, layout.craft_input[i])) {
            return {InventoryScreenRegion::kCraftInput, i};
        }
    }
    if (point_in_rect(mouse_x, mouse_y, layout.craft_result)) {
        return {InventoryScreenRegion::kCraftResult, 0};
    }
    for (u32 i = 0; i < kInventoryMainSlotCount; ++i) {
        if (point_in_rect(mouse_x, mouse_y, layout.main_slots[i])) {
            return {InventoryScreenRegion::kMainInventory, i};
        }
    }
    for (u32 i = 0; i < kHotbarSlotCount; ++i) {
        if (point_in_rect(mouse_x, mouse_y, layout.hotbar_slots[i])) {
            return {InventoryScreenRegion::kHotbar, i};
        }
    }
    return {InventoryScreenRegion::kNone, 0};
}

}  // namespace lcu::ui
