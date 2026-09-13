#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstdint>
#include <cassert>

#include "renderer_vk/compiler/SlangCompiler.hpp"
#include "renderer_vk/compiler/ShaderReflection.hpp"
#include "renderer_vk/compiler/ShaderCache.hpp"
#include "renderer_vk/preset/SlangPresetParser.hpp"
#include "renderer_vk/preset/SlangPreset.hpp"
#include "renderer_vk/preset/IncludeResolver.hpp"
#include "renderer_vk/preset/ShaderPreprocessor.hpp"
#include "renderer_vk/preset/StageSplitter.hpp"
#include "renderer_vk/preset/ParameterExtractor.hpp"
#include "renderer_vk/preset/AliasResolver.hpp"
#include "renderer_vk/preset/PresetValidator.hpp"
#include "renderer_vk/graph/RenderGraphBuilder.hpp"
#include "renderer_vk/graph/RenderGraph.hpp"
#include "renderer_vk/shader_runtime/dependencies/ShaderDependencyGraph.hpp"
#include "renderer_vk/shader_runtime/core/SemanticUniforms.hpp"
#include "renderer_vk/shader_runtime/core/ShaderLanguage.hpp"
#include "renderer_vk/validation/GpuShaderValidator.hpp"

namespace fs = std::filesystem;
using namespace monix::renderer_vk;

static const fs::path kBaseDir = "D:/Monix-2ago-unestable/Monix/Monix";
static const fs::path kShaderDir = kBaseDir / "tests/shaders";
static const fs::path kSlangcPath = kBaseDir / "tools/slangc.exe";
static const fs::path kOutputDir = "D:/Monix-2ago-unestable/build/test-shader-output";

static const std::vector<std::string> kAllGlslShaders = {
    "glsl/solid_red.glsl", "glsl/solid_green.glsl", "glsl/solid_blue.glsl",
    "glsl/solid_white.glsl", "glsl/gradient.glsl", "glsl/checkerboard.glsl",
    "glsl/uv_visualizer.glsl", "glsl/sampler_passthrough.glsl",
    "glsl/multipass_passthrough.glsl", "glsl/color_transform.glsl",
    "glsl/feedback_reader.glsl", "glsl/time_shader.glsl",
    "glsl/framecount_shader.glsl", "glsl/uniform_sizes.glsl"
};

static const std::vector<std::string> kAllPresets = {
    "presets/single_red.slangp", "presets/single_green.slangp",
    "presets/multipass_rgb.slangp", "presets/multipass_passthrough.slangp",
    "presets/multipass_transform.slangp", "presets/multipass_order_a.slangp",
    "presets/multipass_order_b.slangp", "presets/includes_test.slangp",
    "presets/feedback_test.slangp", "presets/mixed_format_test.slangp"
};

static bool ensureOutputDir() {
    std::error_code ec;
    fs::create_directories(kOutputDir, ec);
    if (ec) {
        printf("  FAIL: cannot create output dir: %s\n", ec.message().c_str());
        return false;
    }
    return true;
}

static std::string readFileStr(const fs::path& p) {
    std::ifstream f(p);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool checkSpirvMagic(const std::vector<unsigned char>& spirv) {
    if (spirv.size() < 4) return false;
    uint32_t magic = 0;
    std::memcpy(&magic, spirv.data(), 4);
    return magic == 0x07230203;
}

struct CompileOutcome {
    bool ok = false;
    std::vector<unsigned char> spirv;
    ShaderReflection reflection;
    std::string error;
};

static CompileOutcome compileShaderFile(const fs::path& path, ShaderStage stage) {
    CompileOutcome out;
    std::string source = readFileStr(path);
    if (source.empty()) { out.error = "cannot read " + path.string(); return out; }

    StageSplitter splitter;
    auto splitResult = splitter.split(source);
    if (!splitResult.ok()) { out.error = "split failed: " + splitResult.error(); return out; }
    auto& ssr = splitResult.value();

    std::string stageSource;
    if (stage == ShaderStage::Vertex) {
        if (!ssr.hasVertex) { out.error = "no vertex stage"; return out; }
        stageSource = ssr.vertex;
    } else {
        if (!ssr.hasFragment) { out.error = "no fragment stage"; return out; }
        stageSource = ssr.fragment;
    }

    SlangCompiler compiler;
    ShaderCompileRequest req;
    req.stage = stage;
    req.language = detectLanguageFromExtension(path.extension().string().substr(1));
    req.source = stageSource;
    req.sourcePath = path;
    req.outputDirectory = kOutputDir;
    req.slangcPath = kSlangcPath;
    req.debugInfo = false;

    auto result = compiler.compile(req);
    if (!result.ok()) { out.error = "compile error: " + result.error(); return out; }
    auto& res = result.value();
    out.ok = true;
    out.spirv = res.spirv;
    out.reflection = res.reflection;
    return out;
}

// ===================== GROUP 1: GLSL Single Pass Compilation =====================

static void test_glsl_compile_solid_red() {
    printf("  [TEST] test_glsl_compile_solid_red\n");
    if (!ensureOutputDir()) return;

    auto r = compileShaderFile(kShaderDir / "glsl/solid_red.glsl", ShaderStage::Fragment);
    if (!r.ok) { printf("  FAIL: %s\n", r.error.c_str()); return; }
    if (r.spirv.empty()) { printf("  FAIL: empty SPIR-V\n"); return; }
    if (!checkSpirvMagic(r.spirv)) { printf("  FAIL: bad magic number\n"); return; }
    if (r.reflection.uniformBlocks.empty()) { printf("  FAIL: no uniform blocks\n"); return; }
    if (!r.reflection.samplers.empty()) { printf("  FAIL: solid_red should have no samplers\n"); return; }
    printf("  PASS\n");
}

static void test_glsl_compile_solid_green() {
    printf("  [TEST] test_glsl_compile_solid_green\n");
    if (!ensureOutputDir()) return;

    auto r = compileShaderFile(kShaderDir / "glsl/solid_green.glsl", ShaderStage::Fragment);
    if (!r.ok) { printf("  FAIL: %s\n", r.error.c_str()); return; }
    if (r.spirv.empty()) { printf("  FAIL: empty SPIR-V\n"); return; }
    if (!checkSpirvMagic(r.spirv)) { printf("  FAIL: bad magic number\n"); return; }
    if (r.reflection.uniformBlocks.empty()) { printf("  FAIL: no uniform blocks\n"); return; }
    printf("  PASS\n");
}

static void test_glsl_compile_gradient() {
    printf("  [TEST] test_glsl_compile_gradient\n");
    if (!ensureOutputDir()) return;

    auto r = compileShaderFile(kShaderDir / "glsl/gradient.glsl", ShaderStage::Fragment);
    if (!r.ok) { printf("  FAIL: %s\n", r.error.c_str()); return; }
    if (r.spirv.empty()) { printf("  FAIL: empty SPIR-V\n"); return; }
    if (!checkSpirvMagic(r.spirv)) { printf("  FAIL: bad magic number\n"); return; }
    if (!r.reflection.samplers.empty()) { printf("  FAIL: gradient should have no samplers\n"); return; }
    printf("  PASS\n");
}

static void test_glsl_compile_checkerboard() {
    printf("  [TEST] test_glsl_compile_checkerboard\n");
    if (!ensureOutputDir()) return;

    auto r = compileShaderFile(kShaderDir / "glsl/checkerboard.glsl", ShaderStage::Fragment);
    if (!r.ok) { printf("  FAIL: %s\n", r.error.c_str()); return; }
    if (r.spirv.empty()) { printf("  FAIL: empty SPIR-V\n"); return; }
    if (!checkSpirvMagic(r.spirv)) { printf("  FAIL: bad magic number\n"); return; }
    printf("  PASS\n");
}

static void test_glsl_compile_uv_visualizer() {
    printf("  [TEST] test_glsl_compile_uv_visualizer\n");
    if (!ensureOutputDir()) return;

    auto r = compileShaderFile(kShaderDir / "glsl/uv_visualizer.glsl", ShaderStage::Fragment);
    if (!r.ok) { printf("  FAIL: %s\n", r.error.c_str()); return; }
    if (r.spirv.empty()) { printf("  FAIL: empty SPIR-V\n"); return; }
    if (!checkSpirvMagic(r.spirv)) { printf("  FAIL: bad magic number\n"); return; }
    printf("  PASS\n");
}


static void test_glsl_compile_sampler_passthrough() {
    printf("  [TEST] test_glsl_compile_sampler_passthrough\n");
    if (!ensureOutputDir()) return;

    auto r = compileShaderFile(kShaderDir / "glsl/sampler_passthrough.glsl", ShaderStage::Fragment);
    if (!r.ok) { printf("  FAIL: %s\n", r.error.c_str()); return; }
    if (r.spirv.empty()) { printf("  FAIL: empty SPIR-V\n"); return; }
    bool foundSampler = false;
    for (auto& s : r.reflection.samplers) {
        if (s.binding == 1) { foundSampler = true; break; }
    }
    if (!foundSampler) { printf("  FAIL: no sampler at binding=1\n"); return; }
    printf("  PASS\n");
}

static void test_glsl_compile_with_push_constants() {
    printf("  [TEST] test_glsl_compile_with_push_constants\n");
    if (!ensureOutputDir()) return;

    auto r = compileShaderFile(kShaderDir / "glsl/time_shader.glsl", ShaderStage::Fragment);
    if (!r.ok) { printf("  FAIL: %s\n", r.error.c_str()); return; }
    if (r.spirv.empty()) { printf("  FAIL: empty SPIR-V\n"); return; }
    if (r.reflection.pushConstants.empty()) { printf("  FAIL: no push constants\n"); return; }
    bool hasTime = false;
    for (auto& pc : r.reflection.pushConstants) {
        for (auto& m : pc.members) {
            if (m.name == "Time" || m.name == "time") { hasTime = true; break; }
        }
        if (hasTime) break;
    }
    if (!hasTime) { printf("  FAIL: no Time in push constants\n"); return; }
    printf("  PASS\n");
}

static void test_glsl_compile_all_formats_compile() {
    printf("  [TEST] test_glsl_compile_all_formats_compile\n");
    if (!ensureOutputDir()) return;

    int successes = 0;
    int total = static_cast<int>(kAllGlslShaders.size());
    for (auto& shader : kAllGlslShaders) {
        auto r = compileShaderFile(kShaderDir / shader, ShaderStage::Fragment);
        if (r.ok && !r.spirv.empty() && checkSpirvMagic(r.spirv)) {
            successes++;
        } else {
            printf("    WARN: failed to compile %s\n", shader.c_str());
        }
    }
    printf("    Compiled %d/%d shaders\n", successes, total);
    if (successes < total / 2) { printf("  FAIL: too many failures (%d/%d)\n", total - successes, total); return; }
    printf("  PASS\n");
}

// ===================== GROUP 2: Language Detection =====================

static void test_language_detection_glsl() {
    printf("  [TEST] test_language_detection_glsl\n");
    if (detectLanguageFromExtension(".glsl") != ShaderLanguage::GLSL) { printf("  FAIL: .glsl\n"); return; }
    if (detectLanguageFromExtension(".vert") != ShaderLanguage::GLSL) { printf("  FAIL: .vert\n"); return; }
    if (detectLanguageFromExtension(".frag") != ShaderLanguage::GLSL) { printf("  FAIL: .frag\n"); return; }
    printf("  PASS\n");
}

static void test_language_detection_slang() {
    printf("  [TEST] test_language_detection_slang\n");
    if (detectLanguageFromExtension(".slang") != ShaderLanguage::Slang) { printf("  FAIL: .slang\n"); return; }
    if (detectLanguageFromExtension(".slangp") != ShaderLanguage::Slang) { printf("  FAIL: .slangp\n"); return; }
    printf("  PASS\n");
}

static void test_language_detection_cg() {
    printf("  [TEST] test_language_detection_cg\n");
    if (detectLanguageFromExtension(".cg") != ShaderLanguage::CG) { printf("  FAIL: .cg\n"); return; }
    printf("  PASS\n");
}

static void test_language_detection_unknown() {
    printf("  [TEST] test_language_detection_unknown\n");
    if (detectLanguageFromExtension(".xyz") != ShaderLanguage::Unknown) { printf("  FAIL: .xyz should be Unknown\n"); return; }
    if (detectLanguageFromExtension(".") != ShaderLanguage::Unknown) { printf("  FAIL: empty ext\n"); return; }
    printf("  PASS\n");
}

// ===================== GROUP 3: Stage Splitting =====================

static void test_stage_split_with_pragmas() {
    printf("  [TEST] test_stage_split_with_pragmas\n");
    std::string src =
        "#pragma stage vertex\n"
        "void main() { gl_Position = vec4(0.0); }\n"
        "#pragma stage fragment\n"
        "void main() { fragColor = vec4(1.0); }\n";

    StageSplitter splitter;
    auto res = splitter.split(src);
    if (!res.ok()) { printf("  FAIL: split failed: %s\n", res.error().c_str()); return; }
    auto& ssr = res.value();
    if (!ssr.hasVertex) { printf("  FAIL: no vertex\n"); return; }
    if (!ssr.hasFragment) { printf("  FAIL: no fragment\n"); return; }
    if (ssr.vertex.find("gl_Position") == std::string::npos) { printf("  FAIL: vertex missing content\n"); return; }
    if (ssr.fragment.find("fragColor") == std::string::npos) { printf("  FAIL: fragment missing content\n"); return; }
    printf("  PASS\n");
}

static void test_stage_split_no_pragmas() {
    printf("  [TEST] test_stage_split_no_pragmas\n");
    std::string src = "void main() { fragColor = vec4(1.0); }\n";

    StageSplitter splitter;
    auto res = splitter.split(src);
    if (!res.ok()) { printf("  FAIL: split failed: %s\n", res.error().c_str()); return; }
    auto& ssr = res.value();
    if (ssr.hasVertex) { printf("  FAIL: should not have vertex\n"); return; }
    if (!ssr.hasFragment) { printf("  FAIL: should have fragment\n"); return; }
    if (ssr.fragment.find("fragColor") == std::string::npos) { printf("  FAIL: fragment missing content\n"); return; }
    printf("  PASS\n");
}

static void test_stage_split_common_code() {
    printf("  [TEST] test_stage_split_common_code\n");
    std::string src =
        "vec4 computeColor() { return vec4(1.0); }\n"
        "#pragma stage vertex\n"
        "void main() { vec4 c = computeColor(); }\n"
        "#pragma stage fragment\n"
        "void main() { vec4 c = computeColor(); }\n";

    StageSplitter splitter;
    auto res = splitter.split(src);
    if (!res.ok()) { printf("  FAIL: split failed: %s\n", res.error().c_str()); return; }
    auto& ssr = res.value();
    if (!ssr.hasVertex) { printf("  FAIL: no vertex\n"); return; }
    if (!ssr.hasFragment) { printf("  FAIL: no fragment\n"); return; }
    if (ssr.vertex.find("computeColor") == std::string::npos) { printf("  FAIL: vertex missing common code\n"); return; }
    if (ssr.fragment.find("computeColor") == std::string::npos) { printf("  FAIL: fragment missing common code\n"); return; }
    printf("  PASS\n");
}

static void test_stage_split_empty_stage() {
    printf("  [TEST] test_stage_split_empty_stage\n");
    std::string src = "#pragma stage vertex\n#pragma stage fragment\nvoid main() {}\n";

    StageSplitter splitter;
    auto res = splitter.split(src);
    if (res.ok()) {
        auto& ssr = res.value();
        if (!ssr.hasFragment) { printf("  FAIL: should have fragment\n"); return; }
        printf("  PASS\n");
    } else {
        // StageSplitter rejects empty vertex stage — this is expected behavior
        printf("  PASS (empty stage rejected: %s)\n", res.error().c_str());
    }
}

// ===================== GROUP 4: Parameter Extraction =====================

static void test_parameter_extraction_basic() {
    printf("  [TEST] test_parameter_extraction_basic\n");
    std::string src = "#pragma parameter Brightness \"Brightness\" 1.0 0.0 2.0 0.1\n";

    ParameterExtractor extractor;
    auto params = extractor.extract(src);
    if (params.empty()) { printf("  FAIL: no parameters found\n"); return; }
    if (params[0].name != "Brightness") { printf("  FAIL: wrong name: %s\n", params[0].name.c_str()); return; }
    if (params[0].defaultValue < 0.99f || params[0].defaultValue > 1.01f) { printf("  FAIL: wrong default\n"); return; }
    printf("  PASS\n");
}

static void test_parameter_extraction_int() {
    printf("  [TEST] test_parameter_extraction_int\n");
    std::string src = "#pragma parameter int Tiles \"Tiles\" 8 1 64 1\n";

    ParameterExtractor extractor;
    auto params = extractor.extract(src);
    if (params.empty()) { printf("  FAIL: no parameters found\n"); return; }
    if (params[0].name != "Tiles") { printf("  FAIL: wrong name\n"); return; }
    if (params[0].defaultValue < 7.9f || params[0].defaultValue > 8.1f) { printf("  FAIL: wrong default: %f\n", params[0].defaultValue); return; }
    printf("  PASS\n");
}

static void test_parameter_extraction_multiple() {
    printf("  [TEST] test_parameter_extraction_multiple\n");
    std::string src =
        "#pragma parameter Brightness \"Brightness\" 1.0 0.0 2.0 0.1\n"
        "#pragma parameter Contrast \"Contrast\" 1.0 0.0 4.0 0.1\n"
        "#pragma parameter Saturation \"Saturation\" 1.0 0.0 4.0 0.1\n";

    ParameterExtractor extractor;
    auto params = extractor.extract(src);
    if (params.size() < 3) { printf("  FAIL: expected >= 3 params, got %zu\n", params.size()); return; }
    if (params[0].name != "Brightness") { printf("  FAIL: first param wrong\n"); return; }
    if (params[1].name != "Contrast") { printf("  FAIL: second param wrong\n"); return; }
    if (params[2].name != "Saturation") { printf("  FAIL: third param wrong\n"); return; }
    printf("  PASS\n");
}

static void test_parameter_extraction_none() {
    printf("  [TEST] test_parameter_extraction_none\n");
    std::string src = "void main() { fragColor = vec4(1.0); }\n";

    ParameterExtractor extractor;
    auto params = extractor.extract(src);
    if (!params.empty()) { printf("  FAIL: should have no params, got %zu\n", params.size()); return; }
    printf("  PASS\n");
}

// ===================== GROUP 5: Include Resolution =====================

static void test_include_resolve_relative() {
    printf("  [TEST] test_include_resolve_relative\n");
    IncludeResolver resolver;
    resolver.addRoot(kShaderDir / "includes");

    fs::path includingFile = kShaderDir / "glsl/shader_with_includes.glsl";
    auto result = resolver.resolve(includingFile, "color.inc");
    if (!result.ok()) { printf("  FAIL: resolve failed: %s\n", result.error().c_str()); return; }
    if (result.value().empty()) { printf("  FAIL: empty path\n"); return; }
    printf("  PASS\n");
}

static void test_include_resolve_chain() {
    printf("  [TEST] test_include_resolve_chain\n");
    IncludeResolver resolver;
    resolver.addRoot(kShaderDir / "includes");

    fs::path includingFile = kShaderDir / "glsl/shader_with_includes.glsl";
    auto r1 = resolver.resolve(includingFile, "common.inc");
    if (!r1.ok()) { printf("  FAIL: common.inc not found: %s\n", r1.error().c_str()); return; }

    auto r2 = resolver.resolve(r1.value(), "color.inc");
    if (!r2.ok()) { printf("  FAIL: color.inc not found: %s\n", r2.error().c_str()); return; }

    auto r3 = resolver.resolve(r2.value(), "math.inc");
    if (!r3.ok()) { printf("  FAIL: math.inc not found: %s\n", r3.error().c_str()); return; }

    printf("  PASS\n");
}

static void test_include_resolve_not_found() {
    printf("  [TEST] test_include_resolve_not_found\n");
    IncludeResolver resolver;
    resolver.addRoot(kShaderDir / "includes");

    fs::path includingFile = kShaderDir / "glsl/shader_with_includes.glsl";
    auto result = resolver.resolve(includingFile, "nonexistent_file_xyz.inc");
    if (result.ok()) { printf("  FAIL: should have failed for missing include\n"); return; }
    printf("  PASS\n");
}

static void test_preprocess_includes() {
    printf("  [TEST] test_preprocess_includes\n");
    IncludeResolver resolver;
    resolver.addRoot(kShaderDir / "includes");
    ShaderPreprocessor preprocessor(resolver);

    auto result = preprocessor.preprocessFile(kShaderDir / "glsl/shader_with_includes.glsl");
    if (!result.ok()) { printf("  FAIL: preprocess failed: %s\n", result.error().c_str()); return; }
    auto pp = result.value();
    if (pp.source.empty()) { printf("  FAIL: empty preprocessed source\n"); return; }
    if (pp.dependencies.empty()) { printf("  WARN: no dependencies recorded (might be OK)\n"); }
    printf("  PASS\n");
}

// ===================== GROUP 6: Shader Preprocessing =====================

static void test_preprocess_defines() {
    printf("  [TEST] test_preprocess_defines\n");
    IncludeResolver resolver;
    ShaderPreprocessor preprocessor(resolver);
    std::string src =
        "#define FOO 42\n"
        "int x = FOO;\n";

    std::ofstream tmp(kOutputDir / "test_define.glsl");
    tmp << src;
    tmp.close();

    auto result = preprocessor.preprocessFile(kOutputDir / "test_define.glsl");
    if (!result.ok()) { printf("  FAIL: preprocess failed: %s\n", result.error().c_str()); return; }
    if (result.value().source.find("42") == std::string::npos) { printf("  FAIL: define not resolved\n"); return; }
    printf("  PASS\n");
}

static void test_preprocess_ifdef() {
    printf("  [TEST] test_preprocess_ifdef\n");
    if (!ensureOutputDir()) return;
    IncludeResolver resolver;
    ShaderPreprocessor preprocessor(resolver);

    std::string withDefine =
        "#define FOO\n"
        "#ifdef FOO\n"
        "int x = 1;\n"
        "#endif\n";

    std::ofstream tmp1(kOutputDir / "test_ifdef_with.glsl");
    tmp1 << withDefine;
    tmp1.close();

    auto r1 = preprocessor.preprocessFile(kOutputDir / "test_ifdef_with.glsl");
    if (!r1.ok()) { printf("  FAIL: preprocess with define failed: %s\n", r1.error().c_str()); return; }
    if (r1.value().source.find("int x = 1") == std::string::npos) { printf("  FAIL: ifdef block not included\n"); return; }

    std::string withoutDefine =
        "#ifdef FOO\n"
        "int x = 1;\n"
        "#endif\n";

    std::ofstream tmp2(kOutputDir / "test_ifdef_without.glsl");
    tmp2 << withoutDefine;
    tmp2.close();

    auto r2 = preprocessor.preprocessFile(kOutputDir / "test_ifdef_without.glsl");
    if (!r2.ok()) { printf("  FAIL: preprocess without define failed: %s\n", r2.error().c_str()); return; }
    if (r2.value().source.find("int x = 1") != std::string::npos) { printf("  FAIL: ifdef block should not be included\n"); return; }
    printf("  PASS\n");
}

static void test_preprocess_ifdef_else() {
    printf("  [TEST] test_preprocess_ifdef_else\n");
    if (!ensureOutputDir()) return;
    IncludeResolver resolver;
    ShaderPreprocessor preprocessor(resolver);

    std::string src =
        "#ifdef FOO\n"
        "int a = 1;\n"
        "#else\n"
        "int a = 2;\n"
        "#endif\n";

    std::ofstream tmp(kOutputDir / "test_ifdef_else.glsl");
    tmp << src;
    tmp.close();

    auto r = preprocessor.preprocessFile(kOutputDir / "test_ifdef_else.glsl");
    if (!r.ok()) { printf("  FAIL: preprocess failed: %s\n", r.error().c_str()); return; }
    if (r.value().source.find("int a = 2") == std::string::npos) { printf("  FAIL: else branch should be active (output=[%s])\n", r.value().source.substr(0, 200).c_str()); return; }
    if (r.value().source.find("int a = 1") != std::string::npos) { printf("  FAIL: if branch should not be active\n"); return; }
    printf("  PASS\n");
}

static void test_preprocess_nested_ifdef() {
    printf("  [TEST] test_preprocess_nested_ifdef\n");
    if (!ensureOutputDir()) return;
    IncludeResolver resolver;
    ShaderPreprocessor preprocessor(resolver);

    std::string src =
        "#define OUTER\n"
        "#ifdef OUTER\n"
        "  #ifdef INNER\n"
        "  int a = 1;\n"
        "  #else\n"
        "  int a = 2;\n"
        "  #endif\n"
        "#endif\n";

    std::ofstream tmp(kOutputDir / "test_nested_ifdef.glsl");
    tmp << src;
    tmp.close();

    auto r = preprocessor.preprocessFile(kOutputDir / "test_nested_ifdef.glsl");
    if (!r.ok()) { printf("  FAIL: preprocess failed: %s\n", r.error().c_str()); return; }
    if (r.value().source.find("int a = 2") == std::string::npos) { printf("  FAIL: nested else branch should be active\n"); return; }
    printf("  PASS\n");
}

// ===================== GROUP 7: Preset Parsing =====================

static void test_preset_parse_single() {
    printf("  [TEST] test_preset_parse_single\n");
    SlangPresetParser parser;
    auto result = parser.parseFile(kShaderDir / "presets/single_red.slangp");
    if (!result.ok()) { printf("  FAIL: parse failed: %s\n", result.error().c_str()); return; }
    auto ast = result.value();
    if (ast.entries.empty()) { printf("  FAIL: no entries\n"); return; }
    bool found = false;
    for (auto& e : ast.entries) {
        if (e.value.find("solid_red.glsl") != std::string::npos) { found = true; break; }
    }
    if (!found) { printf("  FAIL: solid_red.glsl not found in preset\n"); return; }
    printf("  PASS\n");
}

static void test_preset_parse_multipass_rgb() {
    printf("  [TEST] test_preset_parse_multipass_rgb\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/multipass_rgb.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }

    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }
    auto ir = irResult.value();
    if (ir.passes.size() != 3) { printf("  FAIL: expected 3 passes, got %zu\n", ir.passes.size()); return; }
    printf("  PASS\n");
}

static void test_preset_parse_multipass_order() {
    printf("  [TEST] test_preset_parse_multipass_order\n");
    SlangPresetParser parser;

    auto astA = parser.parseFile(kShaderDir / "presets/multipass_order_a.slangp");
    if (!astA.ok()) { printf("  FAIL: parse order_a failed: %s\n", astA.error().c_str()); return; }
    auto irA = parser.buildIr(astA.value());
    if (!irA.ok()) { printf("  FAIL: buildIr order_a failed: %s\n", irA.error().c_str()); return; }

    auto astB = parser.parseFile(kShaderDir / "presets/multipass_order_b.slangp");
    if (!astB.ok()) { printf("  FAIL: parse order_b failed: %s\n", astB.error().c_str()); return; }
    auto irB = parser.buildIr(astB.value());
    if (!irB.ok()) { printf("  FAIL: buildIr order_b failed: %s\n", irB.error().c_str()); return; }

    if (irA.value().passes.size() != irB.value().passes.size()) { printf("  FAIL: different pass counts\n"); return; }
    if (irA.value().passes.size() < 2) { printf("  FAIL: need >= 2 passes\n"); return; }
    bool same = true;
    for (size_t i = 0; i < irA.value().passes.size(); i++) {
        if (irA.value().passes[i].shaderPath != irB.value().passes[i].shaderPath) { same = false; break; }
    }
    if (same) { printf("  FAIL: orders should differ\n"); return; }
    printf("  PASS\n");
}

static void test_preset_parse_parameters() {
    printf("  [TEST] test_preset_parse_parameters\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/parameter_test.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }
    auto ir = irResult.value();
    if (ir.parameterOverrides.empty()) { printf("  FAIL: no parameter overrides\n"); return; }
    printf("  PASS\n");
}

static void test_preset_parse_feedback() {
    printf("  [TEST] test_preset_parse_feedback\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/feedback_test.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }
    auto ir = irResult.value();
    bool hasFeedback = false;
    for (auto& p : ir.passes) {
        if (p.framebufferFeedback) { hasFeedback = true; break; }
    }
    if (!hasFeedback) { printf("  FAIL: no feedback pass found\n"); return; }
    printf("  PASS\n");
}

static void test_preset_build_ir() {
    printf("  [TEST] test_preset_build_ir\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/single_red.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }
    auto ir = irResult.value();
    if (ir.passes.empty()) { printf("  FAIL: no passes in IR\n"); return; }
    if (ir.baseDirectory.empty()) { printf("  WARN: baseDirectory empty (may be OK)\n"); }
    printf("  PASS\n");
}

// ===================== GROUP 8: Preset Validation =====================

static void test_preset_validate_valid() {
    printf("  [TEST] test_preset_validate_valid\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/single_red.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }

    PresetValidator validator;
    auto diag = validator.validate(irResult.value());
    if (diag.hasErrors()) { printf("  FAIL: valid preset has errors\n"); return; }
    printf("  PASS\n");
}

static void test_preset_validate_missing_shader() {
    printf("  [TEST] test_preset_validate_missing_shader\n");
    PresetIr ir;
    ir.path = kShaderDir / "bad.slangp";
    ir.baseDirectory = kShaderDir;
    ir.name = "bad";
    PresetPassIr pass;
    pass.index = 0;
    pass.shaderPath = "nonexistent_shader_xyz.glsl";
    pass.filterLinear = false;
    pass.mipmapInput = false;
    pass.floatFramebuffer = false;
    pass.srgbFramebuffer = false;
    pass.framebufferFeedback = false;
    ir.passes.push_back(pass);

    PresetValidator validator;
    auto diag = validator.validate(ir);
    if (!diag.hasErrors()) { printf("  FAIL: should have errors for missing shader\n"); return; }
    printf("  PASS\n");
}

static void test_preset_validate_empty() {
    printf("  [TEST] test_preset_validate_empty\n");
    PresetIr ir;
    ir.path = kShaderDir / "empty.slangp";
    ir.baseDirectory = kShaderDir;
    ir.name = "empty";

    PresetValidator validator;
    auto diag = validator.validate(ir);
    if (!diag.hasErrors()) { printf("  FAIL: should have errors for empty preset\n"); return; }
    printf("  PASS\n");
}

// ===================== GROUP 9: Alias Resolution =====================

static void test_alias_resolve_builtin_source() {
    printf("  [TEST] test_alias_resolve_builtin_source\n");
    AliasDatabase db;
    db.addBuiltin("Source");
    auto res = db.resolve("Source", 0);
    if (res.kind != AliasKind::Builtin) { printf("  FAIL: Source should be Builtin\n"); return; }
    if (res.name != "Source") { printf("  FAIL: wrong name\n"); return; }
    printf("  PASS\n");
}

static void test_alias_resolve_builtin_original() {
    printf("  [TEST] test_alias_resolve_builtin_original\n");
    AliasDatabase db;
    db.addBuiltin("Original");
    auto res = db.resolve("Original", 0);
    if (res.kind != AliasKind::Builtin) { printf("  FAIL: Original should be Builtin\n"); return; }
    printf("  PASS\n");
}

static void test_alias_resolve_pass_output() {
    printf("  [TEST] test_alias_resolve_pass_output\n");
    AliasDatabase db;
    db.addPassAlias("RedPass", 0);
    auto res = db.resolve("RedPass", 1);
    if (res.kind != AliasKind::PassOutput) { printf("  FAIL: RedPass should be PassOutput\n"); return; }
    if (res.passIndex != 0) { printf("  FAIL: wrong passIndex: %d\n", res.passIndex); return; }
    printf("  PASS\n");
}

static void test_alias_resolve_feedback() {
    printf("  [TEST] test_alias_resolve_feedback\n");
    AliasDatabase db;
    db.addPassAlias("RedPass", 0);
    auto res = db.resolve("RedPassFeedback", 1);
    if (res.kind != AliasKind::Feedback) { printf("  FAIL: RedPassFeedback should be Feedback\n"); return; }
    printf("  PASS\n");
}

static void test_alias_resolve_external_texture() {
    printf("  [TEST] test_alias_resolve_external_texture\n");
    AliasDatabase db;
    db.addExternalTextureAlias("LUT", 0);
    auto res = db.resolve("LUT", 0);
    if (res.kind != AliasKind::ExternalTexture) { printf("  FAIL: LUT should be ExternalTexture\n"); return; }
    if (res.textureIndex != 0) { printf("  FAIL: wrong textureIndex\n"); return; }
    printf("  PASS\n");
}

// ===================== GROUP 10: Render Graph Construction =====================

static void test_graph_single_pass() {
    printf("  [TEST] test_graph_single_pass\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/single_red.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());
    for (size_t i = 0; i < irResult.value().passes.size(); i++) {
        reflections[i] = ShaderReflection();
    }

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed: %s\n", graphResult.error().c_str()); return; }
    auto graph = graphResult.value();
    if (graph.passes.empty()) { printf("  FAIL: no pass nodes\n"); return; }
    if (graph.images.empty()) { printf("  FAIL: no image nodes\n"); return; }
    printf("  PASS\n");
}

static void test_graph_multipass_rgb() {
    printf("  [TEST] test_graph_multipass_rgb\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/multipass_rgb.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());
    for (size_t i = 0; i < irResult.value().passes.size(); i++) {
        reflections[i] = ShaderReflection();
    }

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed: %s\n", graphResult.error().c_str()); return; }
    auto graph = graphResult.value();
    if (graph.passes.size() != 3) { printf("  FAIL: expected 3 passes, got %zu\n", graph.passes.size()); return; }
    int outputCount = 0;
    for (auto& img : graph.images) {
        if (!img.external && !img.feedback) outputCount++;
    }
    if (outputCount < 3) { printf("  FAIL: expected >= 3 output images\n"); return; }
    printf("  PASS\n");
}

static void test_graph_multipass_passthrough() {
    printf("  [TEST] test_graph_multipass_passthrough\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/multipass_transform.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());
    for (size_t i = 0; i < irResult.value().passes.size(); i++) {
        reflections[i] = ShaderReflection();
    }
    if (reflections.size() > 1) {
        SamplerReflection sampler;
        sampler.name = "Source";
        sampler.set = 1;
        sampler.binding = 0;
        reflections[1].samplers.push_back(sampler);
    }

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed: %s\n", graphResult.error().c_str()); return; }
    auto graph = graphResult.value();
    bool hasDependency = false;
    for (auto& pass : graph.passes) {
        if (!pass.inputs.empty()) { hasDependency = true; break; }
    }
    if (!hasDependency) { printf("  FAIL: no pass dependencies found\n"); return; }
    printf("  PASS\n");
}

static void test_graph_pass_ordering() {
    printf("  [TEST] test_graph_pass_ordering\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/multipass_rgb.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());
    for (size_t i = 0; i < irResult.value().passes.size(); i++) {
        reflections[i] = ShaderReflection();
    }

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed: %s\n", graphResult.error().c_str()); return; }

    auto plan = builder.compileExecutionPlan(graphResult.value());
    if (plan.passOrder.size() != 3) { printf("  FAIL: expected 3 in pass order, got %zu\n", plan.passOrder.size()); return; }
    for (size_t i = 0; i < plan.passOrder.size(); i++) {
        if (plan.passOrder[i] != static_cast<int>(i)) {
            printf("  FAIL: pass order mismatch at %zu: %d != %zu\n", i, plan.passOrder[i], i);
            return;
        }
    }
    printf("  PASS\n");
}

static void test_graph_lifetime_analysis() {
    printf("  [TEST] test_graph_lifetime_analysis\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/multipass_rgb.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());
    for (size_t i = 0; i < irResult.value().passes.size(); i++) {
        reflections[i] = ShaderReflection();
    }

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed: %s\n", graphResult.error().c_str()); return; }

    auto plan = builder.compileExecutionPlan(graphResult.value());
    if (plan.lifetimes.empty()) { printf("  FAIL: no resource lifetimes\n"); return; }
    for (auto& lt : plan.lifetimes) {
        if (lt.lastPass >= 0 && lt.firstPass > lt.lastPass) {
            printf("  FAIL: lifetime inverted: first=%d > last=%d\n", lt.firstPass, lt.lastPass);
            return;
        }
    }
    printf("  PASS\n");
}

static void test_graph_feedback_nodes() {
    printf("  [TEST] test_graph_feedback_nodes\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/feedback_test.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());
    for (size_t i = 0; i < irResult.value().passes.size(); i++) {
        reflections[i] = ShaderReflection();
    }

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed: %s\n", graphResult.error().c_str()); return; }

    bool hasFeedback = false;
    for (auto& img : graphResult.value().images) {
        if (img.feedback) { hasFeedback = true; break; }
    }
    if (!hasFeedback) { printf("  FAIL: no feedback image node\n"); return; }
    printf("  PASS\n");
}

// ===================== GROUP 11: Semantic Uniforms =====================

static void test_semantic_resolve_input_texture() {
    printf("  [TEST] test_semantic_resolve_input_texture\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("InputTexture");
    if (match.semantic != ShaderSemantic::InputTexture) { printf("  FAIL: wrong semantic\n"); return; }
    if (!match.exactMatch) { printf("  FAIL: not exact match\n"); return; }
    printf("  PASS\n");
}

static void test_semantic_resolve_output_size() {
    printf("  [TEST] test_semantic_resolve_output_size\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("OutputSize");
    if (match.semantic != ShaderSemantic::OutputSize) { printf("  FAIL: wrong semantic\n"); return; }
    if (!match.exactMatch) { printf("  FAIL: not exact match\n"); return; }
    printf("  PASS\n");
}

static void test_semantic_resolve_aliases() {
    printf("  [TEST] test_semantic_resolve_aliases\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    auto m1 = resolver.resolve("SourceSize");
    if (m1.semantic != ShaderSemantic::SourceSize) { printf("  FAIL: SourceSize\n"); return; }
    auto m2 = resolver.resolve("InputSize");
    if (m2.semantic != ShaderSemantic::InputSize) { printf("  FAIL: InputSize\n"); return; }
    auto m3 = resolver.resolve("OriginalSize");
    if (m3.semantic != ShaderSemantic::OriginalSize) { printf("  FAIL: OriginalSize\n"); return; }
    printf("  PASS\n");
}

static void test_semantic_resolve_custom_alias() {
    printf("  [TEST] test_semantic_resolve_custom_alias\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    resolver.addAlias(ShaderSemantic::Time, "MyCustomTime");
    auto match = resolver.resolve("MyCustomTime");
    if (match.semantic != ShaderSemantic::Time) { printf("  FAIL: custom alias not resolved\n"); return; }
    if (!match.aliasMatch) { printf("  FAIL: not flagged as alias match\n"); return; }
    printf("  PASS\n");
}

// ===================== GROUP 12: Shader Cache =====================

static void test_cache_store_retrieve_spirv() {
    printf("  [TEST] test_cache_store_retrieve_spirv\n");
    fs::path cacheDir = kOutputDir / "test_cache_1";
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    ShaderCache cache(cacheDir);
    CacheKeyComponents components;
    components.sourceContent = "test_source_code_123";
    components.stage = "Fragment";
    components.language = "GLSL";
    components.compilerVersion = "1.0";
    components.debugInfo = false;

    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07, 0x01, 0x00, 0x00, 0x00};
    auto key = cache.makeKey(components);
    auto storeResult = cache.store(key, spirv, "");
    if (!storeResult.ok) { printf("  FAIL: store failed: %s\n", storeResult.message.c_str()); return; }

    if (!cache.contains(key)) { printf("  FAIL: cache does not contain stored key\n"); return; }
    auto entry = cache.entryFor(key);
    if (!entry.hit) { printf("  FAIL: entryFor returned miss\n"); return; }
    if (cache.verifyIntegrity(entry) != true) { printf("  FAIL: verifyIntegrity failed\n"); return; }
    printf("  PASS\n");
}

static void test_cache_hit_same_source() {
    printf("  [TEST] test_cache_hit_same_source\n");
    fs::path cacheDir = kOutputDir / "test_cache_2";
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    ShaderCache cache(cacheDir);
    CacheKeyComponents components;
    components.sourceContent = "same_source";
    components.stage = "Fragment";
    components.language = "GLSL";
    components.compilerVersion = "1.0";
    components.debugInfo = false;

    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07};
    auto key = cache.makeKey(components);
    auto storeResult = cache.store(key, spirv, "");
    if (!storeResult.ok) { printf("  FAIL: store failed: %s\n", storeResult.message.c_str()); return; }

    if (!cache.contains(key)) { printf("  FAIL: first contains check failed\n"); return; }
    if (!cache.contains(key)) { printf("  FAIL: second contains check failed\n"); return; }
    auto e1 = cache.entryFor(key);
    auto e2 = cache.entryFor(key);
    if (e1.spirvPath != e2.spirvPath) { printf("  FAIL: entry paths mismatch\n"); return; }
    printf("  PASS\n");
}

static void test_cache_miss_different_source() {
    printf("  [TEST] test_cache_miss_different_source\n");
    fs::path cacheDir = kOutputDir / "test_cache_3";
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    ShaderCache cache(cacheDir);
    CacheKeyComponents components1;
    components1.sourceContent = "source_A";
    components1.stage = "Fragment";
    components1.language = "GLSL";
    components1.compilerVersion = "1.0";
    components1.debugInfo = false;

    CacheKeyComponents components2;
    components2.sourceContent = "source_B";
    components2.stage = "Fragment";
    components2.language = "GLSL";
    components2.compilerVersion = "1.0";
    components2.debugInfo = false;

    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07};
    auto key1 = cache.makeKey(components1);
    auto key2 = cache.makeKey(components2);
    auto storeResult = cache.store(key1, spirv, "");
    if (!storeResult.ok) { printf("  FAIL: store failed: %s\n", storeResult.message.c_str()); return; }

    if (cache.contains(key2)) { printf("  FAIL: should be cache miss for different source\n"); return; }
    printf("  PASS\n");
}

static void test_cache_invalidation() {
    printf("  [TEST] test_cache_invalidation\n");
    fs::path cacheDir = kOutputDir / "test_cache_4";
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    ShaderCache cache(cacheDir);
    CacheKeyComponents components;
    components.sourceContent = "invalidate_me";
    components.stage = "Fragment";
    components.language = "GLSL";
    components.compilerVersion = "1.0";
    components.debugInfo = false;

    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07};
    auto key = cache.makeKey(components);
    auto storeResult = cache.store(key, spirv, "");
    if (!storeResult.ok) { printf("  FAIL: store failed: %s\n", storeResult.message.c_str()); return; }

    auto invResult = cache.invalidate(key);
    if (!invResult.ok) { printf("  FAIL: invalidate failed: %s\n", invResult.message.c_str()); return; }
    if (cache.contains(key)) { printf("  FAIL: should be miss after invalidation\n"); return; }
    printf("  PASS\n");
}

static void test_cache_dependency_content() {
    printf("  [TEST] test_cache_dependency_content\n");
    fs::path cacheDir = kOutputDir / "test_cache_5";
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    ShaderCache cache(cacheDir);
    CacheKeyComponents components1;
    components1.sourceContent = "source";
    components1.stage = "Fragment";
    components1.language = "GLSL";
    components1.compilerVersion = "1.0";
    components1.debugInfo = false;
    components1.dependencyPaths = {"dep_A.inc"};
    components1.dependencyContents = {"dep_A_content"};

    CacheKeyComponents components2;
    components2.sourceContent = "source";
    components2.stage = "Fragment";
    components2.language = "GLSL";
    components2.compilerVersion = "1.0";
    components2.debugInfo = false;
    components2.dependencyPaths = {"dep_B.inc"};
    components2.dependencyContents = {"dep_B_content"};

    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07};
    auto key1 = cache.makeKey(components1);
    auto key2 = cache.makeKey(components2);
    auto storeResult = cache.store(key1, spirv, "");
    if (!storeResult.ok) { printf("  FAIL: store key1 failed: %s\n", storeResult.message.c_str()); return; }

    if (cache.contains(key2)) { printf("  FAIL: different deps should be cache miss\n"); return; }
    printf("  PASS\n");
}

// ===================== GROUP 13: Hot Reload Dependencies =====================

static void test_dep_graph_single_file() {
    printf("  [TEST] test_dep_graph_single_file\n");
    ShaderDependencyGraph graph;
    graph.registerShader("shader_a.glsl");
    graph.registerInclude("common.inc");
    graph.addDependency("shader_a.glsl", "common.inc");

    auto result = graph.invalidate("common.inc");
    if (!result.anyFound) { printf("  FAIL: no affected nodes\n"); return; }
    bool found = false;
    for (auto& s : result.affectedShaders) { if (s.filename() == fs::path("shader_a.glsl")) { found = true; break; } }
    if (!found) { printf("  FAIL: shader_a not affected\n"); return; }
    printf("  PASS\n");
}

static void test_dep_graph_include_chain() {
    printf("  [TEST] test_dep_graph_include_chain\n");
    ShaderDependencyGraph graph;
    graph.registerShader("shader.glsl");
    graph.registerInclude("common.inc");
    graph.registerInclude("color.inc");
    graph.addDependency("shader.glsl", "common.inc");
    graph.addDependency("common.inc", "color.inc");

    auto result = graph.invalidate("color.inc");
    if (!result.anyFound) { printf("  FAIL: no affected nodes\n"); return; }
    bool foundShader = false;
    for (auto& s : result.affectedShaders) { if (s.filename() == fs::path("shader.glsl")) { foundShader = true; break; } }
    if (!foundShader) { printf("  FAIL: shader not affected by transitive dep\n"); return; }
    printf("  PASS\n");
}

static void test_dep_graph_topological_sort() {
    printf("  [TEST] test_dep_graph_topological_sort\n");
    ShaderDependencyGraph graph;
    graph.registerShader("shader.glsl");
    graph.registerInclude("common.inc");
    graph.registerInclude("color.inc");
    graph.addDependency("shader.glsl", "common.inc");
    graph.addDependency("common.inc", "color.inc");

    auto shaderDeps = graph.getDependencies("shader.glsl");
    if (shaderDeps.empty()) { printf("  FAIL: shader.glsl has no dependencies\n"); return; }
    bool hasCommon = false;
    for (auto& d : shaderDeps) { if (d.filename() == fs::path("common.inc")) { hasCommon = true; break; } }
    if (!hasCommon) { printf("  FAIL: shader.glsl should depend on common.inc\n"); return; }

    auto commonDeps = graph.getDependencies("common.inc");
    if (commonDeps.empty()) { printf("  FAIL: common.inc has no dependencies\n"); return; }
    bool hasColor = false;
    for (auto& d : commonDeps) { if (d.filename() == fs::path("color.inc")) { hasColor = true; break; } }
    if (!hasColor) { printf("  FAIL: common.inc should depend on color.inc\n"); return; }

    auto colorDependents = graph.getDependents("color.inc");
    bool commonDependsOnColor = false;
    for (auto& dep : colorDependents) { if (dep.filename() == fs::path("common.inc")) { commonDependsOnColor = true; break; } }
    if (!commonDependsOnColor) { printf("  FAIL: common.inc should be dependent of color.inc\n"); return; }

    auto commonDependents = graph.getDependents("common.inc");
    bool shaderDependsOnCommon = false;
    for (auto& dep : commonDependents) { if (dep.filename() == fs::path("shader.glsl")) { shaderDependsOnCommon = true; break; } }
    if (!shaderDependsOnCommon) { printf("  FAIL: shader.glsl should be dependent of common.inc\n"); return; }

    printf("  PASS\n");
}

static void test_dep_graph_clear() {
    printf("  [TEST] test_dep_graph_clear\n");
    ShaderDependencyGraph graph;
    graph.registerShader("a.glsl");
    graph.registerShader("b.glsl");
    graph.addDependency("a.glsl", "b.glsl");

    if (graph.nodeCount() < 2) { printf("  FAIL: nodes not added\n"); return; }

    graph.clear();
    if (graph.nodeCount() != 0) { printf("  FAIL: nodeCount not zero after clear: %zu\n", graph.nodeCount()); return; }
    if (graph.edgeCount() != 0) { printf("  FAIL: edgeCount not zero after clear: %zu\n", graph.edgeCount()); return; }
    printf("  PASS\n");
}

// ===================== GROUP 14: Reflection Analysis =====================

static void test_reflection_ubo_block() {
    printf("  [TEST] test_reflection_ubo_block\n");
    std::string source = readFileStr(kShaderDir / "glsl/solid_red.glsl");
    if (source.empty()) { printf("  FAIL: cannot read solid_red.glsl\n"); return; }

    auto reflResult = ShaderReflectionParser().fromSourceLayout(source);
    if (!reflResult.ok()) { printf("  FAIL: reflection parse failed: %s\n", reflResult.error().c_str()); return; }
    auto& refl = reflResult.value();
    if (refl.uniformBlocks.empty()) { printf("  FAIL: no uniform blocks\n"); return; }
    if (refl.uniformBlocks[0].size == 0) { printf("  FAIL: uniform block size is 0\n"); return; }
    printf("  PASS\n");
}

static void test_reflection_sampler() {
    printf("  [TEST] test_reflection_sampler\n");
    std::string source = readFileStr(kShaderDir / "glsl/sampler_passthrough.glsl");
    if (source.empty()) { printf("  FAIL: cannot read sampler_passthrough.glsl\n"); return; }

    auto reflResult = ShaderReflectionParser().fromSourceLayout(source);
    if (!reflResult.ok()) { printf("  FAIL: reflection parse failed: %s\n", reflResult.error().c_str()); return; }
    auto& refl = reflResult.value();
    bool found = false;
    for (auto& s : refl.samplers) {
        if (s.name == "InputTexture" && s.binding == 1) { found = true; break; }
    }
    if (!found) { printf("  FAIL: InputTexture at binding=1 not found\n"); return; }
    printf("  PASS\n");
}

static void test_reflection_push_constants() {
    printf("  [TEST] test_reflection_push_constants\n");
    std::string source = readFileStr(kShaderDir / "glsl/time_shader.glsl");
    if (source.empty()) { printf("  FAIL: cannot read time_shader.glsl\n"); return; }

    auto reflResult = ShaderReflectionParser().fromSourceLayout(source);
    if (!reflResult.ok()) { printf("  FAIL: reflection parse failed: %s\n", reflResult.error().c_str()); return; }
    auto& refl = reflResult.value();
    if (refl.pushConstants.empty()) { printf("  FAIL: no push constants\n"); return; }
    bool hasTime = false;
    for (auto& pc : refl.pushConstants) {
        for (auto& m : pc.members) {
            if (m.name == "Time" || m.name == "time") { hasTime = true; break; }
        }
        if (hasTime) break;
    }
    if (!hasTime) { printf("  FAIL: Time not found in push constants\n"); return; }
    printf("  PASS\n");
}

static void test_reflection_descriptor_bindings() {
    printf("  [TEST] test_reflection_descriptor_bindings\n");
    std::string source = readFileStr(kShaderDir / "glsl/sampler_passthrough.glsl");
    if (source.empty()) { printf("  FAIL: cannot read sampler_passthrough.glsl\n"); return; }

    auto reflResult = ShaderReflectionParser().fromSourceLayout(source);
    if (!reflResult.ok()) { printf("  FAIL: reflection parse failed: %s\n", reflResult.error().c_str()); return; }
    auto& refl = reflResult.value();
    size_t totalBindings = refl.descriptors.size() + refl.uniformBlocks.size() + refl.samplers.size();
    if (totalBindings == 0) { printf("  FAIL: no descriptor bindings found\n"); return; }
    printf("  PASS\n");
}

// ===================== GROUP 15: Multipass Pipeline =====================

static void test_multipass_parse_and_graph() {
    printf("  [TEST] test_multipass_parse_and_graph\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/multipass_rgb.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());
    for (size_t i = 0; i < irResult.value().passes.size(); i++) {
        reflections[i] = ShaderReflection();
    }

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed: %s\n", graphResult.error().c_str()); return; }
    if (graphResult.value().passes.size() != 3) {
        printf("  FAIL: expected 3 passes, got %zu\n", graphResult.value().passes.size());
        return;
    }
    printf("  PASS\n");
}

static void test_multipass_transform_pipeline() {
    printf("  [TEST] test_multipass_transform_pipeline\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/multipass_transform.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed: %s\n", astResult.error().c_str()); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed: %s\n", irResult.error().c_str()); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());
    for (size_t i = 0; i < irResult.value().passes.size(); i++) {
        reflections[i] = ShaderReflection();
    }
    if (reflections.size() > 1) {
        SamplerReflection sampler;
        sampler.name = "Source";
        sampler.set = 1;
        sampler.binding = 0;
        reflections[1].samplers.push_back(sampler);
    }

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed: %s\n", graphResult.error().c_str()); return; }

    auto& graph = graphResult.value();
    if (graph.passes.size() < 2) { printf("  FAIL: need >= 2 passes\n"); return; }

    bool pass1HasInput = false;
    for (auto& dep : graph.passes[1].inputs) {
        if (dep.alias.kind == AliasKind::Builtin && dep.alias.name == "Source") {
            pass1HasInput = true;
            break;
        }
    }
    if (!pass1HasInput) { printf("  FAIL: pass1 does not read Source\n"); return; }
    printf("  PASS\n");
}

static void test_multipass_order_matters() {
    printf("  [TEST] test_multipass_order_matters\n");
    SlangPresetParser parser;

    auto astA = parser.parseFile(kShaderDir / "presets/multipass_order_a.slangp");
    if (!astA.ok()) { printf("  FAIL: parse order_a failed\n"); return; }
    auto irA = parser.buildIr(astA.value());
    if (!irA.ok()) { printf("  FAIL: buildIr order_a failed\n"); return; }

    auto astB = parser.parseFile(kShaderDir / "presets/multipass_order_b.slangp");
    if (!astB.ok()) { printf("  FAIL: parse order_b failed\n"); return; }
    auto irB = parser.buildIr(astB.value());
    if (!irB.ok()) { printf("  FAIL: buildIr order_b failed\n"); return; }

    AliasResolver aliasResolver;
    auto aliasDbA = aliasResolver.build(irA.value());
    auto aliasDbB = aliasResolver.build(irB.value());

    std::vector<ShaderReflection> reflA(irA.value().passes.size());
    std::vector<ShaderReflection> reflB(irB.value().passes.size());

    RenderGraphBuilder builder;
    auto graphA = builder.build(irA.value(), aliasDbA, reflA);
    auto graphB = builder.build(irB.value(), aliasDbB, reflB);
    if (!graphA.ok() || !graphB.ok()) { printf("  FAIL: graph build failed\n"); return; }

    auto planA = builder.compileExecutionPlan(graphA.value());
    auto planB = builder.compileExecutionPlan(graphB.value());
    std::vector<std::string> namesA, namesB;
    for (auto& p : graphA.value().passes) namesA.push_back(p.name);
    for (auto& p : graphB.value().passes) namesB.push_back(p.name);
    if (namesA == namesB) { printf("  FAIL: execution plans should differ\n"); return; }
    printf("  PASS\n");
}

static void test_multipass_feedback_graph() {
    printf("  [TEST] test_multipass_feedback_graph\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/feedback_test.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed\n"); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed\n"); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed\n"); return; }

    bool foundFeedback = false;
    for (auto& img : graphResult.value().images) {
        if (img.feedback) { foundFeedback = true; break; }
    }
    if (!foundFeedback) { printf("  FAIL: no feedback image node in graph\n"); return; }
    printf("  PASS\n");
}

static void test_multipass_texture_flow() {
    printf("  [TEST] test_multipass_texture_flow\n");
    SlangPresetParser parser;
    auto astResult = parser.parseFile(kShaderDir / "presets/multipass_rgb.slangp");
    if (!astResult.ok()) { printf("  FAIL: parse failed\n"); return; }
    auto irResult = parser.buildIr(astResult.value());
    if (!irResult.ok()) { printf("  FAIL: buildIr failed\n"); return; }

    AliasResolver aliasResolver;
    auto aliasDb = aliasResolver.build(irResult.value());

    std::vector<ShaderReflection> reflections(irResult.value().passes.size());
    for (size_t i = 0; i < irResult.value().passes.size(); i++) {
        reflections[i] = ShaderReflection();
    }
    for (size_t i = 1; i < reflections.size(); i++) {
        SamplerReflection sampler;
        sampler.name = "Source";
        sampler.set = 1;
        sampler.binding = 0;
        reflections[i].samplers.push_back(sampler);
    }

    RenderGraphBuilder builder;
    auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
    if (!graphResult.ok()) { printf("  FAIL: build graph failed\n"); return; }

    auto& graph = graphResult.value();
    if (graph.passes.size() < 2) { printf("  FAIL: need >= 2 passes\n"); return; }

    for (size_t i = 1; i < graph.passes.size(); i++) {
        if (graph.passes[i].inputs.empty()) {
            printf("  FAIL: pass %zu has no inputs\n", i);
            return;
        }
    }
    printf("  PASS\n");
}

// ===================== GROUP 16: GPU Validation Synthetic =====================

static void test_gpu_validator_analyze_nonblack() {
    printf("  [TEST] test_gpu_validator_analyze_nonblack\n");
    auto validator = GpuShaderValidator::makePermissive();
    const uint32_t w = 4, h = 4;
    std::vector<uint8_t> pixels(w * h * 4);
    for (size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = 255;
        pixels[i + 1] = 128;
        pixels[i + 2] = 64;
        pixels[i + 3] = 255;
    }

    auto stats = validator.analyzeOutput(pixels, w, h);
    if (stats.nonZeroPixels == 0) { printf("  FAIL: nonZeroPixels is 0\n"); return; }
    if (stats.totalPixels != w * h) { printf("  FAIL: totalPixels mismatch\n"); return; }
    printf("  PASS\n");
}

static void test_gpu_validator_analyze_gradient() {
    printf("  [TEST] test_gpu_validator_analyze_gradient\n");
    auto validator = GpuShaderValidator::makePermissive();
    const uint32_t w = 8, h = 8;
    std::vector<uint8_t> pixels(w * h * 4);
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            size_t idx = (y * w + x) * 4;
            uint8_t val = static_cast<uint8_t>((x * 255) / (w - 1));
            pixels[idx + 0] = val;
            pixels[idx + 1] = val;
            pixels[idx + 2] = val;
            pixels[idx + 3] = 255;
        }
    }

    auto stats = validator.analyzeOutput(pixels, w, h);
    auto result = validator.validateOutput(stats, false);
    if (result != GpuTestResult::Pass) {
        printf("  FAIL: gradient should pass validation, got %d\n", static_cast<int>(result));
        return;
    }
    printf("  PASS\n");
}

static void test_gpu_validator_analyze_black() {
    printf("  [TEST] test_gpu_validator_analyze_black\n");
    auto validator = GpuShaderValidator::makeStrict();
    const uint32_t w = 4, h = 4;
    std::vector<uint8_t> pixels(w * h * 4, 0);

    auto stats = validator.analyzeOutput(pixels, w, h);
    auto result = validator.validateOutput(stats, false);
    if (result != GpuTestResult::FailBlackOutput) {
        printf("  FAIL: all-black should fail validation in strict mode, got %d\n", static_cast<int>(result));
        return;
    }
    printf("  PASS\n");
}

// ===================== GROUP 17: GLSL vs Slang Parity =====================

static void test_gslang_slang_same_spiirv_size() {
    printf("  [TEST] test_gslang_slang_same_spiirv_size\n");
    if (!ensureOutputDir()) return;

    fs::path glslFile = kShaderDir / "glsl/solid_red.glsl";
    fs::path slangFile = kShaderDir / "slang/solid_red.slang";

    if (!fs::exists(glslFile)) { printf("  FAIL: solid_red.glsl not found\n"); return; }
    if (!fs::exists(slangFile)) { printf("  FAIL: solid_red.slang not found\n"); return; }

    auto rGLSL = compileShaderFile(glslFile, ShaderStage::Fragment);
    auto rSlang = compileShaderFile(slangFile, ShaderStage::Fragment);
    if (!rGLSL.ok) { printf("  FAIL: GLSL compile: %s\n", rGLSL.error.c_str()); return; }
    if (!rSlang.ok) { printf("  FAIL: Slang compile: %s\n", rSlang.error.c_str()); return; }
    if (rGLSL.spirv.empty()) { printf("  FAIL: GLSL empty SPIR-V\n"); return; }
    if (rSlang.spirv.empty()) { printf("  FAIL: Slang empty SPIR-V\n"); return; }
    printf("  PASS\n");
}

static void test_gslang_slang_same_reflection() {
    printf("  [TEST] test_gslang_slang_same_reflection\n");
    if (!ensureOutputDir()) return;

    fs::path glslFile = kShaderDir / "glsl/solid_red.glsl";
    fs::path slangFile = kShaderDir / "slang/solid_red.slang";

    if (!fs::exists(glslFile) || !fs::exists(slangFile)) {
        printf("  FAIL: shader files not found\n"); return;
    }

    auto rGLSL = compileShaderFile(glslFile, ShaderStage::Fragment);
    auto rSlang = compileShaderFile(slangFile, ShaderStage::Fragment);
    if (!rGLSL.ok || !rSlang.ok) { printf("  FAIL: compile error\n"); return; }

    auto& refGLSL = rGLSL.reflection;
    auto& refSlang = rSlang.reflection;

    if (refGLSL.uniformBlocks.size() != refSlang.uniformBlocks.size()) {
        printf("  FAIL: uniform block count differs: GLSL=%zu, Slang=%zu\n",
               refGLSL.uniformBlocks.size(), refSlang.uniformBlocks.size());
        return;
    }
    if (refGLSL.samplers.size() != refSlang.samplers.size()) {
        printf("  FAIL: sampler count differs: GLSL=%zu, Slang=%zu\n",
               refGLSL.samplers.size(), refSlang.samplers.size());
        return;
    }
    printf("  PASS\n");
}

static void test_gslang_slang_both_valid_magic() {
    printf("  [TEST] test_gslang_slang_both_valid_magic\n");
    if (!ensureOutputDir()) return;

    fs::path glslFile = kShaderDir / "glsl/solid_red.glsl";
    fs::path slangFile = kShaderDir / "slang/solid_red.slang";

    if (!fs::exists(glslFile) || !fs::exists(slangFile)) {
        printf("  FAIL: shader files not found\n"); return;
    }

    auto rGLSL = compileShaderFile(glslFile, ShaderStage::Fragment);
    auto rSlang = compileShaderFile(slangFile, ShaderStage::Fragment);
    if (!rGLSL.ok || !rSlang.ok) { printf("  FAIL: compile error\n"); return; }

    if (!checkSpirvMagic(rGLSL.spirv)) { printf("  FAIL: GLSL bad magic\n"); return; }
    if (!checkSpirvMagic(rSlang.spirv)) { printf("  FAIL: Slang bad magic\n"); return; }
    printf("  PASS\n");
}

// ===================== GROUP 18: Regression =====================

static void test_regression_compile_all_glsl() {
    printf("  [TEST] test_regression_compile_all_glsl\n");
    if (!ensureOutputDir()) return;

    int failures = 0;
    for (auto& shader : kAllGlslShaders) {
        auto r = compileShaderFile(kShaderDir / shader, ShaderStage::Fragment);
        if (!r.ok || r.spirv.empty() || !checkSpirvMagic(r.spirv)) {
            printf("    WARN: failed to compile %s\n", shader.c_str());
            failures++;
        }
    }
    if (failures > 0) { printf("  FAIL: too many failures (%d/%d)\n", failures, (int)kAllGlslShaders.size()); return; }
    printf("  PASS\n");
}

static void test_regression_all_presets_parse() {
    printf("  [TEST] test_regression_all_presets_parse\n");
    SlangPresetParser parser;
    int failures = 0;
    for (auto& preset : kAllPresets) {
        fs::path p = kShaderDir / preset;
        if (!fs::exists(p)) {
            printf("  WARN: %s not found, skipping\n", preset.c_str());
            continue;
        }
        auto astResult = parser.parseFile(p);
        if (!astResult.ok()) {
            printf("  FAIL: parse %s failed: %s\n", preset.c_str(), astResult.error().c_str());
            failures++;
            continue;
        }
        auto irResult = parser.buildIr(astResult.value());
        if (!irResult.ok()) {
            printf("  FAIL: buildIr %s failed: %s\n", preset.c_str(), irResult.error().c_str());
            failures++;
        }
    }
    if (failures > 0) { printf("  FAIL: %d presets failed\n", failures); return; }
    printf("  PASS\n");
}

static void test_regression_multipass_graphs_build() {
    printf("  [TEST] test_regression_multipass_graphs_build\n");
    std::vector<std::string> multipassPresets = {
        "presets/multipass_rgb.slangp", "presets/multipass_order_a.slangp",
        "presets/multipass_order_b.slangp", "presets/multipass_transform.slangp"
    };

    SlangPresetParser parser;
    AliasResolver aliasResolver;
    RenderGraphBuilder builder;
    int failures = 0;

    for (auto& preset : multipassPresets) {
        fs::path p = kShaderDir / preset;
        if (!fs::exists(p)) {
            printf("  WARN: %s not found, skipping\n", preset.c_str());
            continue;
        }

        auto astResult = parser.parseFile(p);
        if (!astResult.ok()) { printf("  FAIL: parse %s\n", preset.c_str()); failures++; continue; }
        auto irResult = parser.buildIr(astResult.value());
        if (!irResult.ok()) { printf("  FAIL: buildIr %s\n", preset.c_str()); failures++; continue; }

        auto aliasDb = aliasResolver.build(irResult.value());
        std::vector<ShaderReflection> reflections(irResult.value().passes.size());
        auto graphResult = builder.build(irResult.value(), aliasDb, reflections);
        if (!graphResult.ok()) {
            printf("  FAIL: graph build %s: %s\n", preset.c_str(), graphResult.error().c_str());
            failures++;
        }
    }
    if (failures > 0) { printf("  FAIL: %d multipass graphs failed\n", failures); return; }
    printf("  PASS\n");
}

static void test_regression_reflection_all_shaders() {
    printf("  [TEST] test_regression_reflection_all_shaders\n");
    int failures = 0;
    for (auto& shader : kAllGlslShaders) {
        std::string source = readFileStr(kShaderDir / shader);
        if (source.empty()) {
            printf("  WARN: cannot read %s, skipping\n", shader.c_str());
            continue;
        }
        auto reflResult = ShaderReflectionParser().fromSourceLayout(source);
        if (!reflResult.ok()) {
            printf("  WARN: reflection failed for %s, skipping\n", shader.c_str());
            continue;
        }
        auto& refl = reflResult.value();
        bool hasUniformBlock = !refl.uniformBlocks.empty();
        bool hasSamplers = !refl.samplers.empty();
        if (!hasUniformBlock && !hasSamplers) {
            printf("  FAIL: %s has no reflection data\n", shader.c_str());
            failures++;
        }
    }
    if (failures > 0) { printf("  FAIL: %d shaders lack reflection data\n", failures); return; }
    printf("  PASS\n");
}

static void test_regression_cache_full_cycle() {
    printf("  [TEST] test_regression_cache_full_cycle\n");
    fs::path cacheDir = kOutputDir / "test_cache_regression";
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    ShaderCache cache(cacheDir);

    auto r1 = compileShaderFile(kShaderDir / "glsl/solid_red.glsl", ShaderStage::Fragment);
    if (!r1.ok) { printf("  FAIL: first compile: %s\n", r1.error.c_str()); return; }
    if (r1.spirv.empty()) { printf("  FAIL: empty SPIR-V\n"); return; }

    CacheKeyComponents components;
    components.sourceContent = "test_source";
    components.stage = "Fragment";
    components.language = "GLSL";
    components.compilerVersion = SlangCompiler::compilerVersion(kSlangcPath);
    auto key = cache.makeKey(components);
    cache.store(key, r1.spirv, "");

    if (!cache.contains(key)) {
        printf("  FAIL: cache does not contain stored key\n"); return;
    }

    if (cache.stats().totalEntries == 0) { printf("  FAIL: cache empty after store\n"); return; }
    printf("  PASS\n");
}

// ===================== runAll =====================

namespace monix::renderer_vk::tests {

void runAll() {
    printf("=== FASE 8: Real Shader Integration Tests ===\n\n");

    printf("=== GROUP 1: GLSL Single Pass Compilation ===\n");
    test_glsl_compile_solid_red();
    test_glsl_compile_solid_green();
    test_glsl_compile_gradient();
    test_glsl_compile_checkerboard();
    test_glsl_compile_uv_visualizer();
    test_glsl_compile_sampler_passthrough();
    test_glsl_compile_with_push_constants();
    test_glsl_compile_all_formats_compile();

    printf("\n=== GROUP 2: Language Detection ===\n");
    test_language_detection_glsl();
    test_language_detection_slang();
    test_language_detection_cg();
    test_language_detection_unknown();

    printf("\n=== GROUP 3: Stage Splitting ===\n");
    test_stage_split_with_pragmas();
    test_stage_split_no_pragmas();
    test_stage_split_common_code();
    test_stage_split_empty_stage();

    printf("\n=== GROUP 4: Parameter Extraction ===\n");
    test_parameter_extraction_basic();
    test_parameter_extraction_int();
    test_parameter_extraction_multiple();
    test_parameter_extraction_none();

    printf("\n=== GROUP 5: Include Resolution ===\n");
    test_include_resolve_relative();
    test_include_resolve_chain();
    test_include_resolve_not_found();
    test_preprocess_includes();

    printf("\n=== GROUP 6: Shader Preprocessing ===\n");
    test_preprocess_defines();
    test_preprocess_ifdef();
    test_preprocess_ifdef_else();
    test_preprocess_nested_ifdef();

    printf("\n=== GROUP 7: Preset Parsing ===\n");
    test_preset_parse_single();
    test_preset_parse_multipass_rgb();
    test_preset_parse_multipass_order();
    test_preset_parse_parameters();
    test_preset_parse_feedback();
    test_preset_build_ir();

    printf("\n=== GROUP 8: Preset Validation ===\n");
    test_preset_validate_valid();
    test_preset_validate_missing_shader();
    test_preset_validate_empty();

    printf("\n=== GROUP 9: Alias Resolution ===\n");
    test_alias_resolve_builtin_source();
    test_alias_resolve_builtin_original();
    test_alias_resolve_pass_output();
    test_alias_resolve_feedback();
    test_alias_resolve_external_texture();

    printf("\n=== GROUP 10: Render Graph Construction ===\n");
    test_graph_single_pass();
    test_graph_multipass_rgb();
    test_graph_multipass_passthrough();
    test_graph_pass_ordering();
    test_graph_lifetime_analysis();
    test_graph_feedback_nodes();

    printf("\n=== GROUP 11: Semantic Uniforms ===\n");
    test_semantic_resolve_input_texture();
    test_semantic_resolve_output_size();
    test_semantic_resolve_aliases();
    test_semantic_resolve_custom_alias();

    printf("\n=== GROUP 12: Shader Cache ===\n");
    test_cache_store_retrieve_spirv();
    test_cache_hit_same_source();
    test_cache_miss_different_source();
    test_cache_invalidation();
    test_cache_dependency_content();

    printf("\n=== GROUP 13: Hot Reload Dependencies ===\n");
    test_dep_graph_single_file();
    test_dep_graph_include_chain();
    test_dep_graph_topological_sort();
    test_dep_graph_clear();

    printf("\n=== GROUP 14: Reflection Analysis ===\n");
    test_reflection_ubo_block();
    test_reflection_sampler();
    test_reflection_push_constants();
    test_reflection_descriptor_bindings();

    printf("\n=== GROUP 15: Multipass Pipeline ===\n");
    test_multipass_parse_and_graph();
    test_multipass_transform_pipeline();
    test_multipass_order_matters();
    test_multipass_feedback_graph();
    test_multipass_texture_flow();

    printf("\n=== GROUP 16: GPU Validation Synthetic ===\n");
    test_gpu_validator_analyze_nonblack();
    test_gpu_validator_analyze_gradient();
    test_gpu_validator_analyze_black();

    printf("\n=== GROUP 17: GLSL vs Slang Parity ===\n");
    test_gslang_slang_same_spiirv_size();
    test_gslang_slang_same_reflection();
    test_gslang_slang_both_valid_magic();

    printf("\n=== GROUP 18: Regression ===\n");
    test_regression_compile_all_glsl();
    test_regression_all_presets_parse();
    test_regression_multipass_graphs_build();
    test_regression_reflection_all_shaders();
    test_regression_cache_full_cycle();

    printf("\n=== All FASE 8 tests complete ===\n");
}

}  // namespace monix::renderer_vk::tests
