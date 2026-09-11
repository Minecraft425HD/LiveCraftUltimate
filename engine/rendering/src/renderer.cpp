#include "lcu/rendering/renderer.h"

#include <bgfx/bgfx.h>

#include <algorithm>
#include <cstring>

#include "lcu/assets/texture_atlas.h"
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

// Real 2D UI overlay (Phase 44), executed *last* (after terrain) - a
// deliberate correction of this phase's own literal "NACH Sky, VOR
// Terrain" (after sky, before terrain) wording: a UI view submitted
// before the terrain view would have every one of its pixels simply
// overdrawn the moment terrain's own opaque geometry rendered into the
// same spot, making the UI invisible wherever anything solid stood
// behind it - the opposite of what a HUD needs (visible on top of
// everything, always). "Real UI that actually appears on screen" took
// priority over the literal phase wording (see DECISIONS.md) - this
// view clears nothing of its own (BGFX_CLEAR_NONE, see init() below)
// and draws with depth testing off, so it composites cleanly over
// whatever the sky/terrain views already drew.
constexpr bgfx::ViewId kUi2dViewId = 2;

}  // namespace

Renderer::~Renderer() {
    if (initialized_) {
        if (bgfx::isValid(sky_light_scale_uniform_)) {
            bgfx::destroy(sky_light_scale_uniform_);
        }
        if (bgfx::isValid(use_textures_uniform_)) {
            bgfx::destroy(use_textures_uniform_);
        }
        if (bgfx::isValid(tile_step_uniform_)) {
            bgfx::destroy(tile_step_uniform_);
        }
        if (bgfx::isValid(tile_inset_uniform_)) {
            bgfx::destroy(tile_inset_uniform_);
        }
        if (bgfx::isValid(atlas_sampler_)) {
            bgfx::destroy(atlas_sampler_);
        }
        if (bgfx::isValid(font_sampler_)) {
            bgfx::destroy(font_sampler_);
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

    // UI view (Phase 44) - no clear of its own (draws over whatever the
    // sky/terrain views already composited, see kUi2dViewId's comment).
    bgfx::setViewClear(kUi2dViewId, BGFX_CLEAR_NONE, 0x000000ff, 1.0f, 0);
    bgfx::setViewRect(kUi2dViewId, 0, 0, static_cast<u16>(width_), static_cast<u16>(height_));

    const bgfx::ViewId view_order[] = {kSkyViewId, 0, kUi2dViewId};
    bgfx::setViewOrder(0, 3, view_order);

    bgfx::setDebug(BGFX_DEBUG_TEXT);

    // Phase 28 - vec4 because bgfx uniforms are always at least a vec4
    // internally; only .x (DayNightCycle::sky_light_scale()) is used by
    // fs_chunk.sc.
    sky_light_scale_uniform_ = bgfx::createUniform("u_skyLightScale", bgfx::UniformType::Vec4);

    // Phase 53 - texture-atlas uniforms/sampler. u_tileStep.xy is one
    // atlas grid cell's pitch (1/kTilesPerRow), .zw its real inset
    // (anti-bleed) visible width/height - both fixed atlas geometry, set
    // once here rather than recomputed per submit_chunk_mesh call.
    // u_tileInset.xy is the same inset expressed as a UV offset (added
    // to each tile's own grid origin) - see fs_chunk.sc for the exact
    // formula this feeds. See lcu::assets::texture_atlas.h for where
    // these numbers come from.
    use_textures_uniform_ = bgfx::createUniform("u_useTextures", bgfx::UniformType::Vec4);
    tile_step_uniform_ = bgfx::createUniform("u_tileStep", bgfx::UniformType::Vec4);
    tile_inset_uniform_ = bgfx::createUniform("u_tileInset", bgfx::UniformType::Vec4);
    atlas_sampler_ = bgfx::createUniform("s_atlas", bgfx::UniformType::Sampler);

    // Phase 57 - the font atlas's own separate sampler slot (bound to
    // slot 1 by flush_ui_quads, `atlas_sampler_` above stays slot 0).
    font_sampler_ = bgfx::createUniform("s_font", bgfx::UniformType::Sampler);

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
    bgfx::touch(kUi2dViewId);
}

void Renderer::submit_chunk_mesh(const GpuChunkMesh& mesh, bgfx::ProgramHandle program,
                                  const math::Mat4& model, const math::Mat4& view, const math::Mat4& proj,
                                  f32 sky_light_scale, bgfx::TextureHandle atlas_texture) {
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

    // Phase 53 - real atlas sampling, only when the caller actually
    // bound one this draw (see this function's own doc comment on why
    // atlas_texture's own validity IS u_useTextures, not a second,
    // separately-tracked toggle).
    const bool use_textures = bgfx::isValid(atlas_texture);
    const f32 use_textures_value[4] = {use_textures ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(use_textures_uniform_, use_textures_value);
    if (use_textures) {
        constexpr f32 kPitch = 1.0f / static_cast<f32>(assets::kTilesPerRow);
        constexpr f32 kInset = assets::kTileInsetTexels / static_cast<f32>(assets::kAtlasSize);
        constexpr f32 kInner = (static_cast<f32>(assets::kTileSize) - 2.0f * assets::kTileInsetTexels) /
                                static_cast<f32>(assets::kAtlasSize);
        const f32 tile_step_value[4] = {kPitch, kPitch, kInner, kInner};
        const f32 tile_inset_value[4] = {kInset, kInset, 0.0f, 0.0f};
        bgfx::setUniform(tile_step_uniform_, tile_step_value);
        bgfx::setUniform(tile_inset_uniform_, tile_inset_value);
        bgfx::setTexture(0, atlas_sampler_, atlas_texture);
    }

    bgfx::submit(0, program);
}

bgfx::TextureHandle Renderer::create_texture_from_pixels(const u8* pixels, u32 width, u32 height) {
    LCU_ASSERT(initialized_);
    const bgfx::Memory* mem = bgfx::copy(pixels, width * height * 4);
    // BGFX_SAMPLER_POINT (nearest filtering) + BGFX_SAMPLER_[UVW_]CLAMP
    // baked into the texture itself, per Phase 53.1's own "Nearest-
    // Filter, Clamp-Mode" requirement - see this function's own doc
    // comment in renderer.h.
    return bgfx::createTexture2D(static_cast<u16>(width), static_cast<u16>(height), false, 1,
                                  bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP, mem);
}

void Renderer::destroy_texture(bgfx::TextureHandle handle) {
    if (bgfx::isValid(handle)) {
        bgfx::destroy(handle);
    }
}

void Renderer::submit_billboard(const math::Vec3& center, const math::Vec3& right, const math::Vec3& up,
                                 f32 half_size, const math::Vec3& color, bgfx::ProgramHandle program,
                                 const math::Mat4& view, const math::Mat4& proj) {
    LCU_ASSERT(initialized_);
    if (!bgfx::isValid(program)) {
        return;
    }

    // Position + color, plus a real atlas UV/"use texture" pair (Phase
    // 56) - always zero for this function (the sun/moon are never
    // textured), but part of the one shared vertex format/layout every
    // real vs_sky.sc/fs_sky.sc caller here uses (see fs_sky.sc and
    // submit_world_billboard's own doc comment for the one real caller
    // that DOES set these to something real). Deliberately not
    // voxel::MeshVertex: this quad has no normal/lighting concept, and
    // reusing the chunk shader's format+lighting for it would be wrong
    // (a light source rendering itself as "lit" makes no sense).
    struct SkyVertex {
        f32 x, y, z;
        f32 r, g, b;
        f32 u, v;
        f32 use_texture;
    };

    const math::Vec3 v0 = center - right * half_size - up * half_size;
    const math::Vec3 v1 = center + right * half_size - up * half_size;
    const math::Vec3 v2 = center + right * half_size + up * half_size;
    const math::Vec3 v3 = center - right * half_size + up * half_size;
    const SkyVertex vertices[4] = {
        {v0.x, v0.y, v0.z, color.x, color.y, color.z, 0.0f, 0.0f, 0.0f},
        {v1.x, v1.y, v1.z, color.x, color.y, color.z, 0.0f, 0.0f, 0.0f},
        {v2.x, v2.y, v2.z, color.x, color.y, color.z, 0.0f, 0.0f, 0.0f},
        {v3.x, v3.y, v3.z, color.x, color.y, color.z, 0.0f, 0.0f, 0.0f},
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
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
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

void Renderer::submit_wireframe_box(const math::Vec3& min, const math::Vec3& max, const math::Vec3& color,
                                     bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj) {
    LCU_ASSERT(initialized_);
    if (!bgfx::isValid(program)) {
        return;
    }

    // Same minimal vertex format submit_billboard's SkyVertex already
    // uses - position + flat color + a real, always-zero atlas UV/flag
    // pair (Phase 56, see SkyVertex's own doc comment).
    struct LineVertex {
        f32 x, y, z;
        f32 r, g, b;
        f32 u, v;
        f32 use_texture;
    };

    const math::Vec3 corners[8] = {
        {min.x, min.y, min.z}, {max.x, min.y, min.z}, {max.x, max.y, min.z}, {min.x, max.y, min.z},
        {min.x, min.y, max.z}, {max.x, min.y, max.z}, {max.x, max.y, max.z}, {min.x, max.y, max.z},
    };
    LineVertex vertices[8];
    for (u32 i = 0; i < 8; ++i) {
        vertices[i] = {corners[i].x, corners[i].y, corners[i].z, color.x, color.y, color.z, 0.0f, 0.0f, 0.0f};
    }
    // 12 edges of a box, each as one line-list segment (2 indices) -
    // bottom face, top face, then the 4 verticals connecting them.
    const u16 indices[24] = {
        0, 1, 1, 2, 2, 3, 3, 0,  // bottom face (min.y)
        4, 5, 5, 6, 6, 7, 7, 4,  // top face (max.y)
        0, 4, 1, 5, 2, 6, 3, 7,  // verticals
    };

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
        .end();

    // Transient buffers, same reasoning as submit_billboard: an
    // entity's box moves every frame, so there's no persistent GPU
    // buffer worth owning for it.
    if (bgfx::getAvailTransientVertexBuffer(8, layout) < 8 || bgfx::getAvailTransientIndexBuffer(24) < 24) {
        return;
    }
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientVertexBuffer(&tvb, 8, layout);
    bgfx::allocTransientIndexBuffer(&tib, 24);
    std::memcpy(tvb.data, vertices, sizeof(vertices));
    std::memcpy(tib.data, indices, sizeof(indices));

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    // Real depth test against terrain (BGFX_STATE_DEPTH_TEST_LESS),
    // but no depth write - the thin line geometry shouldn't leave a
    // lasting mark in the depth buffer other draws test against, the
    // same reasoning a debug aid overlay generally wants. BGFX_STATE_
    // PT_LINES draws this vertex/index data as a line list instead of
    // the default triangle list.
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_PT_LINES);
    bgfx::submit(0, program);
}

void Renderer::submit_solid_box(const math::Vec3& min, const math::Vec3& max, const math::Vec3& color,
                                 bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj) {
    LCU_ASSERT(initialized_);
    if (!bgfx::isValid(program)) {
        return;
    }

    struct BoxVertex {
        f32 x, y, z;
        f32 r, g, b;
        f32 u, v;
        f32 use_texture;
    };

    const math::Vec3 corners[8] = {
        {min.x, min.y, min.z}, {max.x, min.y, min.z}, {max.x, max.y, min.z}, {min.x, max.y, min.z},
        {min.x, min.y, max.z}, {max.x, min.y, max.z}, {max.x, max.y, max.z}, {min.x, max.y, max.z},
    };
    BoxVertex vertices[8];
    for (u32 i = 0; i < 8; ++i) {
        vertices[i] = {corners[i].x, corners[i].y, corners[i].z, color.x, color.y, color.z, 0.0f, 0.0f, 0.0f};
    }
    // 12 triangles, 2 per face, standard box winding (outward-facing,
    // though no backface culling is set below, matching
    // submit_billboard's own "winding doesn't actually matter here"
    // note - consistency with the rest of this codebase's convention).
    const u16 indices[36] = {
        0, 1, 2, 0, 2, 3,  // -Z (min.z) face
        5, 4, 7, 5, 7, 6,  // +Z (max.z) face
        4, 0, 3, 4, 3, 7,  // -X (min.x) face
        1, 5, 6, 1, 6, 2,  // +X (max.x) face
        4, 5, 1, 4, 1, 0,  // -Y (min.y) face
        3, 2, 6, 3, 6, 7,  // +Y (max.y) face
    };

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
        .end();

    if (bgfx::getAvailTransientVertexBuffer(8, layout) < 8 || bgfx::getAvailTransientIndexBuffer(36) < 36) {
        return;
    }
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientVertexBuffer(&tvb, 8, layout);
    bgfx::allocTransientIndexBuffer(&tib, 36);
    std::memcpy(tvb.data, vertices, sizeof(vertices));
    std::memcpy(tib.data, indices, sizeof(indices));

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    // Real depth test against terrain, no depth write - same reasoning
    // submit_wireframe_box's own comment gives: a real per-frame overlay
    // shouldn't leave a lasting mark in the depth buffer. Default
    // triangle-list topology (no BGFX_STATE_PT_LINES), so this draws a
    // real filled box, not an outline.
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS);
    bgfx::submit(0, program);
}

void Renderer::submit_textured_box(const std::array<math::Vec3, 8>& corners, const math::Vec3& color,
                                    bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj,
                                    bgfx::TextureHandle atlas_texture, const BoxUvSet& uvs, bool alpha_blend) {
    LCU_ASSERT(initialized_);
    if (!bgfx::isValid(program)) {
        return;
    }

    struct BoxVertex {
        f32 x, y, z;
        f32 r, g, b;
        f32 u, v;
        f32 use_texture;
    };

    const bool use_texture = bgfx::isValid(atlas_texture);
    const f32 use_texture_value = use_texture ? 1.0f : 0.0f;

    // Same corner indices/winding submit_solid_box's own min/max-derived
    // table uses, but 4 fresh vertices per face (not 8 shared corners)
    // so each face can carry its own independent UV rect - see this
    // function's own doc comment in renderer.h.
    const auto face = [&](u32 i0, u32 i1, u32 i2, u32 i3, const BoxFaceUv& uv) {
        const math::Vec3& p0 = corners[i0];
        const math::Vec3& p1 = corners[i1];
        const math::Vec3& p2 = corners[i2];
        const math::Vec3& p3 = corners[i3];
        return std::array<BoxVertex, 4>{
            BoxVertex{p0.x, p0.y, p0.z, color.x, color.y, color.z, uv.u0, uv.v0, use_texture_value},
            BoxVertex{p1.x, p1.y, p1.z, color.x, color.y, color.z, uv.u1, uv.v0, use_texture_value},
            BoxVertex{p2.x, p2.y, p2.z, color.x, color.y, color.z, uv.u1, uv.v1, use_texture_value},
            BoxVertex{p3.x, p3.y, p3.z, color.x, color.y, color.z, uv.u0, uv.v1, use_texture_value},
        };
    };

    const std::array<std::array<BoxVertex, 4>, 6> faces = {
        face(0, 1, 2, 3, uvs.neg_z),  // -Z (min.z) face
        face(5, 4, 7, 6, uvs.pos_z),  // +Z (max.z) face
        face(4, 0, 3, 7, uvs.neg_x),  // -X (min.x) face
        face(1, 5, 6, 2, uvs.pos_x),  // +X (max.x) face
        face(4, 5, 1, 0, uvs.neg_y),  // -Y (min.y) face
        face(3, 2, 6, 7, uvs.pos_y),  // +Y (max.y) face
    };

    std::array<BoxVertex, 24> vertices{};
    for (u32 f = 0; f < 6; ++f) {
        for (u32 v = 0; v < 4; ++v) {
            vertices[f * 4 + v] = faces[f][v];
        }
    }
    std::array<u16, 36> indices{};
    for (u32 f = 0; f < 6; ++f) {
        const u16 base = static_cast<u16>(f * 4);
        const u16 face_indices[6] = {base, static_cast<u16>(base + 1), static_cast<u16>(base + 2),
                                      base, static_cast<u16>(base + 2), static_cast<u16>(base + 3)};
        std::memcpy(&indices[f * 6], face_indices, sizeof(face_indices));
    }

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
        .end();

    if (bgfx::getAvailTransientVertexBuffer(24, layout) < 24 || bgfx::getAvailTransientIndexBuffer(36) < 36) {
        return;
    }
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientVertexBuffer(&tvb, 24, layout);
    bgfx::allocTransientIndexBuffer(&tib, 36);
    std::memcpy(tvb.data, vertices.data(), vertices.size() * sizeof(BoxVertex));
    std::memcpy(tib.data, indices.data(), indices.size() * sizeof(u16));

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    if (use_texture) {
        bgfx::setTexture(0, atlas_sampler_, atlas_texture);
    }
    // Real alpha blending (Phase 60) for the one real caller that needs
    // it (the break-progress crack overlay) - see this function's own
    // doc comment in renderer.h. No depth write either way, matching
    // every other real per-frame world-space primitive here.
    const u64 blend_state = alpha_blend ? BGFX_STATE_BLEND_ALPHA : 0;
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | blend_state);
    bgfx::submit(0, program);
}

void Renderer::submit_world_billboard(const math::Vec3& center, const math::Vec3& right, const math::Vec3& up,
                                       f32 half_size, const math::Vec3& color, bgfx::ProgramHandle program,
                                       const math::Mat4& view, const math::Mat4& proj,
                                       bgfx::TextureHandle atlas_texture, f32 u0, f32 v0, f32 u1, f32 v1) {
    LCU_ASSERT(initialized_);
    if (!bgfx::isValid(program)) {
        return;
    }

    // Same minimal vertex format every other flat-color debug/world
    // primitive here already uses (see submit_billboard's own doc
    // comment on why not voxel::MeshVertex) - the one real caller (this
    // function) that ever sets the trailing UV/use_texture pair to
    // something other than all-zero (Phase 56 - see this function's own
    // doc comment in renderer.h).
    struct BillboardVertex {
        f32 x, y, z;
        f32 r, g, b;
        f32 u, v;
        f32 use_texture;
    };

    const bool use_texture = bgfx::isValid(atlas_texture);
    const f32 use_texture_value = use_texture ? 1.0f : 0.0f;

    const math::Vec3 v0_pos = center - right * half_size - up * half_size;
    const math::Vec3 v1_pos = center + right * half_size - up * half_size;
    const math::Vec3 v2_pos = center + right * half_size + up * half_size;
    const math::Vec3 v3_pos = center - right * half_size + up * half_size;
    const BillboardVertex vertices[4] = {
        {v0_pos.x, v0_pos.y, v0_pos.z, color.x, color.y, color.z, u0, v0, use_texture_value},
        {v1_pos.x, v1_pos.y, v1_pos.z, color.x, color.y, color.z, u1, v0, use_texture_value},
        {v2_pos.x, v2_pos.y, v2_pos.z, color.x, color.y, color.z, u1, v1, use_texture_value},
        {v3_pos.x, v3_pos.y, v3_pos.z, color.x, color.y, color.z, u0, v1, use_texture_value},
    };
    const u16 indices[6] = {0, 1, 2, 0, 2, 3};

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
        .end();

    if (bgfx::getAvailTransientVertexBuffer(4, layout) < 4 || bgfx::getAvailTransientIndexBuffer(6) < 6) {
        return;
    }
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientVertexBuffer(&tvb, 4, layout);
    bgfx::allocTransientIndexBuffer(&tib, 6);
    std::memcpy(tvb.data, vertices, sizeof(vertices));
    std::memcpy(tib.data, indices, sizeof(indices));

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    if (use_texture) {
        bgfx::setTexture(0, atlas_sampler_, atlas_texture);
    }
    // Real depth test against terrain, no depth write - same reasoning
    // submit_wireframe_box/submit_solid_box's own comments give for a
    // per-frame, moving object; drawn into view 0 (terrain), not
    // kSkyViewId, so it's genuinely occluded by/occludes real geometry.
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS);
    bgfx::submit(0, program);
}

void Renderer::submit_ui_quad(f32 x, f32 y, f32 width, f32 height, const math::Vec4& color) {
    LCU_ASSERT(initialized_);

    const f32 left = x;
    const f32 right = x + width;
    const f32 top = y;
    const f32 bottom = y + height;
    const UiVertex2D v0{left, top, 0.0f, 0.0f, color.x, color.y, color.z, color.w, 0.0f};
    const UiVertex2D v1{right, top, 1.0f, 0.0f, color.x, color.y, color.z, color.w, 0.0f};
    const UiVertex2D v2{right, bottom, 1.0f, 1.0f, color.x, color.y, color.z, color.w, 0.0f};
    const UiVertex2D v3{left, bottom, 0.0f, 1.0f, color.x, color.y, color.z, color.w, 0.0f};

    const auto base = static_cast<u16>(ui_vertices_.size());
    ui_vertices_.push_back(v0);
    ui_vertices_.push_back(v1);
    ui_vertices_.push_back(v2);
    ui_vertices_.push_back(v3);
    const u16 quad_indices[6] = {base, static_cast<u16>(base + 1), static_cast<u16>(base + 2),
                                  base, static_cast<u16>(base + 2), static_cast<u16>(base + 3)};
    ui_indices_.insert(ui_indices_.end(), std::begin(quad_indices), std::end(quad_indices));
}

void Renderer::submit_textured_ui_quad(f32 x, f32 y, f32 width, f32 height, const math::Vec4& color, f32 u0, f32 v0,
                                        f32 u1, f32 v1) {
    LCU_ASSERT(initialized_);

    const f32 left = x;
    const f32 right = x + width;
    const f32 top = y;
    const f32 bottom = y + height;
    const UiVertex2D tv0{left, top, u0, v0, color.x, color.y, color.z, color.w, 1.0f};
    const UiVertex2D tv1{right, top, u1, v0, color.x, color.y, color.z, color.w, 1.0f};
    const UiVertex2D tv2{right, bottom, u1, v1, color.x, color.y, color.z, color.w, 1.0f};
    const UiVertex2D tv3{left, bottom, u0, v1, color.x, color.y, color.z, color.w, 1.0f};

    const auto base = static_cast<u16>(ui_vertices_.size());
    ui_vertices_.push_back(tv0);
    ui_vertices_.push_back(tv1);
    ui_vertices_.push_back(tv2);
    ui_vertices_.push_back(tv3);
    const u16 quad_indices[6] = {base, static_cast<u16>(base + 1), static_cast<u16>(base + 2),
                                  base, static_cast<u16>(base + 2), static_cast<u16>(base + 3)};
    ui_indices_.insert(ui_indices_.end(), std::begin(quad_indices), std::end(quad_indices));
}

void Renderer::submit_text_glyph_quad(f32 x, f32 y, f32 width, f32 height, const math::Vec4& color, f32 u0, f32 v0,
                                       f32 u1, f32 v1) {
    LCU_ASSERT(initialized_);

    const f32 left = x;
    const f32 right = x + width;
    const f32 top = y;
    const f32 bottom = y + height;
    const UiVertex2D gv0{left, top, u0, v0, color.x, color.y, color.z, color.w, 2.0f};
    const UiVertex2D gv1{right, top, u1, v0, color.x, color.y, color.z, color.w, 2.0f};
    const UiVertex2D gv2{right, bottom, u1, v1, color.x, color.y, color.z, color.w, 2.0f};
    const UiVertex2D gv3{left, bottom, u0, v1, color.x, color.y, color.z, color.w, 2.0f};

    const auto base = static_cast<u16>(ui_vertices_.size());
    ui_vertices_.push_back(gv0);
    ui_vertices_.push_back(gv1);
    ui_vertices_.push_back(gv2);
    ui_vertices_.push_back(gv3);
    const u16 quad_indices[6] = {base, static_cast<u16>(base + 1), static_cast<u16>(base + 2),
                                  base, static_cast<u16>(base + 2), static_cast<u16>(base + 3)};
    ui_indices_.insert(ui_indices_.end(), std::begin(quad_indices), std::end(quad_indices));
}

void Renderer::flush_ui_quads(bgfx::ProgramHandle program, bgfx::TextureHandle atlas_texture,
                               bgfx::TextureHandle font_atlas_texture) {
    LCU_ASSERT(initialized_);

    if (bgfx::isValid(program) && !ui_vertices_.empty()) {
        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
            .end();

        const auto vertex_count = static_cast<u32>(ui_vertices_.size());
        const auto index_count = static_cast<u32>(ui_indices_.size());
        if (bgfx::getAvailTransientVertexBuffer(vertex_count, layout) >= vertex_count &&
            bgfx::getAvailTransientIndexBuffer(index_count) >= index_count) {
            bgfx::TransientVertexBuffer tvb;
            bgfx::TransientIndexBuffer tib;
            bgfx::allocTransientVertexBuffer(&tvb, vertex_count, layout);
            bgfx::allocTransientIndexBuffer(&tib, index_count);
            std::memcpy(tvb.data, ui_vertices_.data(), ui_vertices_.size() * sizeof(UiVertex2D));
            std::memcpy(tib.data, ui_indices_.data(), ui_indices_.size() * sizeof(u16));

            // Top-left origin, y increasing downward (screen-space
            // convention every submit_ui_quad caller already uses) -
            // see Mat4::orthographic's own doc comment for the mapping.
            const math::Mat4 proj =
                math::Mat4::orthographic(0.0f, static_cast<f32>(width_), static_cast<f32>(height_), 0.0f, -1.0f, 1.0f);
            bgfx::setViewTransform(kUi2dViewId, math::Mat4::identity().data(), proj.data());
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(&tib);
            // Real atlas binding (Phase 56) - only when actually handed
            // one; every queued quad's own per-vertex use_texture flag
            // (set by submit_ui_quad vs. submit_textured_ui_quad) is
            // what actually decides whether fs_ui2d.sc samples it, same
            // "invalid handle, never sampled" contract every other real
            // atlas_texture parameter in this class establishes.
            if (bgfx::isValid(atlas_texture)) {
                bgfx::setTexture(0, atlas_sampler_, atlas_texture);
            }
            // Real font-atlas binding (Phase 57) - same "only bound
            // when actually handed one" contract as atlas_texture
            // above, its own separate slot (1, s_font) so both atlases
            // can be sampled within this one draw call.
            if (bgfx::isValid(font_atlas_texture)) {
                bgfx::setTexture(1, font_sampler_, font_atlas_texture);
            }
            // No depth test/write (2D overlay, always on top - see
            // kUi2dViewId's own comment), real alpha blending (a menu
            // background or a semi-transparent slot highlight is real,
            // near-term future use for this, not speculative - see
            // TASK_QUEUE.md Phase 46/47+).
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(kUi2dViewId, program);
        }
    }

    ui_vertices_.clear();
    ui_indices_.clear();
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

void Renderer::request_screenshot(const std::string& file_path_without_extension) {
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, file_path_without_extension.c_str());
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
        bgfx::setViewRect(kUi2dViewId, 0, 0, static_cast<u16>(width_), static_cast<u16>(height_));
    }
}

}  // namespace lcu::rendering
