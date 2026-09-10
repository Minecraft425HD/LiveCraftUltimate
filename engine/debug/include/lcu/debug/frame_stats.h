#pragma once

#include <optional>

#include "lcu/core/types.h"

namespace lcu::debug {

// Minimal FPS/frame-time tracker (brief section 60's debug overlay,
// starting with just the FPS/frame-time line - CPU/GPU/RAM/chunks/etc.
// are added once the systems that produce those numbers exist). Pure
// accumulator, no SDL/bgfx dependency, so it's equally usable by
// VoxelClient (text overlay today, on-screen later) and VoxelServer (tick
// rate reporting) without either depending on the other's platform layer.
class FrameStats {
   public:
    struct Report {
        f32 fps = 0.0f;
        f32 avg_frame_ms = 0.0f;
        u64 frame_count = 0;
    };

    explicit FrameStats(f32 report_interval_seconds = 1.0f)
        : report_interval_(report_interval_seconds) {}

    // Call once per frame with the elapsed time since the previous call,
    // in seconds. Returns a Report roughly every `report_interval_seconds`
    // of wall-clock time (averaged over the frames in that window), and
    // std::nullopt on every other call.
    std::optional<Report> update(f32 delta_seconds);

    u64 total_frames() const { return total_frames_; }

   private:
    f32 report_interval_;
    f32 accumulated_seconds_ = 0.0f;
    u64 frames_in_window_ = 0;
    u64 total_frames_ = 0;
};

}  // namespace lcu::debug
