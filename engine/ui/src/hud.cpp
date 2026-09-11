#include "lcu/ui/hud.h"

#include <algorithm>

namespace lcu::ui {

std::array<HotbarSlotRect, kHotbarSlotCount> hotbar_slot_layout(u32 screen_width, u32 screen_height) {
    std::array<HotbarSlotRect, kHotbarSlotCount> rects{};
    const f32 total_width = static_cast<f32>(kHotbarSlotCount) * kHotbarSlotSize +
                             static_cast<f32>(kHotbarSlotCount - 1) * kHotbarSlotGap;
    const f32 start_x = (static_cast<f32>(screen_width) - total_width) * 0.5f;
    const f32 y = static_cast<f32>(screen_height) - kHotbarBottomMargin - kHotbarSlotSize;
    for (u32 i = 0; i < kHotbarSlotCount; ++i) {
        rects[i] = {start_x + static_cast<f32>(i) * (kHotbarSlotSize + kHotbarSlotGap), y, kHotbarSlotSize};
    }
    return rects;
}

std::array<StatBarIconRect, kStatBarIconCount> stat_bar_layout(u32 screen_width, f32 bottom_y, f32 value,
                                                                f32 max_value) {
    std::array<StatBarIconRect, kStatBarIconCount> rects{};
    // Real left-alignment with the hotbar's own left edge (both share
    // this same total-width formula) - not an independent guessed
    // x-offset, so the bars visually line up with the row below them
    // the way Minecraft's own HUD does.
    const f32 hotbar_total_width = static_cast<f32>(kHotbarSlotCount) * kHotbarSlotSize +
                                    static_cast<f32>(kHotbarSlotCount - 1) * kHotbarSlotGap;
    const f32 start_x = (static_cast<f32>(screen_width) - hotbar_total_width) * 0.5f;
    const f32 y = bottom_y - kStatBarIconSize;
    for (u32 i = 0; i < kStatBarIconCount; ++i) {
        const f32 icon_min_value = static_cast<f32>(i) * 2.0f;
        const f32 fill = max_value <= 0.0f ? 0.0f : std::clamp((value - icon_min_value) / 2.0f, 0.0f, 1.0f);
        rects[i] = {start_x + static_cast<f32>(i) * (kStatBarIconSize + kStatBarIconGap), y, kStatBarIconSize, fill};
    }
    return rects;
}

}  // namespace lcu::ui
