#pragma once

#include "lcu/core/types.h"

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

}  // namespace game::systems
