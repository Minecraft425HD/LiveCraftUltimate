#pragma once

#include <deque>
#include <functional>

#include "lcu/core/types.h"

namespace lcu::replication {

// Client-side prediction + server reconciliation (brief section 64),
// generic over an application-supplied `State`/`Input` pair and a pure
// step function `State(const State&, const Input&, f32 dt)` - this
// doesn't know or care that a real caller's `State` is
// lcu::physics::PlayerPhysicsState and `Input` is a movement vector; it
// only needs a deterministic step function, which `integrate_player`
// already is (see tests/replication/prediction_buffer_test.cpp for that
// concrete instantiation, plus a plain-float instantiation for the pure
// algorithm tests).
//
// Usage: each local simulation tick, predict_and_record() applies the
// input immediately (so local movement feels instant, not delayed by a
// network round-trip to the server and back) and remembers
// {sequence, input, dt} for possible replay. When an authoritative
// correction for some earlier sequence arrives from the server later,
// reconcile() discards every recorded input at or before that sequence
// (the server has already accounted for them) and replays every
// remaining one on top of the server's corrected state to reconstruct
// an up-to-date predicted state - the standard prediction/reconciliation
// algorithm.
template <typename State, typename Input>
class PredictionBuffer {
   public:
    using ApplyFn = std::function<State(const State&, const Input&, f32)>;

    explicit PredictionBuffer(ApplyFn apply) : apply_(std::move(apply)) {}

    // Applies `input` to `state` immediately and records it under
    // `sequence` (the caller's own monotonically increasing input
    // counter, incremented once per local simulation tick) for later
    // replay. Returns the newly predicted state.
    State predict_and_record(const State& state, u32 sequence, Input input, f32 dt) {
        State next = apply_(state, input, dt);
        history_.push_back(Record{sequence, std::move(input), dt});
        return next;
    }

    // Reconciles against an authoritative state the server reports as
    // of `acknowledged_sequence` (the last input sequence the server
    // had actually applied when it produced that state). Discards every
    // recorded input at or before that sequence, then replays every
    // remaining one on top of `authoritative_state`. Returns the
    // reconstructed, up-to-date predicted state - identical to what
    // predict_and_record would have produced if the server's view had
    // been available all along.
    State reconcile(const State& authoritative_state, u32 acknowledged_sequence) {
        while (!history_.empty() && history_.front().sequence <= acknowledged_sequence) {
            history_.pop_front();
        }

        State state = authoritative_state;
        for (const Record& record : history_) {
            state = apply_(state, record.input, record.dt);
        }
        return state;
    }

    // How many predicted inputs are still awaiting server acknowledgment.
    usize pending_input_count() const { return history_.size(); }

   private:
    struct Record {
        u32 sequence;
        Input input;
        f32 dt;
    };

    ApplyFn apply_;
    std::deque<Record> history_;
};

}  // namespace lcu::replication
