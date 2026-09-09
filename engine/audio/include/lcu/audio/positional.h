#pragma once

#include "lcu/core/types.h"
#include "lcu/math/vec3.h"

namespace lcu::audio {

// Per-ear gain multipliers (Phase 12's positional audio) applied on top
// of a source's own volume - together with distance_attenuation() below,
// this is what makes a sound seem to come from a direction/distance
// rather than play identically regardless of where its source is.
struct StereoGain {
    f32 left = 1.0f;
    f32 right = 1.0f;
};

// Simple pan based on which side of the listener (defined by
// `listener_right`, expected to already be a unit vector - e.g. a
// FirstPersonCamera's right()) `source_position` is on. Directly ahead/
// behind (zero lateral component) pans center (left == right == 1); fully
// to one side fades the opposite ear to 0. Not full HRTF/3D audio - a
// real, working first pass, not a placeholder (see DECISIONS.md for why
// nothing more elaborate is justified yet).
StereoGain compute_stereo_pan(const math::Vec3& listener_position, const math::Vec3& listener_right,
                               const math::Vec3& source_position);

// Linear falloff from 1.0 at distance 0 to 0.0 at `max_distance`, clamped
// to [0, 1] - a source at or beyond max_distance is fully silent, never
// negative gain. Deliberately simpler than real inverse-square
// attenuation: with no in-game sound content yet to tune a rolloff
// exponent against, a plain "silent past this distance" curve is the
// honest amount of complexity to add right now (see DECISIONS.md).
f32 distance_attenuation(f32 distance, f32 max_distance);

}  // namespace lcu::audio
