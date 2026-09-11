#pragma once

#include "lcu/core/types.h"
#include "lcu/math/vec3.h"

namespace game::systems {

// Tracks elapsed time through a repeating day/night cycle (brief section
// 60's Tag/Nacht-Zyklus) and reports how bright the sky currently is.
// Doesn't touch any lcu::lighting::LightStorage itself - callers scale
// their already-computed sky light by sky_light_scale() (a cheap global
// multiplier). Recomputing every chunk's sky light continuously over
// time as the sun moves is not implemented - nothing has a persistent,
// continuously-observed visible world to make that worth building yet
// (see DECISIONS.md); the scale factor here is the real, useful part:
// something a renderer or ambient-light system can already multiply by
// today.
class DayNightCycle {
   public:
    // day_length_seconds: how many real seconds one full day/night cycle
    // takes. Default is arbitrary (20 real minutes) - no design pass has
    // picked a "real" value yet since nothing renders the sky.
    explicit DayNightCycle(lcu::f32 day_length_seconds = 1200.0f);

    void update(lcu::f32 dt);

    // [0, 1): 0.0 = dawn, 0.25 = noon, 0.5 = dusk, 0.75 = midnight,
    // wrapping back to 0.0 at the next dawn.
    lcu::f32 time_of_day() const;

    // [kNightSkyLightFraction, 1.0]: 1.0 at noon, smoothly down to a
    // small nonzero floor at midnight (dim ambient moon/starlight, never
    // fully black) via a cosine curve peaking at noon.
    lcu::f32 sky_light_scale() const;

   private:
    lcu::f32 day_length_seconds_;
    lcu::f32 elapsed_seconds_ = 0.0f;
};

// Pure function (Phase 27, skybox sun/moon): unit-circle direction of the
// sun for a given time_of_day(), independent of any DayNightCycle
// instance so it's trivially headlessly testable at exact phase points.
// angle = 0 at dawn (t=0, sun on the horizon), pi/2 at noon (t=0.25, sun
// straight up), pi at dusk (t=0.5, sun on the opposite horizon), 3pi/2 at
// midnight (t=0.75, sun straight down / below the world). The moon is
// always exactly opposite the sun (moon_direction = -sun_direction).
lcu::math::Vec3 sun_direction(lcu::f32 time_of_day);

}  // namespace game::systems
