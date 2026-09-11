#include "game/systems/player_vitals_system.h"

#include <gtest/gtest.h>

using game::components::PlayerHealth;
using game::components::PlayerHunger;
using game::systems::apply_damage;
using game::systems::apply_jump_hunger_cost;
using game::systems::eat;
using game::systems::fall_damage_for_distance;
using game::systems::FallTracker;
using game::systems::kFallDamageSafeBlocks;
using game::systems::kHealthRegenIntervalSeconds;
using game::systems::kHungerDrainIntervalSeconds;
using game::systems::kStarvationDamageIntervalSeconds;
using game::systems::update_fall_tracking;
using game::systems::update_health_regen;
using game::systems::update_hunger_drain;
using game::systems::update_starvation;

// --- apply_damage ---------------------------------------------------------

TEST(ApplyDamage, ReducesHealthByAmount) {
    PlayerHealth health;
    apply_damage(health, 5.0f);
    EXPECT_FLOAT_EQ(health.current, 15.0f);
}

TEST(ApplyDamage, ClampsAtZero) {
    PlayerHealth health;
    apply_damage(health, 100.0f);
    EXPECT_FLOAT_EQ(health.current, 0.0f);
}

TEST(ApplyDamage, NegativeAmountHeals) {
    PlayerHealth health;
    health.current = 10.0f;
    apply_damage(health, -5.0f);
    EXPECT_FLOAT_EQ(health.current, 15.0f);
}

TEST(ApplyDamage, HealingClampsAtMax) {
    PlayerHealth health;
    health.current = 18.0f;
    apply_damage(health, -10.0f);
    EXPECT_FLOAT_EQ(health.current, 20.0f);
}

TEST(ApplyDamage, ReturnsTrueOnlyOnTheRealDeathTransition) {
    PlayerHealth health;
    health.current = 5.0f;
    EXPECT_TRUE(apply_damage(health, 10.0f));   // 5 -> 0: real transition.
    EXPECT_FALSE(apply_damage(health, 1.0f));   // already dead: no transition.
}

// --- fall_damage_for_distance ----------------------------------------------

TEST(FallDamageForDistance, NoDamageWithinSafeDistance) {
    EXPECT_FLOAT_EQ(fall_damage_for_distance(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(fall_damage_for_distance(kFallDamageSafeBlocks), 0.0f);
}

TEST(FallDamageForDistance, OneDamagePerBlockBeyondSafeDistance) {
    EXPECT_FLOAT_EQ(fall_damage_for_distance(kFallDamageSafeBlocks + 5.0f), 5.0f);
}

// --- update_fall_tracking ---------------------------------------------------

TEST(UpdateFallTracking, AccumulatesDescendingDistanceWhileAirborne) {
    FallTracker tracker;
    tracker.was_grounded = false;
    PlayerHealth health;

    const lcu::f32 damage = update_fall_tracking(tracker, health, 2.0f, false);

    EXPECT_FLOAT_EQ(damage, 0.0f);
    EXPECT_FLOAT_EQ(tracker.fall_distance, 2.0f);
    EXPECT_FLOAT_EQ(health.current, health.max);  // no damage until landing.
}

TEST(UpdateFallTracking, AscendingMotionDoesNotAddToFallDistance) {
    FallTracker tracker;
    tracker.was_grounded = false;
    PlayerHealth health;

    update_fall_tracking(tracker, health, -3.0f, false);  // moved up.

    EXPECT_FLOAT_EQ(tracker.fall_distance, 0.0f);
}

TEST(UpdateFallTracking, AppliesRealDamageOnlyOnTheLandingFrame) {
    FallTracker tracker;
    tracker.was_grounded = false;
    tracker.fall_distance = kFallDamageSafeBlocks + 4.0f;  // pre-accumulated from a real fall.
    PlayerHealth health;

    const lcu::f32 damage = update_fall_tracking(tracker, health, 0.0f, true);  // lands this frame.

    EXPECT_FLOAT_EQ(damage, 4.0f);
    EXPECT_FLOAT_EQ(health.current, health.max - 4.0f);
    EXPECT_FLOAT_EQ(tracker.fall_distance, 0.0f);  // reset after landing.
}

TEST(UpdateFallTracking, NoDamageForARealSmallHop) {
    FallTracker tracker;
    PlayerHealth health;
    // Real jump: leaves ground (rising), then falls back roughly the
    // same real small height.
    update_fall_tracking(tracker, health, -1.2f, false);  // rising.
    const lcu::f32 damage = update_fall_tracking(tracker, health, 1.2f, true);  // falls back, lands.

    EXPECT_FLOAT_EQ(damage, 0.0f);
    EXPECT_FLOAT_EQ(health.current, health.max);
}

TEST(UpdateFallTracking, StaysGroundedNeverAccumulatesOrDamages) {
    FallTracker tracker;
    PlayerHealth health;

    const lcu::f32 damage = update_fall_tracking(tracker, health, 0.0f, true);

    EXPECT_FLOAT_EQ(damage, 0.0f);
    EXPECT_FLOAT_EQ(tracker.fall_distance, 0.0f);
}

// --- update_health_regen -----------------------------------------------------

TEST(UpdateHealthRegen, HealsOneHpPerIntervalWhenWellFed) {
    PlayerHealth health;
    health.current = 10.0f;
    PlayerHunger hunger;  // full, 20 >= kHealthRegenMinHunger.
    lcu::f32 accumulator = 0.0f;

    update_health_regen(health, hunger, accumulator, kHealthRegenIntervalSeconds);

    EXPECT_FLOAT_EQ(health.current, 11.0f);
}

TEST(UpdateHealthRegen, NoRegenBelowMinHunger) {
    PlayerHealth health;
    health.current = 10.0f;
    PlayerHunger hunger;
    hunger.current = 10.0f;  // below kHealthRegenMinHunger (18).
    lcu::f32 accumulator = 0.0f;

    update_health_regen(health, hunger, accumulator, 100.0f);

    EXPECT_FLOAT_EQ(health.current, 10.0f);
}

TEST(UpdateHealthRegen, StopsAtMaxHealth) {
    PlayerHealth health;  // already full.
    PlayerHunger hunger;
    lcu::f32 accumulator = 0.0f;

    update_health_regen(health, hunger, accumulator, kHealthRegenIntervalSeconds * 3.0f);

    EXPECT_FLOAT_EQ(health.current, health.max);
}

TEST(UpdateHealthRegen, MultipleTicksInOneLargeDtEachHealOneHp) {
    PlayerHealth health;
    health.current = 5.0f;
    PlayerHunger hunger;
    lcu::f32 accumulator = 0.0f;

    update_health_regen(health, hunger, accumulator, kHealthRegenIntervalSeconds * 2.5f);

    EXPECT_FLOAT_EQ(health.current, 7.0f);  // 2 full ticks; 0.5 tick left over.
}

// --- update_starvation --------------------------------------------------------

TEST(UpdateStarvation, NoDamageWhileHungerIsPositive) {
    PlayerHealth health;
    PlayerHunger hunger;
    hunger.current = 1.0f;
    lcu::f32 accumulator = 0.0f;

    EXPECT_FALSE(update_starvation(health, hunger, accumulator, kStarvationDamageIntervalSeconds));
    EXPECT_FLOAT_EQ(health.current, health.max);
}

TEST(UpdateStarvation, DealsRealDamageOnceHungerReachesZero) {
    PlayerHealth health;
    PlayerHunger hunger;
    hunger.current = 0.0f;
    lcu::f32 accumulator = 0.0f;

    update_starvation(health, hunger, accumulator, kStarvationDamageIntervalSeconds);

    EXPECT_FLOAT_EQ(health.current, health.max - 1.0f);
}

TEST(UpdateStarvation, ReturnsTrueOnTheRealDeathTransition) {
    PlayerHealth health;
    health.current = 1.0f;
    PlayerHunger hunger;
    hunger.current = 0.0f;
    lcu::f32 accumulator = 0.0f;

    EXPECT_TRUE(update_starvation(health, hunger, accumulator, kStarvationDamageIntervalSeconds));
    EXPECT_FLOAT_EQ(health.current, 0.0f);
}

// --- update_hunger_drain --------------------------------------------------------

TEST(UpdateHungerDrain, DrainsOnePointPerIntervalNormally) {
    PlayerHunger hunger;
    lcu::f32 accumulator = 0.0f;

    update_hunger_drain(hunger, false, accumulator, kHungerDrainIntervalSeconds);

    EXPECT_FLOAT_EQ(hunger.current, 19.0f);
}

TEST(UpdateHungerDrain, SprintingDrainsFasterViaTheAccumulatorMultiplier) {
    PlayerHunger hunger;
    lcu::f32 accumulator = 0.0f;

    // Half the normal interval, but sprinting doubles accumulation.
    update_hunger_drain(hunger, true, accumulator, kHungerDrainIntervalSeconds / 2.0f);

    EXPECT_FLOAT_EQ(hunger.current, 19.0f);
}

TEST(UpdateHungerDrain, StopsAtZero) {
    PlayerHunger hunger;
    hunger.current = 0.0f;
    lcu::f32 accumulator = 0.0f;

    update_hunger_drain(hunger, false, accumulator, kHungerDrainIntervalSeconds * 5.0f);

    EXPECT_FLOAT_EQ(hunger.current, 0.0f);
}

// --- apply_jump_hunger_cost / eat --------------------------------------------------

TEST(ApplyJumpHungerCost, DrainsARealSmallFixedAmount) {
    PlayerHunger hunger;
    apply_jump_hunger_cost(hunger);
    EXPECT_FLOAT_EQ(hunger.current, 20.0f - game::systems::kHungerPerJump);
}

TEST(Eat, RestoresHungerClampedToMax) {
    PlayerHunger hunger;
    hunger.current = 10.0f;
    eat(hunger, 5.0f);
    EXPECT_FLOAT_EQ(hunger.current, 15.0f);

    eat(hunger, 100.0f);
    EXPECT_FLOAT_EQ(hunger.current, hunger.max);
}
