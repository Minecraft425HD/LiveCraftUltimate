#include "lcu/replication/position_interpolator.h"

#include "lcu/core/assert.h"

namespace lcu::replication {

PositionInterpolator::PositionInterpolator(f32 render_delay) : render_delay_(render_delay) {}

void PositionInterpolator::add_sample(f32 timestamp, const math::Vec3& position) {
    LCU_ASSERT(samples_.empty() || timestamp >= samples_.back().timestamp);
    samples_.push_back({timestamp, position});
    if (samples_.size() > kMaxBufferedSamples) {
        samples_.pop_front();
    }
}

math::Vec3 PositionInterpolator::interpolated_position(f32 render_time) const {
    if (samples_.empty()) {
        return {0.0f, 0.0f, 0.0f};
    }

    const f32 target = render_time - render_delay_;

    if (target <= samples_.front().timestamp) {
        return samples_.front().position;
    }
    if (target >= samples_.back().timestamp) {
        return samples_.back().position;
    }

    for (usize i = 0; i + 1 < samples_.size(); ++i) {
        const Sample& a = samples_[i];
        const Sample& b = samples_[i + 1];
        if (target >= a.timestamp && target <= b.timestamp) {
            const f32 span = b.timestamp - a.timestamp;
            const f32 t = span > 0.0f ? (target - a.timestamp) / span : 0.0f;
            return a.position + (b.position - a.position) * t;
        }
    }

    return samples_.back().position;  // unreachable given the bounds checks above; defensive.
}

}  // namespace lcu::replication
