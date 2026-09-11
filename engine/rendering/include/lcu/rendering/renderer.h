#pragma once

#include <array>
#include <string>
#include <vector>

#include <bgfx/bgfx.h>

#include "lcu/core/types.h"
#include "lcu/math/mat4.h"
#include "lcu/math/vec4.h"
#include "lcu/platform/native_handle.h"
#include "lcu/rendering/chunk_mesh_upload.h"

namespace lcu::rendering {

struct RendererDesc {
    platform::NativeWindowHandle window_handle;
    u32 width = 1280;
    u32 height = 720;
    // Forces bgfx's headless Noop backend regardless of window_handle -
    // used for CI/sandbox verification where no real GPU/display exists.
    // When false and window_handle.nwh is null, Renderer::init falls back
    // to Noop automatically anyway (there is nothing else it could do).
    bool force_headless = false;
};

// One 2D UI vertex (Phase 44): screen-space position (pixels) + UV (0..1
// within its own quad) + straight-alpha RGBA color - the real vertex
// format submit_ui_quad/flush_ui_quads batch, mirroring the position+
// color-only SkyVertex/LineVertex structs renderer.cpp already defines
// for the 3D debug/sky draws, with UV added since 2D UI has a real
// future use for it (a pattern/atlas lookup) those don't.
struct UiVertex2D {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 u = 0.0f;
    f32 v = 0.0f;
    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    f32 a = 0.0f;
    // Real per-vertex sample mode (Phase 56, extended Phase 57) - 0 for
    // a flat-color quad (submit_ui_quad, unchanged since Phase 44:
    // borders, backgrounds, health/hunger bars), 1 for a real item-icon
    // quad (submit_textured_ui_quad, samples the block/item atlas's own
    // RGB as-is), 2 for a real font-glyph quad (submit_text_glyph_quad,
    // samples the SEPARATE font atlas and multiplies its RGB by this
    // vertex's own color - see that method's doc comment). Whichever
    // mode, (u,v) above is a real atlas sample rect for that quad's own
    // atlas, not a per-quad-local 0..1 UV, except mode 0 where it's
    // unused. Lets all three kinds of quad share the same batch/single
    // draw call - fs_ui2d.sc's own mix chain picks the right one.
    f32 use_texture = 0.0f;
};

// Thin wrapper around bgfx's global init/frame/shutdown lifecycle. This is
// the only engine subsystem below the client allowed to include bgfx
// headers, per ARCHITECTURE.md's rendering abstraction layering
// (game -> engine -> engine/rendering -> bgfx -> platform backend).
class Renderer : public NonCopyable {
   public:
    Renderer() = default;
    ~Renderer();

    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // Returns false if bgfx failed to initialize (logged). Safe to call
    // once per Renderer instance.
    bool init(const RendererDesc& desc);

    // Clears view 0 to `color` (each channel 0-1, alpha separate - Phase
    // 27's real sky color, interpolated from DayNightCycle::
    // sky_light_scale() by the caller, replaces the fixed
    // brief-Phase-1-era sky-blue constant this used to always be) and
    // marks it touched. Call once per frame, before any
    // submit_chunk_mesh() calls, then end_frame() after.
    void begin_frame(const math::Vec3& clear_color, f32 alpha = 1.0f);

    // Draws one chunk mesh with the given program/transforms into view 0.
    // No-op (logged at debug level would be noisy per-frame - silently
    // skipped) if `mesh` has no geometry or `program` is invalid, both
    // of which are legitimate states today (an empty chunk; no shader
    // compiled because LCU_BUILD_SHADER_TOOLS is off - see BUILDING.md).
    // `sky_light_scale` (Phase 28) is DayNightCycle::sky_light_scale()
    // for the frame being drawn - the one real time signal fs_chunk.sc's
    // u_skyLightScale uniform needs to weight each vertex's already-
    // computed, already-packed sky light by; set once per draw call
    // here, never recomputed per-voxel/per-frame in meshing itself (see
    // DECISIONS.md "Light is never computed per frame").
    // `atlas_texture` (Phase 53) - a texture created via create_texture_
    // from_pixels, or an invalid handle (the default) to render fully
    // procedurally, exactly as every prior phase already did. Only when
    // valid does fs_chunk.sc actually sample it (`u_useTextures` is set
    // from this, not a separate flag - a real, always-in-sync single
    // source of truth: no atlas bound genuinely means no atlas to
    // sample, not a caller-managed toggle that could drift out of sync
    // with what's actually bound).
    void submit_chunk_mesh(const GpuChunkMesh& mesh, bgfx::ProgramHandle program, const math::Mat4& model,
                            const math::Mat4& view, const math::Mat4& proj, f32 sky_light_scale = 1.0f,
                            bgfx::TextureHandle atlas_texture = BGFX_INVALID_HANDLE);

    // Real GPU texture upload (Phase 53) - `pixels` is a tightly packed
    // RGBA8 buffer, `width*height*4` bytes, row-major top-to-bottom (the
    // same layout engine/assets::procedural_textures - Phase 54 - packs
    // its atlas buffer in). Nearest-filter, clamp-addressed (Phase
    // 53.1's own "Nearest-Filter, Clamp-Mode" requirement) - baked into
    // the texture's own creation flags, not a separate per-draw sampler
    // state, since every real consumer of a texture this project creates
    // wants exactly that (a pixel-art atlas, never filtered/wrapped).
    // Returns an invalid handle (bgfx::isValid == false) if bgfx itself
    // is Noop-backed and rejects it - a real, legitimate possibility
    // this sandbox's own headless runs can hit, not treated as fatal by
    // any caller (see submit_chunk_mesh's own "invalid atlas_texture
    // means fully procedural" fallback above).
    bgfx::TextureHandle create_texture_from_pixels(const u8* pixels, u32 width, u32 height);

    // Destroys a texture created via create_texture_from_pixels above.
    // No-op if `handle` is already invalid (same "safe to call on an
    // already-empty/never-created resource" convention destroy_gpu_
    // chunk_mesh already establishes).
    void destroy_texture(bgfx::TextureHandle handle);

    // Draws one camera-facing colored quad (Phase 27 - the sun/moon)
    // into a dedicated sky view, executed before the terrain view so
    // terrain naturally occludes it (see renderer.cpp's kSkyViewId
    // comment) - no depth test/write, no texture (no atlas exists yet,
    // same as chunk rendering before Phase 26). `right`/`up` should be
    // the camera's own basis vectors so the quad actually faces the
    // camera; `center` is the quad's world-space position, `half_size`
    // its half-width/height in world units. No-op if `program` is
    // invalid (e.g. LCU_BUILD_SHADER_TOOLS is off).
    void submit_billboard(const math::Vec3& center, const math::Vec3& right, const math::Vec3& up, f32 half_size,
                           const math::Vec3& color, bgfx::ProgramHandle program, const math::Mat4& view,
                           const math::Mat4& proj);

    // Draws an axis-aligned wireframe box (Phase 36 - entity debug
    // boxes, brief section 60) as 12 line-list edges from `min` to
    // `max` in world space, tinted `color`. Reuses the same minimal
    // position+color vertex format/shader submit_billboard already
    // established (vs_sky.sc/fs_sky.sc - unlit, no lighting concept
    // needed for a debug aid) rather than adding a third shader pair
    // for what's visually the same "flat-colored, no lighting" need.
    // Drawn into view 0 (the terrain view, not the sky view) with real
    // depth testing against solid terrain - a box behind a wall is
    // correctly hidden, not painted through it. No-op if `program` is
    // invalid (e.g. LCU_BUILD_SHADER_TOOLS is off).
    void submit_wireframe_box(const math::Vec3& min, const math::Vec3& max, const math::Vec3& color,
                               bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj);

    // Draws an axis-aligned SOLID box (Phase 48's real break-progress
    // overlay - the targeted block darkens as break progress advances).
    // Reuses the exact same minimal position+color vertex format/shader
    // submit_wireframe_box/submit_billboard already established, just
    // with the default triangle-list topology (a real box, 12
    // triangles) and real depth testing against terrain instead of a
    // line list. No-op if `program` is invalid.
    void submit_solid_box(const math::Vec3& min, const math::Vec3& max, const math::Vec3& color,
                           bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj);

    // Draws one camera-facing colored quad into the terrain view (view
    // 0) WITH real depth testing against solid terrain (Phase 50's real
    // item-entity rendering) - the same camera-facing quad math
    // submit_billboard already uses, but genuinely occluded by/occluding
    // against nearby geometry the way a real object living in the game
    // world needs, unlike submit_billboard's own sky view (no depth
    // test - correct for a sun/moon "at infinity", wrong for a dropped
    // item sitting on the ground behind a wall). No depth *write*, same
    // reasoning submit_wireframe_box/submit_solid_box's own comments
    // give for a per-frame, moving object. No-op if `program` is
    // invalid.
    // `atlas_texture`/`u0`/`v0`/`u1`/`v1` (Phase 56, all defaulted):
    // when `atlas_texture` is valid, samples the real atlas rect
    // (`u0`,`v0`)-(`u1`,`v1`) instead of drawing flat `color` - `color`'s
    // own alpha still multiplies the sample's alpha (a real dropped
    // torch's transparent background composites correctly). Invalid
    // `atlas_texture` (the default) draws exactly the flat-color quad
    // every dropped item rendered before Phase 56 - the same real "no
    // atlas bound, no texture sampled" contract every other real
    // `atlas_texture` parameter in this class establishes.
    void submit_world_billboard(const math::Vec3& center, const math::Vec3& right, const math::Vec3& up,
                                 f32 half_size, const math::Vec3& color, bgfx::ProgramHandle program,
                                 const math::Mat4& view, const math::Mat4& proj,
                                 bgfx::TextureHandle atlas_texture = BGFX_INVALID_HANDLE, f32 u0 = 0.0f,
                                 f32 v0 = 0.0f, f32 u1 = 1.0f, f32 v1 = 1.0f);

    // One face's real UV rect for submit_textured_box below.
    struct BoxFaceUv {
        f32 u0 = 0.0f;
        f32 v0 = 0.0f;
        f32 u1 = 1.0f;
        f32 v1 = 1.0f;
    };
    // All 6 real faces of a box (Phase 58, the player/NPC character
    // model + first-person arm) - unlike submit_solid_box's shared 8
    // corners, each face gets its OWN 4 vertices so it can carry its own
    // independent UV rect (a real Minecraft-format skin needs a
    // different texture region per face, e.g. a torso's front is not
    // its back - see lcu::assets::skin_texture.h).
    struct BoxUvSet {
        BoxFaceUv neg_x, pos_x, neg_y, pos_y, neg_z, pos_z;
    };

    // Draws an arbitrary (not necessarily axis-aligned) textured box
    // from 8 real world-space corners the caller already computed
    // (Phase 58) - `corners` follows the exact same index convention
    // submit_solid_box's own `min`/`max` corner table uses (0..3 the
    // "negative Z" face, 4..7 the "positive Z" face, in the same
    // min/min/min .. max/max/max winding order), so any code that
    // already knows how to build an axis-aligned box's 8 corners can
    // feed this directly. Rotation (a character model's own body-yaw/
    // head-pitch) is entirely the CALLER's job - this function only
    // ever draws the 8 positions it's handed, using the exact same
    // vs_sky.sc/fs_sky.sc pipeline/texture-binding contract
    // submit_world_billboard already established (an invalid
    // `atlas_texture` draws flat `color` instead, same honest
    // fallback). Real depth test against terrain, no depth write - same
    // reasoning every other per-frame world-space primitive here gives.
    void submit_textured_box(const std::array<math::Vec3, 8>& corners, const math::Vec3& color,
                              bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj,
                              bgfx::TextureHandle atlas_texture, const BoxUvSet& uvs);

    // Real 2D UI quad batch (Phase 44, brief section 60's UI framework):
    // appends one screen-space rectangle - `x`/`y`/`width`/`height` in
    // pixels, top-left origin, y increasing downward (SDL/mouse
    // convention) - to this frame's pending batch, tinted `color`
    // (straight, not premultiplied, alpha). Nothing is actually drawn
    // yet: call flush_ui_quads() once per frame (after every
    // submit_ui_quad() call for that frame, before end_frame()) to
    // upload the whole batch and issue exactly one real draw call for
    // however many quads were queued - the real "Quad-Batch...ein
    // Draw-Call" behavior the brief asks for, not one draw call per
    // quad. UV runs 0..1 across each quad independently, unused by
    // fs_ui2d.sc unless a texture is actually sampled (see
    // submit_textured_ui_quad below - this call always draws flat
    // `color`, no atlas involved, the same real behavior every quad had
    // before Phase 56).
    void submit_ui_quad(f32 x, f32 y, f32 width, f32 height, const math::Vec4& color);

    // Real atlas-textured UI quad (Phase 56) - same real batch/one-
    // draw-call contract as submit_ui_quad above, but samples
    // `s_atlas` at the real rect (`u0`,`v0`)-(`u1`,`v1`) (from
    // lcu::assets::tile_uv_range) instead of drawing flat `color` -
    // `color`'s own alpha still multiplies the sampled texture's alpha
    // (so a torch icon's real transparent background composites
    // correctly), its RGB is otherwise unused once textured. Whichever
    // atlas texture flush_ui_quads() below is actually handed this
    // frame is what gets sampled - this call only queues the quad/UV
    // data, same "nothing drawn yet" contract as submit_ui_quad.
    void submit_textured_ui_quad(f32 x, f32 y, f32 width, f32 height, const math::Vec4& color, f32 u0, f32 v0, f32 u1,
                                  f32 v1);

    // Real font-atlas glyph quad (Phase 57) - same real batch/one-draw-
    // call contract as submit_ui_quad/submit_textured_ui_quad above,
    // but samples the SEPARATE font atlas (`s_font`, bound by
    // flush_ui_quads' own `font_atlas_texture` param below) instead of
    // the block/item atlas `submit_textured_ui_quad` samples. Unlike
    // that call, `color`'s RGB IS applied (multiplied against the
    // sampled glyph's own white-on-transparent pixels), not just its
    // alpha - a font atlas is deliberately colorless (see
    // lcu::assets::generate_glyph_pixels) so one glyph texture can be
    // tinted to any real text color at draw time, the same real
    // "colorless glyph, tinted at draw time" technique any bitmap-font
    // renderer uses. `engine::ui::TextRenderer` is the one real caller
    // - most code should go through that, not this directly.
    void submit_text_glyph_quad(f32 x, f32 y, f32 width, f32 height, const math::Vec4& color, f32 u0, f32 v0, f32 u1,
                                 f32 v1);

    // How many quads are currently queued (real, testable state - not
    // just an implementation detail): 0 right after flush_ui_quads() or
    // before any submit_ui_quad() call this frame.
    usize pending_ui_quad_count() const { return ui_vertices_.size() / 4; }

    // Uploads every quad queued via submit_ui_quad() since the last
    // flush and issues one real bgfx::submit() into the dedicated UI
    // view (drawn last, after terrain - see renderer.cpp's kUi2dViewId
    // comment for why, a deliberate correction of this phase's own
    // literal "before terrain" wording, which would make the UI
    // invisible behind opaque terrain). Always clears the pending batch
    // before returning, whether or not it actually drew anything (an
    // invalid `program` - e.g. LCU_BUILD_SHADER_TOOLS is off - means
    // this frame's queued quads are silently skipped, the same "no
    // shader, no draw" behavior submit_chunk_mesh/submit_billboard
    // already have, not something that should re-appear stale on a
    // later frame once a program becomes valid). Call once per frame,
    // after every submit_ui_quad() for that frame, before end_frame().
    // `atlas_texture` (Phase 56) - same real "invalid handle means no
    // atlas bound, every textured quad's own u_useTextures mix just
    // reads garbage no submit_textured_ui_quad call this frame should
    // have produced" contract Renderer::submit_chunk_mesh's own
    // `atlas_texture` parameter already establishes; pass the same
    // texture handle here as there, or an invalid one if `LCU_USE_
    // TEXTURES` is off (see client/main.cpp). `font_atlas_texture`
    // (Phase 57) is the SAME kind of contract for submit_text_glyph_
    // quad's own queued quads - a separate real texture bound to a
    // separate sampler slot (`s_font`), since the font atlas is its own
    // texture, not packed into the block/item atlas (see
    // lcu::assets::font_atlas.h). Both atlases can be bound in the same
    // draw call - fs_ui2d.sc's own per-vertex mode selects which one (if
    // either) a given quad actually samples.
    void flush_ui_quads(bgfx::ProgramHandle program, bgfx::TextureHandle atlas_texture = BGFX_INVALID_HANDLE,
                         bgfx::TextureHandle font_atlas_texture = BGFX_INVALID_HANDLE);

    // Advances one bgfx frame. Returns the frame count bgfx reports,
    // mainly useful for tests/logging.
    u32 end_frame();

    // Real on-screen text (Phase 12, brief section 60's debug overlay/
    // engine/ui): bgfx's built-in VGA-style debug-text character buffer
    // (BGFX_DEBUG_TEXT, enabled in init()) - no font/texture-atlas
    // renderer exists yet (see DECISIONS.md), but this is genuinely
    // rendered on screen, not a log line. `x`/`y` are character-cell
    // coordinates (8x16 cells from the top-left), not pixels. This is the
    // only place besides submit_chunk_mesh() that touches bgfx per frame -
    // engine/ui (and anything else) calls through here rather than
    // including bgfx itself, keeping ARCHITECTURE.md's "only
    // engine/rendering includes bgfx headers" rule intact.
    void clear_debug_text();
    void draw_debug_text(u16 x, u16 y, u8 color_attr, const std::string& text);

    // Real screenshot trigger (Phase 47, F2): wraps
    // `bgfx::requestScreenShot(BGFX_INVALID_HANDLE, ...)` against the
    // default backbuffer - bgfx's own default callback writes a real
    // `.tga` file asynchronously once the in-flight frame finishes (no
    // custom `bgfx::CallbackI` is installed - see init(), so this is
    // bgfx's own stock behavior, not something this project reimplements).
    // Under the headless `Noop` backend (this sandbox's own verification
    // runs) there is no real framebuffer content to capture - a real,
    // honest environment limitation, not a bug (see BUILD_STATUS.md).
    void request_screenshot(const std::string& file_path_without_extension);

    void resize(u32 width, u32 height);

    bool is_headless() const { return headless_; }

   private:
    bool initialized_ = false;
    bool headless_ = false;
    u32 width_ = 0;
    u32 height_ = 0;
    // Phase 28 - see submit_chunk_mesh's doc comment. Created
    // unconditionally in init() and destroyed in ~Renderer(); bgfx
    // uniforms don't require any particular shader to be compiled or
    // bound (setting one a currently-submitted program doesn't declare
    // is simply ignored, not an error), so this doesn't need to be
    // gated on whether LCU_BUILD_SHADER_TOOLS built real chunk shaders.
    bgfx::UniformHandle sky_light_scale_uniform_ = BGFX_INVALID_HANDLE;
    // Phase 53 - texture-atlas uniforms/sampler, same "created
    // unconditionally in init(), destroyed in ~Renderer()" reasoning as
    // sky_light_scale_uniform_ above (a bgfx uniform costs nothing to
    // declare even if no atlas texture is ever actually bound - see
    // submit_chunk_mesh's own doc comment). See fs_chunk.sc for what
    // each one actually does.
    bgfx::UniformHandle use_textures_uniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle tile_step_uniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle tile_inset_uniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle atlas_sampler_ = BGFX_INVALID_HANDLE;
    // Phase 57 - the font atlas's own separate sampler slot (slot 1,
    // `s_font` - `atlas_sampler_` above stays slot 0, `s_atlas`), same
    // "created unconditionally in init(), destroyed in ~Renderer()"
    // reasoning.
    bgfx::UniformHandle font_sampler_ = BGFX_INVALID_HANDLE;
    // Phase 44 - this frame's queued submit_ui_quad() calls, 4 vertices/
    // 6 indices per quad, uploaded and cleared together by
    // flush_ui_quads(). Real per-frame state (not a persistent GPU
    // resource - see submit_ui_quad's own doc comment), so a plain CPU
    // std::vector is the right tool, the same as every other per-frame
    // batch this codebase builds before handing it to a bgfx transient
    // buffer.
    std::vector<UiVertex2D> ui_vertices_;
    std::vector<u16> ui_indices_;
};

}  // namespace lcu::rendering
