#include <gtest/gtest.h>

#include "lcu/math/vec4.h"
#include "lcu/rendering/renderer.h"

using lcu::math::Vec4;
using lcu::rendering::Renderer;
using lcu::rendering::RendererDesc;

namespace {

// Same real-headless-Renderer-per-test pattern chunk_mesh_upload_test.cpp
// already established (bgfx is process-wide singleton state - see that
// file's own comment on why each test owns its own lifecycle).
RendererDesc headless_desc() {
    RendererDesc desc;
    desc.force_headless = true;
    return desc;
}

}  // namespace

TEST(QuadBatch2D, StartsEmpty) {
    Renderer renderer;
    ASSERT_TRUE(renderer.init(headless_desc()));

    EXPECT_EQ(renderer.pending_ui_quad_count(), 0u);
}

TEST(QuadBatch2D, SubmitQuadIncreasesPendingCount) {
    Renderer renderer;
    ASSERT_TRUE(renderer.init(headless_desc()));

    renderer.submit_ui_quad(0.0f, 0.0f, 16.0f, 16.0f, Vec4{1.0f, 1.0f, 1.0f, 1.0f});
    EXPECT_EQ(renderer.pending_ui_quad_count(), 1u);

    renderer.submit_ui_quad(20.0f, 20.0f, 8.0f, 8.0f, Vec4{0.0f, 0.0f, 0.0f, 1.0f});
    EXPECT_EQ(renderer.pending_ui_quad_count(), 2u);
}

TEST(QuadBatch2D, FlushClearsThePendingBatch) {
    Renderer renderer;
    ASSERT_TRUE(renderer.init(headless_desc()));

    renderer.submit_ui_quad(0.0f, 0.0f, 16.0f, 16.0f, Vec4{1.0f, 1.0f, 1.0f, 1.0f});
    ASSERT_EQ(renderer.pending_ui_quad_count(), 1u);

    // BGFX_INVALID_HANDLE (no LCU_BUILD_SHADER_TOOLS in this build, same
    // as every other headless test in this repo) - flush_ui_quads still
    // real-clears the batch even though it can't actually submit a draw
    // call without a valid program (see its own doc comment).
    renderer.flush_ui_quads(BGFX_INVALID_HANDLE);

    EXPECT_EQ(renderer.pending_ui_quad_count(), 0u);
}

TEST(QuadBatch2D, FlushWithNoQueuedQuadsIsSafe) {
    Renderer renderer;
    ASSERT_TRUE(renderer.init(headless_desc()));

    // Real proxy for "never crashes on the empty-batch case a frame
    // with no UI to draw yet would hit" - every earlier phase's
    // real-run verification exercises exactly this path (no HUD widget
    // submits a quad, only the crosshair does, before this phase it was
    // literally zero calls).
    renderer.flush_ui_quads(BGFX_INVALID_HANDLE);
    EXPECT_EQ(renderer.pending_ui_quad_count(), 0u);
}

TEST(QuadBatch2D, MultipleQuadsBatchIntoOneFlushCall) {
    Renderer renderer;
    ASSERT_TRUE(renderer.init(headless_desc()));

    // Real proxy for "one draw call for however many quads were queued"
    // (the brief's own "ein Draw-Call" requirement): submitting several
    // quads before a single flush_ui_quads() call is the whole point of
    // batching - this doesn't assert on bgfx's internal draw-call count
    // directly (no headless-observable counter exists for that), but
    // does confirm the batch genuinely accumulates across multiple
    // submit_ui_quad() calls rather than each one being its own
    // independent unit that would need its own flush.
    for (int i = 0; i < 5; ++i) {
        renderer.submit_ui_quad(static_cast<float>(i) * 10.0f, 0.0f, 8.0f, 8.0f, Vec4{1.0f, 0.0f, 0.0f, 1.0f});
    }
    EXPECT_EQ(renderer.pending_ui_quad_count(), 5u);

    renderer.flush_ui_quads(BGFX_INVALID_HANDLE);
    EXPECT_EQ(renderer.pending_ui_quad_count(), 0u);
}
