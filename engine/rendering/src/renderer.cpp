#include "lcu/rendering/renderer.h"

#include <bgfx/bgfx.h>

#include "lcu/core/assert.h"
#include "lcu/core/log.h"

namespace lcu::rendering {

Renderer::~Renderer() {
    if (initialized_) {
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

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, static_cast<u16>(width_), static_cast<u16>(height_));
    bgfx::setDebug(BGFX_DEBUG_TEXT);

    initialized_ = true;
    return true;
}

void Renderer::begin_frame(u32 clear_rgba) {
    LCU_ASSERT(initialized_);

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clear_rgba, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, static_cast<u16>(width_), static_cast<u16>(height_));
    // touch(0) ensures view 0 executes its clear even when nothing submits
    // a draw call to it this frame (e.g. an empty chunk, or no shader
    // program compiled - see submit_chunk_mesh).
    bgfx::touch(0);
}

void Renderer::submit_chunk_mesh(const GpuChunkMesh& mesh, bgfx::ProgramHandle program,
                                  const math::Mat4& model, const math::Mat4& view, const math::Mat4& proj) {
    LCU_ASSERT(initialized_);
    if (!mesh.is_valid() || !bgfx::isValid(program)) {
        return;
    }

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setTransform(model.data());
    bgfx::setVertexBuffer(0, mesh.vertex_buffer);
    bgfx::setIndexBuffer(mesh.index_buffer);
    bgfx::setState(BGFX_STATE_DEFAULT);
    bgfx::submit(0, program);
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
    }
}

}  // namespace lcu::rendering
