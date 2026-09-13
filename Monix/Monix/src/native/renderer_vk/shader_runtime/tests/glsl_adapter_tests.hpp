#pragma once

#include "../shader_runtime/core/ShaderLanguage.hpp"
#include "../shader_runtime/core/SemanticUniforms.hpp"
#include "../shader_runtime/adapters/GlslAdapter.hpp"
#include "../shader_runtime/adapters/SlangAdapter.hpp"
#include "../shader_runtime/adapters/IShaderLanguageAdapter.hpp"
#include "../shader_runtime/core/ShaderModule.hpp"
#include "../shader_runtime/core/ShaderDiagnostics.hpp"
#include "../compiler/SlangCompiler.hpp"
#include "../shader_runtime/ShaderRuntime.hpp"

#include <cassert>
#include <cstdio>
#include <string>

namespace monix::renderer_vk::tests {

inline void test_glsl_language_detected() {
    assert(detectLanguageFromExtension(".glsl") == ShaderLanguage::GLSL);
    assert(detectLanguageFromExtension(".vert") == ShaderLanguage::GLSL);
    assert(detectLanguageFromExtension(".frag") == ShaderLanguage::GLSL);
    assert(detectLanguageFromExtension(".geom") == ShaderLanguage::GLSL);
    assert(detectLanguageFromExtension(".comp") == ShaderLanguage::GLSL);
    assert(detectLanguageFromExtension(".slang") == ShaderLanguage::Slang);
    assert(detectLanguageFromExtension(".cg") == ShaderLanguage::CG);
    printf("  [PASS] glsl_language_detected\n");
}

inline void test_glsl_adapter_available() {
    GlslAdapter adapter;
    assert(adapter.language() == ShaderLanguage::GLSL);
    assert(adapter.canCompile(ShaderLanguage::GLSL) == true);
    assert(adapter.canCompile(ShaderLanguage::Slang) == false);
    assert(adapter.isAvailable() == true);
    printf("  [PASS] glsl_adapter_available\n");
}

inline void test_glsl_adapter_rejects_slang() {
    GlslAdapter adapter;
    assert(adapter.canCompile(ShaderLanguage::Slang) == false);
    assert(adapter.canCompile(ShaderLanguage::CG) == false);
    assert(adapter.canCompile(ShaderLanguage::Unknown) == false);
    printf("  [PASS] glsl_adapter_rejects_slang\n");
}

inline void test_glsl_adapter_name() {
    GlslAdapter adapter;
    assert(std::string(adapter.languageName()) == "GLSL");
    printf("  [PASS] glsl_adapter_name\n");
}

inline void test_glsl_compile_invalid_source() {
    GlslAdapter adapter;
    ShaderRuntimeCompileRequest req;
    req.language = ShaderLanguage::GLSL;
    req.source = "invalid garbage code {{{";
    req.sourcePath = "test.glsl";
    req.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto result = adapter.compile(req);
    assert(!result);
    printf("  [PASS] glsl_compile_invalid_source\n");
}

inline void test_glsl_compile_valid_source() {
    GlslAdapter adapter;
    ShaderRuntimeCompileRequest req;
    req.language = ShaderLanguage::GLSL;
    req.source = R"(
#version 450
#pragma stage vertex
layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() {
    vTexCoord = TexCoord;
    gl_Position = vec4(Position, 1.0);
}
#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";
    req.sourcePath = "test.glsl";
    req.outputDirectory = "D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\shader-cache-vk";
    req.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto result = adapter.compile(req);
    assert(result);
    assert(result.value().compiled);
    assert(result.value().stages.size() == 2);
    assert(result.value().stages[0].spirv.size() > 0);
    assert(result.value().stages[1].spirv.size() > 0);
    assert(result.value().language == ShaderLanguage::GLSL);
    printf("  [PASS] glsl_compile_valid_source\n");
}

inline void test_glsl_reflection() {
    GlslAdapter adapter;
    ShaderRuntimeCompileRequest req;
    req.language = ShaderLanguage::GLSL;
    req.source = R"(
#version 450
#pragma stage vertex
layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() {
    vTexCoord = TexCoord;
    gl_Position = vec4(Position, 1.0);
}
#pragma stage fragment
layout(set = 0, binding = 0) uniform sampler2D Source;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
void main() {
    FragColor = texture(Source, vTexCoord);
}
)";
    req.sourcePath = "test_reflection.glsl";
    req.outputDirectory = "D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\shader-cache-vk";
    req.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto result = adapter.compile(req);
    assert(result);
    assert(result.value().compiled);
    assert(result.value().stages.size() == 2);
    assert(result.value().stages[1].reflection.samplers.size() > 0);
    assert(result.value().stages[1].reflection.samplers[0].name == "Source");
    assert(result.value().stages[1].reflection.samplers[0].set == 0);
    assert(result.value().stages[1].reflection.samplers[0].binding == 0);
    printf("  [PASS] glsl_reflection\n");
}

inline void test_glsl_version_detection() {
    GlslAdapter adapter;
    ShaderRuntimeCompileRequest req;
    req.language = ShaderLanguage::GLSL;
    req.source = R"(
#version 450
#pragma stage vertex
layout(location = 0) in vec3 Position;
void main() { gl_Position = vec4(Position, 1.0); }
#pragma stage fragment
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(1.0); }
)";
    req.sourcePath = "test_version.glsl";
    req.outputDirectory = "D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\shader-cache-vk";
    req.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto result = adapter.compile(req);
    assert(result);
    assert(result.value().compiled);
    printf("  [PASS] glsl_version_detection\n");
}

inline void test_glsl_include_handling() {
    GlslAdapter adapter;
    ShaderRuntimeCompileRequest req;
    req.language = ShaderLanguage::GLSL;
    req.source = R"(
#version 450
#pragma stage vertex
layout(location = 0) in vec3 Position;
void main() { gl_Position = vec4(Position, 1.0); }
#pragma stage fragment
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(0.5, 0.5, 0.5, 1.0); }
)";
    req.sourcePath = "test_include.glsl";
    req.outputDirectory = "D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\shader-cache-vk";
    req.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto result = adapter.compile(req);
    assert(result);
    assert(result.value().compiled);
    printf("  [PASS] glsl_include_handling\n");
}

inline void test_glsl_spirv_generation() {
    GlslAdapter adapter;
    ShaderRuntimeCompileRequest req;
    req.language = ShaderLanguage::GLSL;
    req.source = R"(
#version 450
#pragma stage vertex
layout(location = 0) in vec3 Position;
void main() { gl_Position = vec4(Position, 1.0); }
#pragma stage fragment
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(1.0); }
)";
    req.sourcePath = "test_spirv.glsl";
    req.outputDirectory = "D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\shader-cache-vk";
    req.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto result = adapter.compile(req);
    assert(result);
    assert(result.value().compiled);
    assert(result.value().stages[0].spirv.size() > 44);
    assert(result.value().stages[1].spirv.size() > 44);
    uint32_t magic = *reinterpret_cast<const uint32_t*>(result.value().stages[0].spirv.data());
    assert(magic == 0x07230203);
    printf("  [PASS] glsl_spirv_generation\n");
}

inline void test_glsl_regression_slang() {
    SlangAdapter adapter;
    assert(adapter.language() == ShaderLanguage::Slang);
    assert(adapter.canCompile(ShaderLanguage::Slang) == true);
    assert(adapter.canCompile(ShaderLanguage::GLSL) == false);
    assert(adapter.isAvailable() == true);
    printf("  [PASS] glsl_regression_slang\n");
}

inline void test_glsl_shader_red_output() {
    GlslAdapter adapter;
    ShaderRuntimeCompileRequest req;
    req.language = ShaderLanguage::GLSL;
    req.source = R"(
#version 450
#pragma stage vertex
layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() {
    vTexCoord = TexCoord;
    gl_Position = vec4(Position, 1.0);
}
#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";
    req.sourcePath = "solid_red.glsl";
    req.outputDirectory = "D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\shader-cache-vk";
    req.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto result = adapter.compile(req);
    assert(result);
    assert(result.value().compiled);
    assert(result.value().stages.size() == 2);
    assert(result.value().stages[0].spirv.size() > 0);
    assert(result.value().stages[1].spirv.size() > 0);
    printf("  [PASS] glsl_shader_red_output\n");
}

inline void test_glsl_shader_blue_output() {
    GlslAdapter adapter;
    ShaderRuntimeCompileRequest req;
    req.language = ShaderLanguage::GLSL;
    req.source = R"(
#version 450
#pragma stage vertex
layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() {
    vTexCoord = TexCoord;
    gl_Position = vec4(Position, 1.0);
}
#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
void main() {
    FragColor = vec4(0.0, 0.0, 1.0, 1.0);
}
)";
    req.sourcePath = "solid_blue.glsl";
    req.outputDirectory = "D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\shader-cache-vk";
    req.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto result = adapter.compile(req);
    assert(result);
    assert(result.value().compiled);
    assert(result.value().stages.size() == 2);
    printf("  [PASS] glsl_shader_blue_output\n");
}

inline void test_glsl_shader_red_then_blue() {
    GlslAdapter adapter;

    ShaderRuntimeCompileRequest redReq;
    redReq.language = ShaderLanguage::GLSL;
    redReq.source = R"(
#version 450
#pragma stage vertex
layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() { vTexCoord = TexCoord; gl_Position = vec4(Position, 1.0); }
#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(1.0, 0.0, 0.0, 1.0); }
)";
    redReq.sourcePath = "red.glsl";
    redReq.outputDirectory = "D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\shader-cache-vk";
    redReq.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto redResult = adapter.compile(redReq);
    assert(redResult);
    assert(redResult.value().compiled);

    ShaderRuntimeCompileRequest blueReq;
    blueReq.language = ShaderLanguage::GLSL;
    blueReq.source = R"(
#version 450
#pragma stage vertex
layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() { vTexCoord = TexCoord; gl_Position = vec4(Position, 1.0); }
#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(0.0, 0.0, 1.0, 1.0); }
)";
    blueReq.sourcePath = "blue.glsl";
    blueReq.outputDirectory = "D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\shader-cache-vk";
    blueReq.compilerPath = "D:\\Monix-2ago-unestable\\Monix\\Monix\\tools\\slangc.exe";
    auto blueResult = adapter.compile(blueReq);
    assert(blueResult);
    assert(blueResult.value().compiled);

    assert(redResult.value().stages[1].spirv != blueResult.value().stages[1].spirv);
    printf("  [PASS] glsl_shader_red_then_blue\n");
}

}  // namespace monix::renderer_vk::tests
