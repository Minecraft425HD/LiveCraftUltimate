#pragma once

#include <bgfx/bgfx.h>

#include <string>

namespace lcu::rendering {

// Maps the currently active bgfx renderer (bgfx::getRendererType(),
// valid only after Renderer::init()) to the profile subdirectory
// bgfx_compile_shaders() (see client/CMakeLists.txt) writes shaders to:
// "spirv" for Vulkan, "glsl" for desktop OpenGL, "essl" for OpenGL ES.
// Falls back to "glsl" for any renderer this build doesn't compile a
// shader profile for (including Noop - it never actually samples the
// shader bytecode, so the choice is arbitrary there).
std::string active_shader_profile_dir();

// Reads a compiled .bin shader (shaderc output) from `path` and creates
// a bgfx shader from it. Returns an invalid handle (bgfx::isValid ==
// false, logged) if the file can't be read or bgfx rejects it - shaders
// are optional content today (LCU_BUILD_SHADER_TOOLS is opt-in, see
// BUILDING.md), so a missing shader is not fatal to the caller by
// itself.
bgfx::ShaderHandle load_shader_from_file(const std::string& path);

// Loads "<shader_dir>/<active profile>/vs_<name>.sc.bin" and the
// matching "fs_<name>.sc.bin", and links them into a program. Returns
// an invalid handle if either half fails to load.
bgfx::ProgramHandle load_chunk_program(const std::string& shader_dir, const std::string& name);

}  // namespace lcu::rendering
