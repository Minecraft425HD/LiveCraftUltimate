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

TEST(SunDirection, AtDawnSunIsOnTheHorizon) {
    const lcu::math::Vec3 dir = game::systems::sun_direction(0.0f);
    EXPECT_NEAR(dir.x, 1.0f, 1e-5f);
    EXPECT_NEAR(dir.y, 0.0f, 1e-5f);
    EXPECT_NEAR(dir.z, 0.0f, 1e-5f);
}

TEST(SunDirection, AtNoonSunIsStraightUp) {
    const lcu::math::Vec3 dir = game::systems::sun_direction(0.25f);
    EXPECT_NEAR(dir.x, 0.0f, 1e-5f);
    EXPECT_NEAR(dir.y, 1.0f, 1e-5f);
    EXPECT_NEAR(dir.z, 0.0f, 1e-5f);
}

TEST(SunDirection, AtDuskSunIsOnTheOppositeHorizon) {
    const lcu::math::Vec3 dir = game::systems::sun_direction(0.5f);
    EXPECT_NEAR(dir.x, -1.0f, 1e-5f);
    EXPECT_NEAR(dir.y, 0.0f, 1e-5f);
    EXPECT_NEAR(dir.z, 0.0f, 1e-5f);
}

TEST(SunDirection, AtMidnightSunIsStraightDown) {
    const lcu::math::Vec3 dir = game::systems::sun_direction(0.75f);
    EXPECT_NEAR(dir.x, 0.0f, 1e-5f);
    EXPECT_NEAR(dir.y, -1.0f, 1e-5f);
    EXPECT_NEAR(dir.z, 0.0f, 1e-5f);
}

TEST(SunDirection, IsAlwaysAUnitVectorInTheXyPlane) {
    for (int i = 0; i < 16; ++i) {
        const lcu::f32 t = static_cast<lcu::f32>(i) / 16.0f;
        const lcu::math::Vec3 dir = game::systems::sun_direction(t);
        EXPECT_NEAR(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z, 1.0f, 1e-4f);
        EXPECT_FLOAT_EQ(dir.z, 0.0f);
    }
}

TEST(SunDirection, MoonIsAlwaysExactlyOppositeTheSun) {
    for (int i = 0; i < 16; ++i) {
        const lcu::f32 t = static_cast<lcu::f32>(i) / 16.0f;
        const lcu::math::Vec3 sun_dir = game::systems::sun_direction(t);
        const lcu::math::Vec3 moon_dir = -sun_dir;
        EXPECT_FLOAT_EQ(moon_dir.x, -sun_dir.x);
        EXPECT_FLOAT_EQ(moon_dir.y, -sun_dir.y);
    }
}
