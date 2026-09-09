#include "game/systems/day_night_cycle.h"

#include <gtest/gtest.h>

using game::systems::DayNightCycle;

TEST(DayNightCycle, StartsAtDawn) {
    DayNightCycle cycle(1000.0f);
    EXPECT_FLOAT_EQ(cycle.time_of_day(), 0.0f);
}

TEST(DayNightCycle, TimeOfDayAdvancesProportionallyToElapsedTime) {
    DayNightCycle cycle(1000.0f);
    cycle.update(250.0f);
    EXPECT_NEAR(cycle.time_of_day(), 0.25f, 1e-5f);
}

TEST(DayNightCycle, TimeOfDayWrapsAroundAfterAFullDay) {
    DayNightCycle cycle(1000.0f);
    cycle.update(1200.0f);  // 1.2 days
    EXPECT_NEAR(cycle.time_of_day(), 0.2f, 1e-4f);
}

TEST(DayNightCycle, SkyLightScaleIsFullAtNoon) {
    DayNightCycle cycle(1000.0f);
    cycle.update(250.0f);  // t = 0.25 = noon
    EXPECT_NEAR(cycle.sky_light_scale(), 1.0f, 1e-4f);
}

TEST(DayNightCycle, SkyLightScaleIsAtItsNightFloorAtMidnight) {
    DayNightCycle cycle(1000.0f);
    cycle.update(750.0f);  // t = 0.75 = midnight
    EXPECT_NEAR(cycle.sky_light_scale(), 0.1f, 1e-4f);
}

TEST(DayNightCycle, SkyLightScaleIsMidwayAtDawnAndDusk) {
    DayNightCycle dawn(1000.0f);
    dawn.update(0.0f);  // t = 0.0 = dawn
    EXPECT_NEAR(dawn.sky_light_scale(), 0.55f, 1e-4f);

    DayNightCycle dusk(1000.0f);
    dusk.update(500.0f);  // t = 0.5 = dusk
    EXPECT_NEAR(dusk.sky_light_scale(), 0.55f, 1e-4f);
}

TEST(DayNightCycle, SkyLightScaleNeverGoesBelowTheNightFloor) {
    DayNightCycle cycle(1000.0f);
    for (int i = 0; i < 20; ++i) {
        cycle.update(50.0f);
        EXPECT_GE(cycle.sky_light_scale(), 0.1f - 1e-4f);
        EXPECT_LE(cycle.sky_light_scale(), 1.0f + 1e-4f);
    }
}
