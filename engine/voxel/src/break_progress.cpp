#include "lcu/voxel/break_progress.h"

#include <algorithm>

namespace lcu::voxel {

f32 break_progress_fraction(f32 held_seconds, f32 hardness) {
    if (hardness <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(held_seconds / hardness, 0.0f, 1.0f);
}

bool is_break_ready(f32 held_seconds, f32 hardness) { return break_progress_fraction(held_seconds, hardness) >= 1.0f; }

}  // namespace lcu::voxel
