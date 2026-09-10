#pragma once

#include <deque>

#include "lcu/core/types.h"
#include "lcu/math/vec3.h"

namespace lcu::replication {

// Buffers timestamped position samples (as received from a replicated
// entity over the network) and produces a smoothly interpolated
// position for any render time - remote entity interpolation (brief
// section 64). Rather than snapping to each new sample the instant it
// arrives (which looks like teleporting between the sparse ticks a
// network actually delivers at), this renders slightly in the past
// (`render_delay` behind the caller's own clock) so there are usually
// two real samples bracketing the requested time to interpolate
// between, instead of guessing at where the entity is going next.
class PositionInterpolator {
   public:
    explicit PositionInterpolator(f32 render_delay = 0.1f);

    // Records a new sample at `timestamp` (seconds, on the receiving
    // side's own clock at the moment the sample arrived - not the
    // sender's clock, since the two aren't synchronized here; see
    // DECISIONS.md). Samples are expected in non-decreasing timestamp
    // order (the normal case: they arrive and get recorded as they're
    // received) - LCU_ASSERT enforces that in development builds rather
    // than silently reordering.
    void add_sample(f32 timestamp, const math::Vec3& position);

    // Returns the interpolated position for `render_time` (the caller's
    // current local time; `render_delay` is subtracted internally to
    // get the actual query time). No samples yet -> the zero vector.
    // Query time at or before the oldest buffered sample, or with only
    // one sample recorded so far -> that sample's position, unchanged.
    // Query time at or after the newest sample (the network has fallen
    // behind, or `render_delay` is smaller than the actual update
    // interval) -> holds at the newest known position rather than
    // extrapolating past it - see DECISIONS.md "no extrapolation".
    // Otherwise -> linear interpolation between the two samples
    // bracketing the query time.
    math::Vec3 interpolated_position(f32 render_time) const;

    usize sample_count() const { return samples_.size(); }

   private:
    struct Sample {
        f32 timestamp;
        math::Vec3 position;
    };

    // Bounds memory for a connection that outlives its sample rate
    // expectations (e.g. a paused/very slow client) - old samples this
    // far back are no longer useful for interpolation anyway once far
    // newer ones exist.
    static constexpr usize kMaxBufferedSamples = 32;

    f32 render_delay_;
    std::deque<Sample> samples_;
};

}  // namespace lcu::replication
