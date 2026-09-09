#include "game/systems/day_night_cycle.h"

#include <cmath>

#include "lcu/core/assert.h"

namespace game::systems {

namespace {
constexpr lcu::f32 kPi = 3.14159265358979323846f;
constexpr lcu::f32 kNightSkyLightFraction = 0.1f;
}  // namespace

DayNightCycle::DayNightCycle(lcu::f32 day_length_seconds) : day_length_seconds_(day_length_seconds) {
    LCU_ASSERT(day_length_seconds_ > 0.0f);
}

void DayNightCycle::update(lcu::f32 dt) {
    elapsed_seconds_ = std::fmod(elapsed_seconds_ + dt, day_length_seconds_);
    if (elapsed_seconds_ < 0.0f) {
        elapsed_seconds_ += day_length_seconds_;
    }
}

lcu::f32 DayNightCycle::time_of_day() const { return elapsed_seconds_ / day_length_seconds_; }

lcu::f32 DayNightCycle::sky_light_scale() const {
    // Shifted so the cosine peaks (angle = 0) at noon (t = 0.25) and
    // troughs (angle = pi) at midnight (t = 0.75).
    const lcu::f32 angle = (time_of_day() - 0.25f) * 2.0f * kPi;
    const lcu::f32 raw = (std::cos(angle) + 1.0f) * 0.5f;  // [0, 1]: 1 at noon, 0 at midnight
    return kNightSkyLightFraction + raw * (1.0f - kNightSkyLightFraction);
}

}  // namespace game::systems
