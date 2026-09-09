#include "lcu/debug/frame_stats.h"

namespace lcu::debug {

std::optional<FrameStats::Report> FrameStats::update(f32 delta_seconds) {
    ++total_frames_;
    ++frames_in_window_;
    accumulated_seconds_ += delta_seconds;

    if (accumulated_seconds_ < report_interval_) {
        return std::nullopt;
    }

    Report report;
    report.frame_count = total_frames_;
    report.avg_frame_ms = (accumulated_seconds_ / static_cast<f32>(frames_in_window_)) * 1000.0f;
    report.fps = report.avg_frame_ms > 0.0f ? 1000.0f / report.avg_frame_ms : 0.0f;

    accumulated_seconds_ = 0.0f;
    frames_in_window_ = 0;

    return report;
}

}  // namespace lcu::debug
