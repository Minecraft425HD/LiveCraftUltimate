#include "lcu/audio/positional.h"

#include <algorithm>

namespace lcu::audio {

StereoGain compute_stereo_pan(const math::Vec3& listener_position, const math::Vec3& listener_right,
                               const math::Vec3& source_position) {
    const math::Vec3 to_source = source_position - listener_position;
    const f32 distance = math::length(to_source);
    if (distance < 1e-5f) {
        // Source at (effectively) the listener's own position - no
        // meaningful direction to pan toward; play centered.
        return StereoGain{1.0f, 1.0f};
    }

    const math::Vec3 direction = to_source / distance;
    // Signed lateral component: +1 fully to the listener's right, -1
    // fully to their left, 0 straight ahead/behind.
    const f32 pan = std::clamp(math::dot(direction, listener_right), -1.0f, 1.0f);

    StereoGain gain;
    gain.left = pan > 0.0f ? (1.0f - pan) : 1.0f;
    gain.right = pan < 0.0f ? (1.0f + pan) : 1.0f;
    return gain;
}

f32 distance_attenuation(f32 distance, f32 max_distance) {
    if (max_distance <= 0.0f) {
        return distance <= 0.0f ? 1.0f : 0.0f;
    }
    return std::clamp(1.0f - (distance / max_distance), 0.0f, 1.0f);
}

}  // namespace lcu::audio
