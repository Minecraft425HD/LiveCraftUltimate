#pragma once

#include "lcu/core/types.h"
#include "lcu/ui/crafting_table_screen.h"

namespace lcu::rendering {
class Renderer;
}

namespace lcu::ui {

// Same real quad-batching/label-drawing split inventory_screen_renderer.h
// already establishes - see its own doc comments for the shared
// contract (queue before flush_ui_quads; does not call
// Renderer::clear_debug_text() itself).
void queue_crafting_table_screen_quads(rendering::Renderer& renderer, const CraftingTableScreenLayout& layout,
                                        const CraftingTableScreenState& state, u32 screen_width, u32 screen_height);

void draw_crafting_table_screen_labels(rendering::Renderer& renderer, const CraftingTableScreenLayout& layout,
                                        const CraftingTableScreenState& state);

}  // namespace lcu::ui
