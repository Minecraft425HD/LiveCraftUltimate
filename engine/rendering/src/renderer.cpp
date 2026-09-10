#include "lcu/rendering/renderer.h"

#include <bgfx/bgfx.h>

#include <algorithm>
#include <cstring>

#include "lcu/core/assert.h"
#include "lcu/core/log.h"

namespace lcu::rendering {

namespace {

// Sky/sun/moon (Phase 27) get their own bgfx view, executed *before* the
// terrain view (view 0) via an explicit bgfx::setViewOrder below - not
// the default ascending-id order, since view 0 was already "terrain"
// before this phase and renumbering it would be a bigger, riskier diff
// for no real benefit. The sky view clears color+depth (it's first);
// terrain draws into the same shared depth buffer afterward with its
// normal depth test, so it naturally overwrites/occludes the sky
// wherever a block is actually in front of it - real occlusion via view
// ordering, not a depth-test trick on the sky quad itself (which
// explicitly has depth test/write off, per the "Tiefentest aus" brief).
constexpr bgfx::ViewId kSkyViewId = 1;

}  // namespace

Renderer::~Renderer() {
    if (initialized_) {
        if (bgfx::isValid(sky_light_scale_uniform_)) {
            bgfx::destroy(sky_light_scale_uniform_);
        }
        bgfx::shutdown();
    }
}

bool Renderer::init(const RendererDesc& desc) {
    LCU_ASSERT(!initialized_);

    width_ = desc.width;
    height_ = desc.height;
    headless_ = desc.force_headless || desc.window_handle.nwh == nullptr;

    bgfx::Init init;
    init.type = headless_ ? bgfx::RendererType::Noop : bgfx::RendererType::Count;
    init.vendorId = BGFX_PCI_ID_NONE;
    // This bgfx version moved the main window's nwh/ndt/size out of a flat
    // `resolution`/`platformData` pair into `Init::swapChain` (see
    // bgfx/bgfx.h `struct SwapChain`) so multiple windows can each own one.
    // We only ever create the one swap chain bgfx::init makes for us here.
    init.swapChain.nwh = desc.window_handle.nwh;
    init.swapChain.ndt = desc.window_handle.ndt;
    init.swapChain.width = width_;
    init.swapChain.height = height_;
    init.reset = BGFX_RESET_VSYNC;

    if (!bgfx::init(init)) {
        LCU_LOG_ERROR("bgfx::init failed");
        return false;
    }

    const bgfx::RendererType::Enum active = bgfx::getRendererType();
    LCU_LOG_INFO("bgfx initialized: backend={} headless={} resolution={}x{}",
                 bgfx::getRendererName(active), headless_, width_, height_);

    // Terrain view: no clear of its own - the sky view (below) clears
    // both color and depth, and executes first (see kSkyViewId's
    // comment), so terrain's own BGFX_STATE_DEFAULT depth test already
    // sees a freshly-cleared depth buffer without needing to clear it
    // again here (which would wipe out the sky quad the sky view just
    // drew).
    bgfx::setViewClear(0, BGFX_CLEAR_NONE, 0x303030ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, static_cast<u16>(width_), static_cast<u16>(height_));

    bgfx::setViewClear(kSkyViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
    bgfx::setViewRect(kSkyViewId, 0, 0, static_cast<u16>(width_), static_cast<u16>(height_));
    const bgfx::ViewId sky_first_order[] = {kSkyViewId, 0};
    bgfx::setViewOrder(0, 2, sky_first_order);

    bgfx::setDebug(BGFX_DEBUG_TEXT);

    // Phase 28 - vec4 because bgfx uniforms are always at least a vec4
    // internally; only .x (DayNightCycle::sky_light_scale()) is used by
    // fs_chunk.sc.
    sky_light_scale_uniform_ = bgfx::createUniform("u_skyLightScale", bgfx::UniformType::Vec4);

    initialized_ = true;
    return true;
}

namespace {

// Packs 0-1 float channels into bgfx::setViewClear's 0xRRGGBBAA u32.
u32 pack_rgba(const math::Vec3& color, f32 alpha) {
    const auto channel = [](f32 c) {
        return static_cast<u32>(std::clamp(c, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return (channel(color.x) << 24) | (channel(color.y) << 16) | (channel(color.z) << 8) | channel(alpha);
}

}  // namespace

void Renderer::begin_frame(const math::Vec3& clear_color, f32 alpha) {
    LCU_ASSERT(initialized_);

    // The sky view (executes first, see kSkyViewId) owns the actual
    // background clear now - view 0 (terrain) deliberately doesn't
    // re-clear, see init()'s comment.
    const u32 clear_rgba = pack_rgba(clear_color, alpha);
    bgfx::setViewClear(kSkyViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clear_rgba, 1.0f, 0);
    // touch() ensures a view executes its clear/state even when nothing
    // submits a draw call to it this frame (e.g. an empty chunk, no
    // shader program compiled, or no sun/moon submitted this frame).
    bgfx::touch(kSkyViewId);
    bgfx::touch(0);
}

void Renderer::submit_chunk_mesh(const GpuChunkMesh& mesh, bgfx::ProgramHandle program,
                                  const math::Mat4& model, const math::Mat4& view, const math::Mat4& proj,
                                  f32 sky_light_scale) {
    LCU_ASSERT(initialized_);
    if (!mesh.is_valid() || !bgfx::isValid(program)) {
        return;
    }

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setTransform(model.data());
    bgfx::setVertexBuffer(0, mesh.vertex_buffer);
    bgfx::setIndexBuffer(mesh.index_buffer);
    bgfx::setState(BGFX_STATE_DEFAULT);
    const f32 uniform_value[4] = {sky_light_scale, 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(sky_light_scale_uniform_, uniform_value);
    bgfx::submit(0, program);
}

void Renderer::submit_billboard(const math::Vec3& center, const math::Vec3& right, const math::Vec3& up,
                                 f32 half_size, const math::Vec3& color, bgfx::ProgramHandle program,
                                 const math::Mat4& view, const math::Mat4& proj) {
    LCU_ASSERT(initialized_);
    if (!bgfx::isValid(program)) {
        return;
    }

    // Position + color only - a dedicated minimal vertex format (see
    // client/shaders/{vs_sky,fs_sky}.sc), deliberately not
    // voxel::MeshVertex: the sky quad has no normal/UV/lighting concept,
    // and reusing the chunk shader's format+lighting for it would be
    // wrong (a light source rendering itself as "lit" makes no sense).
    struct SkyVertex {
        f32 x, y, z;
        f32 r, g, b;
    };

    const math::Vec3 v0 = center - right * half_size - up * half_size;
    const math::Vec3 v1 = center + right * half_size - up * half_size;
    const math::Vec3 v2 = center + right * half_size + up * half_size;
    const math::Vec3 v3 = center - right * half_size + up * half_size;
    const SkyVertex vertices[4] = {
        {v0.x, v0.y, v0.z, color.x, color.y, color.z},
        {v1.x, v1.y, v1.z, color.x, color.y, color.z},
        {v2.x, v2.y, v2.z, color.x, color.y, color.z},
        {v3.x, v3.y, v3.z, color.x, color.y, color.z},
    };
    // Built from the camera's own right/up (see the billboard's caller),
    // so this winding already faces the camera - no backface culling is
    // set in this draw's state below either way, so winding direction
    // doesn't actually matter for correctness here, just for consistency
    // with the rest of this codebase's convention.
    const u16 indices[6] = {0, 1, 2, 0, 2, 3};

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 3, bgfx::AttribType::Float)
        .end();

    // Transient buffers (Phase 27): this quad's position changes every
    // frame (the sun/moon move), so there's no persistent GPU buffer to
    // own the way chunk meshes have one - a fresh tiny per-frame
    // allocation from bgfx's transient buffer pool is the correct tool
    // here, not a manually managed create/destroy lifecycle for 4
    // vertices.
    if (bgfx::getAvailTransientVertexBuffer(4, layout) < 4 || bgfx::getAvailTransientIndexBuffer(6) < 6) {
        return;
    }
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientVertexBuffer(&tvb, 4, layout);
    bgfx::allocTransientIndexBuffer(&tib, 6);
    std::memcpy(tvb.data, vertices, sizeof(vertices));
    std::memcpy(tib.data, indices, sizeof(indices));

    bgfx::setViewTransform(kSkyViewId, view.data(), proj.data());
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    // No depth test, no depth write, no culling ("Tiefentest aus" - see
    // kSkyViewId's comment for why terrain still correctly occludes this
    // via view ordering instead).
    bgfx::setState(BGFX_STATE_WRITE_RGB);
    bgfx::submit(kSkyViewId, program);
}

u32 Renderer::end_frame() {
    LCU_ASSERT(initialized_);
    return bgfx::frame();
}

void Renderer::clear_debug_text() {
    LCU_ASSERT(initialized_);
    bgfx::dbgTextClear();
}

void Renderer::draw_debug_text(u16 x, u16 y, u8 color_attr, const std::string& text) {
    LCU_ASSERT(initialized_);
    // "%s" (not `text.c_str()` directly as the format string) - text may
    // come from mod-registered content or other data callers don't fully
    // control, and printf-family functions treat their format argument as
    // executable-ish (a stray "%s"/"%n" inside it would misbehave).
    bgfx::dbgTextPrintf(x, y, color_attr, "%s", text.c_str());
}

void Renderer::resize(u32 width, u32 height) {
    width_ = width;
    height_ = height;
    if (initialized_) {
        // Resizes the main swap chain's back buffer (nullptr = the one
        // bgfx::init created for us). Actual window resizing is SDL's job.
        bgfx::reset(BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, static_cast<u16>(width_), static_cast<u16>(height_));
        bgfx::setViewRect(kSkyViewId, 0, 0, static_cast<u16>(width_), static_cast<u16>(height_));
    }
}

}  // namespace lcu::rendering
