#include "lcu/replication/prediction_buffer.h"

#include <gtest/gtest.h>

#include "lcu/physics/collision.h"
#include "lcu/voxel/chunk.h"
#include "lcu/world/world.h"

using lcu::replication::PredictionBuffer;

namespace {

// A simple, hand-verifiable State/Input pair for the pure algorithm
// tests: State is a 1D position, Input is a signed step, apply is plain
// addition scaled by dt.
float apply_step(const float& state, const float& input, float dt) { return state + input * dt; }

}  // namespace

TEST(PredictionBuffer, PredictAndRecordAppliesInputImmediately) {
    PredictionBuffer<float, float> buffer(apply_step);
    const float next = buffer.predict_and_record(0.0f, 1, 5.0f, 1.0f);
    EXPECT_FLOAT_EQ(next, 5.0f);
    EXPECT_EQ(buffer.pending_input_count(), 1u);
}

TEST(PredictionBuffer, ReconcileWithNoActualDivergenceReproducesTheSamePrediction) {
    PredictionBuffer<float, float> buffer(apply_step);
    float state = 0.0f;
    state = buffer.predict_and_record(state, 1, 2.0f, 1.0f);  // -> 2
    state = buffer.predict_and_record(state, 2, 3.0f, 1.0f);  // -> 5
    state = buffer.predict_and_record(state, 3, 1.0f, 1.0f);  // -> 6

    // The server, having applied only input 1, reports exactly the same
    // state the client already predicted for that point (2.0) - the
    // common "no divergence" case.
    const float reconciled = buffer.reconcile(2.0f, 1);
    EXPECT_FLOAT_EQ(reconciled, state);  // replaying 2 and 3 reproduces 6 either way
}

TEST(PredictionBuffer, ReconcileDiscardsAcknowledgedHistory) {
    PredictionBuffer<float, float> buffer(apply_step);
    buffer.predict_and_record(0.0f, 1, 1.0f, 1.0f);
    buffer.predict_and_record(1.0f, 2, 1.0f, 1.0f);
    buffer.predict_and_record(2.0f, 3, 1.0f, 1.0f);
    EXPECT_EQ(buffer.pending_input_count(), 3u);

    buffer.reconcile(1.0f, 1);  // server has now accounted for input 1
    EXPECT_EQ(buffer.pending_input_count(), 2u);  // only 2 and 3 remain pending
}

TEST(PredictionBuffer, ReconcileWithACorrectionShiftsTheFinalStateByTheSameAmount) {
    PredictionBuffer<float, float> buffer(apply_step);
    float state = 0.0f;
    state = buffer.predict_and_record(state, 1, 2.0f, 1.0f);  // -> 2
    state = buffer.predict_and_record(state, 2, 3.0f, 1.0f);  // -> 5
    state = buffer.predict_and_record(state, 3, 1.0f, 1.0f);  // -> 6 (locally predicted)

    // The server disagreed about input 1's result (reports 2.5, not
    // 2.0 - a 0.5 correction). Replaying the same still-pending inputs
    // (2 and 3) on top of that corrected base should shift the final
    // result by exactly that same 0.5, since apply_step is a pure
    // translation with no clamping to interfere.
    const float reconciled = buffer.reconcile(2.5f, 1);
    EXPECT_FLOAT_EQ(reconciled, 6.5f);
}

TEST(PredictionBuffer, ReconcileWithEverythingAcknowledgedReturnsTheAuthoritativeStateUnchanged) {
    PredictionBuffer<float, float> buffer(apply_step);
    buffer.predict_and_record(0.0f, 1, 5.0f, 1.0f);

    const float reconciled = buffer.reconcile(42.0f, 1);  // sequence 1 fully acknowledged, nothing to replay
    EXPECT_FLOAT_EQ(reconciled, 42.0f);
    EXPECT_EQ(buffer.pending_input_count(), 0u);
}

// --- Real instantiation: lcu::physics::PlayerPhysicsState ------------------

namespace {

constexpr lcu::voxel::BlockId kSolid = 5;

lcu::world::World make_open_floor_world() {
    lcu::world::World world(1, [](lcu::voxel::Chunk& chunk, lcu::voxel::ChunkCoord) {
        for (lcu::u32 x = 0; x < lcu::voxel::Chunk::kEdgeLength; ++x) {
            for (lcu::u32 z = 0; z < lcu::voxel::Chunk::kEdgeLength; ++z) {
                chunk.set_block(x, 0, z, kSolid);
            }
        }
    });
    world.load_chunk({0, 0, 0});
    return world;
}

bool is_solid_predicate(lcu::voxel::BlockId id) { return id == kSolid; }

}  // namespace

TEST(PredictionBuffer, RealPlayerPhysicsStateReconciliationAfterAServerCorrection) {
    using lcu::math::Vec3;
    using lcu::physics::AABB;
    using lcu::physics::integrate_player;
    using lcu::physics::PlayerPhysicsConfig;
    using lcu::physics::PlayerPhysicsState;

    const lcu::world::World world = make_open_floor_world();
    const PlayerPhysicsConfig config;

    // A pure, deterministic step function wired against the same
    // integrate_player used by the real game - exactly the "generic
    // over a real engine step function" case this buffer exists for.
    const auto apply = [&](const PlayerPhysicsState& state, const Vec3& horizontal_delta, lcu::f32 dt) {
        PlayerPhysicsState next = state;
        integrate_player(world, next, horizontal_delta, config, dt, is_solid_predicate);
        return next;
    };

    PredictionBuffer<PlayerPhysicsState, Vec3> buffer(apply);

    PlayerPhysicsState state;
    state.aabb = AABB{{-0.3f, 1.0f, -0.3f}, {0.3f, 2.0f, 0.3f}};  // resting on the y=0 floor
    state.grounded = true;

    // Three predicted ticks of open-air horizontal movement (no gravity
    // in play here - vertical_velocity stays 0, so this isolates the
    // reconciliation math from unrelated falling behavior).
    state = buffer.predict_and_record(state, 1, {1.0f, 0.0f, 0.0f}, 1.0f);
    state = buffer.predict_and_record(state, 2, {1.0f, 0.0f, 0.0f}, 1.0f);
    const lcu::f32 locally_predicted_x = state.aabb.min.x;

    // The server's authoritative state after input 1 differs from what
    // the client predicted by +0.5 in x (a plausible real correction -
    // some other player's block placement the client hadn't received
    // yet, say). Reconciling should replay input 2 on top of that
    // corrected base, landing exactly 0.5 ahead of the original
    // (uncorrected) prediction, since movement in open air over solid
    // ground is a pure translation with nothing else to interfere.
    PlayerPhysicsState authoritative = state;  // start from a state shaped like the client's own
    authoritative.aabb = AABB{{-0.3f + 1.0f + 0.5f, 1.0f, -0.3f}, {0.3f + 1.0f + 0.5f, 2.0f, 0.3f}};
    authoritative.grounded = true;

    const PlayerPhysicsState reconciled = buffer.reconcile(authoritative, 1);

    EXPECT_NEAR(reconciled.aabb.min.x, locally_predicted_x + 0.5f, 1e-3f);
    EXPECT_EQ(buffer.pending_input_count(), 1u);  // only input 2 was replayed/still pending
}
