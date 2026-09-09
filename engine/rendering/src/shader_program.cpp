#include "lcu/rendering/shader_program.h"

#include <cstdio>
#include <vector>

#include "lcu/core/log.h"

namespace lcu::rendering {

std::string active_shader_profile_dir() {
    switch (bgfx::getRendererType()) {
        case bgfx::RendererType::Vulkan:
            return "spirv";
        case bgfx::RendererType::OpenGL:
            return "glsl";
        case bgfx::RendererType::OpenGLES:
            return "essl";
        default:
            return "glsl";
    }
}

bgfx::ShaderHandle load_shader_from_file(const std::string& path) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        LCU_LOG_WARN("load_shader_from_file: could not open \"{}\"", path);
        return BGFX_INVALID_HANDLE;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(file);
        LCU_LOG_WARN("load_shader_from_file: empty or unreadable file \"{}\"", path);
        return BGFX_INVALID_HANDLE;
    }

    std::vector<u8> bytes(static_cast<usize>(size));
    const usize read = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (read != bytes.size()) {
        LCU_LOG_WARN("load_shader_from_file: short read on \"{}\"", path);
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::Memory* mem = bgfx::copy(bytes.data(), static_cast<u32>(bytes.size()));
    const bgfx::ShaderHandle handle = bgfx::createShader(mem);
    if (!bgfx::isValid(handle)) {
        LCU_LOG_WARN("load_shader_from_file: bgfx::createShader rejected \"{}\"", path);
    }
    return handle;
}

bgfx::ProgramHandle load_chunk_program(const std::string& shader_dir, const std::string& name) {
    const std::string base = shader_dir + "/" + active_shader_profile_dir() + "/";
    const bgfx::ShaderHandle vs = load_shader_from_file(base + "vs_" + name + ".sc.bin");
    const bgfx::ShaderHandle fs = load_shader_from_file(base + "fs_" + name + ".sc.bin");

    if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
        if (bgfx::isValid(vs)) {
            bgfx::destroy(vs);
        }
        if (bgfx::isValid(fs)) {
            bgfx::destroy(fs);
        }
        return BGFX_INVALID_HANDLE;
    }

    // `true`: the program takes ownership of both shaders and destroys
    // them along with itself.
    return bgfx::createProgram(vs, fs, true);
}

}  // namespace lcu::rendering
