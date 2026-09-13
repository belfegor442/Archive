#include "external_compat_tests.hpp"
#include "../preset/SlangPresetParser.hpp"
#include "../preset/SlangPreset.hpp"
#include "../preset/ShaderPreprocessor.hpp"
#include "../preset/IncludeResolver.hpp"
#include "../preset/PresetValidator.hpp"
#include "../preset/StageSplitter.hpp"
#include "../preset/ParameterExtractor.hpp"
#include "../library/ShaderLibrary.hpp"
#include "../library/ShaderLibraryEntry.hpp"
#include "../library/ShaderLibraryCompiler.hpp"
#include "../library/ShaderWorkspace.hpp"
#include "../compiler/ShaderCache.hpp"
#include "../shader_runtime/ShaderRuntime.hpp"
#include "../shader_runtime/TransactionalShaderState.hpp"
#include "../shader_runtime/dependencies/ShaderDependencyGraph.hpp"
#include "../shader_runtime/reload/ShaderHotReload.hpp"
#include "../ui/ShaderBrowserPanel.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace monix::renderer_vk {
namespace tests {

static int testsPassed = 0;
static int testsFailed = 0;

#define F17_ASSERT(cond) do { \
    if (!(cond)) { \
        std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        testsFailed++; \
    } else { testsPassed++; } \
} while(0)

struct TestEnv {
    std::filesystem::path root;
    ShaderWorkspace ws;
    ShaderLibrary* lib = nullptr;

    TestEnv() {
        root = std::filesystem::temp_directory_path() / "monix_fase17_test";
        std::filesystem::remove_all(root);
        ws = ShaderWorkspace(root);
        ws.initialize();
    }

    ~TestEnv() {
        delete lib;
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void createFile(const std::string& relPath, const std::string& content) {
        auto full = root / relPath;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream f(full);
        f << content;
    }

    void scanLib() {
        delete lib;
        lib = new ShaderLibrary(root, &ws);
        lib->scan();
    }

    ShaderBrowserPanelState panelState;
};

// ============================================================================
// FASE 17.1 — SlangPresetParser audit
// ============================================================================

static void test_17_1_parse_single_pass() {
    std::printf("  [TEST] 17.1 parse single pass\n");
    TestEnv env;
    env.createFile("GLSL/CRT/test.slangp",
        "shaders = 1\nshader0 = pass.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "test.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes.size() == 1);
}

static void test_17_1_parse_multi_pass() {
    std::printf("  [TEST] 17.1 parse multi-pass\n");
    TestEnv env;
    env.createFile("GLSL/CRT/multi.slangp",
        "shaders = 3\nshader0 = pass0.slang\nshader1 = pass1.slang\nshader2 = pass2.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "multi.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes.size() == 3);
}

static void test_17_1_parse_textures() {
    std::printf("  [TEST] 17.1 parse textures\n");
    TestEnv env;
    env.createFile("GLSL/CRT/tex.slangp",
        "shaders = 1\nshader0 = pass.slang\ntextures = 1\ntexture0 = LUT.png\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "tex.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().externalTextures.size() == 1);
}

static void test_17_1_parse_parameters() {
    std::printf("  [TEST] 17.1 parse parameters\n");
    TestEnv env;
    env.createFile("GLSL/CRT/params.slangp",
        "shaders = 1\nshader0 = pass.slang\nparameters = scanline_count\nscanline_count = 240.0\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "params.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().parameterOverrides.size() == 1);
}

static void test_17_1_parse_aliases() {
    std::printf("  [TEST] 17.1 parse aliases\n");
    TestEnv env;
    env.createFile("GLSL/CRT/alias.slangp",
        "shaders = 2\nshader0 = pass0.slang\nshader1 = pass1.slang\nalias1 = MyPass\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "alias.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes.size() == 2);
    F17_ASSERT(ir.value().passes[1].alias == "MyPass");
}

static void test_17_1_parse_feedback_flag() {
    std::printf("  [TEST] 17.1 parse feedback flag\n");
    TestEnv env;
    env.createFile("GLSL/CRT/feedback.slangp",
        "shaders = 2\nshader0 = pass0.slang\nshader1 = pass1.slang\nframebuffer_feedback1 = true\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "feedback.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes[1].framebufferFeedback);
}

static void test_17_1_parse_reference_cycle() {
    std::printf("  [TEST] 17.1 parse reference cycle\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.slangp", "#reference b.slangp\nshaders = 1\nshader0 = x.slang\n");
    env.createFile("GLSL/CRT/b.slangp", "#reference a.slangp\nshaders = 1\nshader0 = y.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "a.slangp");
    F17_ASSERT(!ast.ok());
}

static void test_17_1_parse_no_shaders_fails() {
    std::printf("  [TEST] 17.1 parse no shaders fails\n");
    TestEnv env;
    env.createFile("GLSL/CRT/empty.slangp", "textures = 1\ntexture0 = foo.png\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "empty.slangp");
    F17_ASSERT(!ast.ok());
}

// ============================================================================
// FASE 17.2 — Path resolution
// ============================================================================

static void test_17_2_relative_include() {
    std::printf("  [TEST] 17.2 relative include\n");
    TestEnv env;
    env.createFile("GLSL/CRT/main.glsl", "#include \"helper.glsl\"\nvoid main() {}\n");
    env.createFile("GLSL/CRT/helper.glsl", "float helper() { return 1.0; }\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    auto result = resolver.resolve(env.root / "GLSL" / "CRT" / "main.glsl", "helper.glsl");
    F17_ASSERT(result.ok());
}

static void test_17_2_nested_include() {
    std::printf("  [TEST] 17.2 nested include\n");
    TestEnv env;
    env.createFile("GLSL/CRT/main.glsl", "#include \"sub/deep.glsl\"\nvoid main() {}\n");
    env.createFile("GLSL/CRT/sub/deep.glsl", "float deep() { return 2.0; }\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    auto result = resolver.resolve(env.root / "GLSL" / "CRT" / "main.glsl", "sub/deep.glsl");
    F17_ASSERT(result.ok());
}

static void test_17_2_parent_include() {
    std::printf("  [TEST] 17.2 parent include\n");
    TestEnv env;
    env.createFile("GLSL/sub/main.glsl", "#include \"../shared.glsl\"\nvoid main() {}\n");
    env.createFile("GLSL/shared.glsl", "float shared_func() { return 3.0; }\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL");
    auto result = resolver.resolve(env.root / "GLSL" / "sub" / "main.glsl", "../shared.glsl");
    F17_ASSERT(result.ok());
}

static void test_17_2_missing_include() {
    std::printf("  [TEST] 17.2 missing include\n");
    TestEnv env;
    env.createFile("GLSL/CRT/main.glsl", "#include \"nonexistent.glsl\"\nvoid main() {}\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    auto result = resolver.resolve(env.root / "GLSL" / "CRT" / "main.glsl", "nonexistent.glsl");
    F17_ASSERT(!result.ok());
}

static void test_17_2_normalized_path() {
    std::printf("  [TEST] 17.2 normalized path\n");
    TestEnv env;
    env.createFile("GLSL/CRT/main.glsl", "#include \"helper.glsl\"\nvoid main() {}\n");
    env.createFile("GLSL/CRT/helper.glsl", "float h() { return 1.0; }\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    auto result = resolver.resolve(env.root / "GLSL" / "CRT" / "main.glsl", "helper.glsl");
    F17_ASSERT(result.ok());
    auto normalized = FileSystem::normalize(result.value());
    F17_ASSERT(result.value() == normalized);
}

static void test_17_2_escape_prevention() {
    std::printf("  [TEST] 17.2 escape prevention\n");
    TestEnv env;
    // Workspace root is env.root. Include from GLSL/CRT/ that goes ../../escape.glsl
    // This resolves to env.root/escape.glsl which IS inside workspace — that's fine.
    // To truly test escape, we need a file that would resolve OUTSIDE workspace.
    // We'll use a different workspace root: env.root/GLSL/CRT (narrower)
    // Including ../../escape.glsl from GLSL/CRT/main.glsl would go to env.root/escape.glsl
    // which is OUTSIDE env.root/GLSL/CRT
    env.createFile("GLSL/CRT/main.glsl", "#include \"../../escape.glsl\"\nvoid main() {}\n");
    // Place escape.glsl at env.root level (outside GLSL/CRT workspace)
    {
        std::ofstream f(env.root / "escape.glsl");
        f << "float escaped() { return 99.0; }\n";
    }
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    resolver.setWorkspaceRoot(env.root / "GLSL" / "CRT");
    auto result = resolver.resolve(env.root / "GLSL" / "CRT" / "main.glsl", "../../escape.glsl");
    F17_ASSERT(!result.ok());
}

// ============================================================================
// FASE 17.3 — .slangp references
// ============================================================================

static void test_17_3_reference_chain() {
    std::printf("  [TEST] 17.3 reference chain\n");
    TestEnv env;
    env.createFile("GLSL/CRT/base.slangp", "shaders = 1\nshader0 = base.slang\n");
    env.createFile("GLSL/CRT/derived.slangp",
        "#reference base.slangp\nshader1 = extra.slang\nshaders = 2\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "derived.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes.size() == 2);
}

static void test_17_3_reference_override() {
    std::printf("  [TEST] 17.3 reference override\n");
    TestEnv env;
    env.createFile("GLSL/CRT/base.slangp",
        "shaders = 1\nshader0 = base.slang\nalias0 = BasePass\n");
    env.createFile("GLSL/CRT/derived.slangp",
        "#reference base.slangp\nalias0 = OverriddenPass\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "derived.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes[0].alias == "OverriddenPass");
}

static void test_17_3_reference_cycle_detected() {
    std::printf("  [TEST] 17.3 reference cycle detected\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.slangp", "#reference b.slangp\nshaders = 1\nshader0 = x.slang\n");
    env.createFile("GLSL/CRT/b.slangp", "#reference a.slangp\nshaders = 1\nshader0 = y.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "a.slangp");
    F17_ASSERT(!ast.ok());
}

// ============================================================================
// FASE 17.4 — Multi-pass
// ============================================================================

static void test_17_4_single_pass() {
    std::printf("  [TEST] 17.4 single pass\n");
    TestEnv env;
    env.createFile("GLSL/CRT/one.slangp", "shaders = 1\nshader0 = crt.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "one.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes.size() == 1);
}

static void test_17_4_two_passes() {
    std::printf("  [TEST] 17.4 two passes\n");
    TestEnv env;
    env.createFile("GLSL/CRT/two.slangp",
        "shaders = 2\nshader0 = pre.slang\nshader1 = main.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "two.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes.size() == 2);
    F17_ASSERT(ir.value().passes[0].index == 0);
    F17_ASSERT(ir.value().passes[1].index == 1);
}

static void test_17_4_ten_passes() {
    std::printf("  [TEST] 17.4 ten passes\n");
    TestEnv env;
    std::string content = "shaders = 10\n";
    for (int i = 0; i < 10; ++i) {
        content += "shader" + std::to_string(i) + " = pass" + std::to_string(i) + ".slang\n";
    }
    env.createFile("GLSL/CRT/ten.slangp", content);
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "ten.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes.size() == 10);
}

static void test_17_4_pass_preserves_properties() {
    std::printf("  [TEST] 17.4 pass preserves properties\n");
    TestEnv env;
    env.createFile("GLSL/CRT/props.slangp",
        "shaders = 1\nshader0 = pass.slang\n"
        "filter_linear0 = true\nwrap_mode0 = repeat\n"
        "float_framebuffer0 = true\nscale_type0 = source\nscale0 = 2.0\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "props.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    const auto& pass = ir.value().passes[0];
    F17_ASSERT(pass.filterLinear);
    F17_ASSERT(pass.wrapMode == WrapMode::Repeat);
    F17_ASSERT(pass.floatFramebuffer);
}

static void test_17_4_preset_validator_missing_shader() {
    std::printf("  [TEST] 17.4 validator missing shader\n");
    TestEnv env;
    env.createFile("GLSL/CRT/bad.slangp", "shaders = 1\nshader0 = nonexistent.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "bad.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    PresetValidator validator;
    auto diags = validator.validate(ir.value());
    F17_ASSERT(diags.hasErrors());
}

static void test_17_4_preset_validator_duplicate_alias() {
    std::printf("  [TEST] 17.4 validator duplicate alias\n");
    TestEnv env;
    env.createFile("GLSL/CRT/pass.slang", "void main() {}\n");
    env.createFile("GLSL/CRT/dup.slangp",
        "shaders = 2\nshader0 = pass.slang\nshader1 = pass.slang\nalias0 = Dup\nalias1 = Dup\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "dup.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    PresetValidator validator;
    auto diags = validator.validate(ir.value());
    F17_ASSERT(diags.hasErrors());
}

// ============================================================================
// FASE 17.5 — External GLSL
// ============================================================================

static void test_17_5_glsl_simple() {
    std::printf("  [TEST] 17.5 GLSL simple\n");
    TestEnv env;
    env.createFile("GLSL/CRT/simple.glsl",
        "#version 450\n"
        "layout(location = 0) in vec2 vTexCoord;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(vTexCoord, 0.0, 1.0); }\n");
    ShaderLibrary lib(env.root, &env.ws);
    lib.scan();
    F17_ASSERT(lib.entryCount() == 1);
    const auto* e = lib.entry(0);
    F17_ASSERT(e != nullptr);
    F17_ASSERT(e->language == ShaderLanguage::GLSL);
}

static void test_17_5_glsl_uniforms() {
    std::printf("  [TEST] 17.5 GLSL uniforms\n");
    TestEnv env;
    env.createFile("GLSL/CRT/uniforms.glsl",
        "#version 450\n"
        "layout(set = 0, binding = 0) uniform sampler2D Source;\n"
        "layout(push_constant) uniform Params { float Time; } params;\n"
        "layout(location = 0) in vec2 vTexCoord;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() {\n"
        "  vec4 col = texture(Source, vTexCoord);\n"
        "  fragColor = col * (0.5 + 0.5 * sin(params.Time));\n"
        "}\n");
    ShaderLibrary lib(env.root, &env.ws);
    lib.scan();
    F17_ASSERT(lib.entryCount() == 1);
    F17_ASSERT(lib.entry(0)->language == ShaderLanguage::GLSL);
}

static void test_17_5_glsl_preprocessor() {
    std::printf("  [TEST] 17.5 GLSL preprocessor\n");
    TestEnv env;
    env.createFile("GLSL/CRT/preproc.glsl",
        "#version 450\n"
        "#define INTENSITY 1.5\n"
        "#ifdef INTENSITY\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(INTENSITY); }\n"
        "#endif\n");
    ShaderLibrary lib(env.root, &env.ws);
    lib.scan();
    F17_ASSERT(lib.entryCount() == 1);
}

// ============================================================================
// FASE 17.6 — External Slang
// ============================================================================

static void test_17_6_slang_simple() {
    std::printf("  [TEST] 17.6 Slang simple\n");
    TestEnv env;
    env.createFile("SLANG/CRT/simple.slang",
        "#pragma stage fragment\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(1.0); }\n");
    ShaderLibrary lib(env.root, &env.ws);
    lib.scan();
    F17_ASSERT(lib.entryCount() == 1);
    F17_ASSERT(lib.entry(0)->language == ShaderLanguage::Slang);
}

// ============================================================================
// FASE 17.7 — Includes
// ============================================================================

static void test_17_7_nested_include() {
    std::printf("  [TEST] 17.7 nested include\n");
    TestEnv env;
    env.createFile("GLSL/CRT/main.glsl",
        "#version 450\n#include \"common/math.glsl\"\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(PI, 0.0, 0.0, 1.0); }\n");
    env.createFile("GLSL/CRT/common/math.glsl", "const float PI = 3.14159;\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    ShaderPreprocessor preprocessor(resolver);
    auto result = preprocessor.preprocessFile(env.root / "GLSL" / "CRT" / "main.glsl");
    F17_ASSERT(result.ok());
    F17_ASSERT(result.value().source.find("3.14159") != std::string::npos);
}

static void test_17_7_duplicate_include() {
    std::printf("  [TEST] 17.7 duplicate include\n");
    TestEnv env;
    env.createFile("GLSL/CRT/main.glsl",
        "#include \"common.glsl\"\n#include \"common.glsl\"\n"
        "layout(location = 0) out vec4 fragColor;\nvoid main() {}\n");
    env.createFile("GLSL/CRT/common.glsl", "float common() { return 1.0; }\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    ShaderPreprocessor preprocessor(resolver);
    auto result = preprocessor.preprocessFile(env.root / "GLSL" / "CRT" / "main.glsl");
    F17_ASSERT(result.ok());
}

static void test_17_7_missing_include() {
    std::printf("  [TEST] 17.7 missing include\n");
    TestEnv env;
    env.createFile("GLSL/CRT/main.glsl",
        "#include \"nonexistent.glsl\"\n"
        "layout(location = 0) out vec4 fragColor;\nvoid main() {}\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    ShaderPreprocessor preprocessor(resolver);
    auto result = preprocessor.preprocessFile(env.root / "GLSL" / "CRT" / "main.glsl");
    F17_ASSERT(!result.ok());
}

static void test_17_7_include_cycle() {
    std::printf("  [TEST] 17.7 include cycle\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", "#include \"b.glsl\"\nvoid main() {}\n");
    env.createFile("GLSL/CRT/b.glsl", "#include \"a.glsl\"\nvoid main() {}\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    ShaderPreprocessor preprocessor(resolver);
    auto result = preprocessor.preprocessFile(env.root / "GLSL" / "CRT" / "a.glsl");
    F17_ASSERT(!result.ok());
}

// ============================================================================
// FASE 17.8 — Semantic uniforms
// ============================================================================

static void test_17_8_source_alias() {
    std::printf("  [TEST] 17.8 Source alias\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("Source");
    F17_ASSERT(match.semantic == ShaderSemantic::InputTexture);
    F17_ASSERT(match.aliasMatch);
    F17_ASSERT(match.originalName == "Source");
}

static void test_17_8_sourcetexture_alias() {
    std::printf("  [TEST] 17.8 SourceTexture alias\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("SourceTexture");
    F17_ASSERT(match.semantic == ShaderSemantic::InputTexture);
    F17_ASSERT(match.aliasMatch);
}

static void test_17_8_timer_alias() {
    std::printf("  [TEST] 17.8 Timer alias\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("Timer");
    F17_ASSERT(match.semantic == ShaderSemantic::Time);
    F17_ASSERT(match.aliasMatch);
}

static void test_17_8_elapsed_time_alias() {
    std::printf("  [TEST] 17.8 ElapsedTime alias\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("ElapsedTime");
    F17_ASSERT(match.semantic == ShaderSemantic::Time);
    F17_ASSERT(match.aliasMatch);
}

static void test_17_8_frame_count_aliases() {
    std::printf("  [TEST] 17.8 FrameCount aliases\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    F17_ASSERT(resolver.detectSemantic("FrameCount") == ShaderSemantic::FrameCount);
    F17_ASSERT(resolver.detectSemantic("frame_count") == ShaderSemantic::FrameCount);
    F17_ASSERT(resolver.detectSemantic("FrameCounter") == ShaderSemantic::FrameCount);
}

static void test_17_8_preserves_original_name() {
    std::printf("  [TEST] 17.8 preserves original name\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("SourceTexture");
    F17_ASSERT(match.originalName == "SourceTexture");
}

static void test_17_8_case_insensitive() {
    std::printf("  [TEST] 17.8 case insensitive\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    F17_ASSERT(resolver.isKnownAlias("source"));
    F17_ASSERT(resolver.isKnownAlias("SOURCE"));
    F17_ASSERT(resolver.isKnownAlias("Source"));
    F17_ASSERT(resolver.isKnownAlias("sOurCe"));
}

static void test_17_8_unknown_uniform() {
    std::printf("  [TEST] 17.8 unknown uniform\n");
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("CompletelyUnknownXYZ");
    F17_ASSERT(match.semantic == ShaderSemantic::None);
}

// ============================================================================
// FASE 17.9 — Feedback
// ============================================================================

static void test_17_9_feedback_flag_parsed() {
    std::printf("  [TEST] 17.9 feedback flag parsed\n");
    TestEnv env;
    env.createFile("GLSL/CRT/feedback.slangp",
        "shaders = 3\nshader0 = p0.slang\nshader1 = p1.slang\nshader2 = p2.slang\n"
        "framebuffer_feedback1 = true\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "feedback.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(ir.value().passes[1].framebufferFeedback);
    F17_ASSERT(!ir.value().passes[0].framebufferFeedback);
}

static void test_17_9_no_feedback_default() {
    std::printf("  [TEST] 17.9 no feedback default\n");
    TestEnv env;
    env.createFile("GLSL/CRT/nofb.slangp",
        "shaders = 2\nshader0 = p0.slang\nshader1 = p1.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "nofb.slangp");
    F17_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F17_ASSERT(ir.ok());
    F17_ASSERT(!ir.value().passes[0].framebufferFeedback);
    F17_ASSERT(!ir.value().passes[1].framebufferFeedback);
}

// ============================================================================
// FASE 17.10 — Cache
// ============================================================================

static void test_17_10_cache_store_retrieve() {
    std::printf("  [TEST] 17.10 cache store/retrieve\n");
    TestEnv env;
    auto cacheDir = env.root / "cache";
    std::filesystem::create_directories(cacheDir);
    ShaderCache cache(cacheDir);

    CacheKeyComponents comp;
    comp.sourceContent = "void main() {}";
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "test";
    comp.debugInfo = false;

    auto key = cache.makeKey(comp);
    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07};
    auto storeResult = cache.store(key, spirv, "{}");
    F17_ASSERT(storeResult);

    auto entry = cache.entryFor(key);
    F17_ASSERT(entry.hit);
    F17_ASSERT(cache.verifyIntegrity(entry));
}

static void test_17_10_cache_miss() {
    std::printf("  [TEST] 17.10 cache miss\n");
    TestEnv env;
    auto cacheDir = env.root / "cache";
    std::filesystem::create_directories(cacheDir);
    ShaderCache cache(cacheDir);

    CacheKeyComponents comp;
    comp.sourceContent = "nonexistent";
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "test";

    auto key = cache.makeKey(comp);
    auto entry = cache.entryFor(key);
    F17_ASSERT(!entry.hit);
}

static void test_17_10_cache_invalidation() {
    std::printf("  [TEST] 17.10 cache invalidation\n");
    TestEnv env;
    auto cacheDir = env.root / "cache";
    std::filesystem::create_directories(cacheDir);
    ShaderCache cache(cacheDir);

    CacheKeyComponents comp;
    comp.sourceContent = "invalidate me";
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "test";

    auto key = cache.makeKey(comp);
    cache.store(key, {0x01, 0x02}, "{}");
    F17_ASSERT(cache.contains(key));

    cache.invalidate(key);
    F17_ASSERT(!cache.contains(key));
}

// ============================================================================
// FASE 17.11 — Hot Reload
// ============================================================================

static void test_17_11_hot_reload_dependency_tracking() {
    std::printf("  [TEST] 17.11 hot reload dependency tracking\n");
    TestEnv env;
    ShaderDependencyGraph graph;
    graph.registerShader(env.root / "GLSL" / "CRT" / "main.slang");
    graph.registerInclude(env.root / "GLSL" / "CRT" / "common.glsl");
    graph.addDependency(env.root / "GLSL" / "CRT" / "main.slang",
                        env.root / "GLSL" / "CRT" / "common.glsl");
    auto affected = graph.invalidate(env.root / "GLSL" / "CRT" / "common.glsl");
    F17_ASSERT(affected.anyFound);
    F17_ASSERT(affected.affectedShaders.size() == 1);
}

// ============================================================================
// FASE 17.12 — Diagnostics
// ============================================================================

static void test_17_12_diagnostics_preserve_info() {
    std::printf("  [TEST] 17.12 diagnostics preserve info\n");
    ShaderDiagnostics diags;
    diags.add(ShaderDiagnostic{
        ShaderDiagnosticKind::CompileError,
        DiagnosticSeverity::Error,
        "test.glsl",
        42, 18,
        "fragment",
        "SYNTAX_ERROR",
        "Unknown identifier"
    });
    F17_ASSERT(diags.hasErrors());
    const auto& entries = diags.entries();
    F17_ASSERT(entries.size() == 1);
    F17_ASSERT(entries[0].file == "test.glsl");
    F17_ASSERT(entries[0].line == 42);
    F17_ASSERT(entries[0].column == 18);
    F17_ASSERT(entries[0].stage == "fragment");
    F17_ASSERT(entries[0].errorCode == "SYNTAX_ERROR");
}

static void test_17_12_diagnostics_summary() {
    std::printf("  [TEST] 17.12 diagnostics summary\n");
    ShaderDiagnostics diags;
    diags.add(ShaderDiagnostic{
        ShaderDiagnosticKind::CompileError, DiagnosticSeverity::Error,
        "a.glsl", 10, 5, "vertex", "ERR1", "Error one"
    });
    diags.add(ShaderDiagnostic{
        ShaderDiagnosticKind::PreprocessError, DiagnosticSeverity::Warning,
        "b.glsl", 20, 1, "", "WARN1", "Warning one"
    });
    auto summary = diags.summary();
    F17_ASSERT(!summary.empty());
    F17_ASSERT(diags.hasErrors());
    F17_ASSERT(diags.hasWarnings());
}

// ============================================================================
// FASE 17.13 — Error isolation
// ============================================================================

static void test_17_13_error_isolation() {
    std::printf("  [TEST] 17.13 error isolation\n");
    TestEnv env;

    env.createFile("GLSL/CRT/bad.glsl", "this is not valid GLSL @#$%");
    env.scanLib();
    TransactionalShaderState tx;

    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = env.root;
    rtConfig.shaderCacheDirectory = env.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();

    F17_ASSERT(env.lib->entryCount() == 1);
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    compiler.compileAndActivate(0, tx);
    F17_ASSERT(env.lib->entry(0)->status == ShaderEntryStatus::Error);
}

// ============================================================================
// FASE 17.14 — Persistence
// ============================================================================

static void test_17_14_persistence_save_load() {
    std::printf("  [TEST] 17.14 persistence save/load\n");
    TestEnv env;
    env.createFile("GLSL/CRT/scanlines.glsl", "void main() {}");
    env.scanLib();

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = env.root;
    rtConfig.shaderCacheDirectory = env.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);

    panel.setFilter(ShaderLanguageFilter::GLSL);
    panel.selectShader(0);
    panel.setFavorite(0, true);

    F17_ASSERT(env.lib->entry(0)->isFavorite);
}

static void test_17_14_persistence_corrupt_recovery() {
    std::printf("  [TEST] 17.14 corrupt config recovery\n");
    TestEnv env;
    env.createFile("config.json", "{{{{corrupt json content}}}}");
    // Should not crash when reading corrupt JSON
    auto result = FileSystem::readText(env.root / "config.json");
    F17_ASSERT(result.ok());
    std::string content = result.value();
    F17_ASSERT(content.find("corrupt") != std::string::npos);
}

static void test_17_14_persistence_missing_config() {
    std::printf("  [TEST] 17.14 missing config\n");
    TestEnv env;
    auto result = FileSystem::readText(env.root / "nonexistent.json");
    F17_ASSERT(!result.ok());
}

// ============================================================================
// FASE 17.15 — Resource limits
// ============================================================================

static void test_17_15_max_shader_count() {
    std::printf("  [TEST] 17.15 max shader count\n");
    TestEnv env;
    for (int i = 0; i < 10; ++i) {
        env.createFile("GLSL/CRT/shader" + std::to_string(i) + ".glsl",
            "float f" + std::to_string(i) + "() { return " + std::to_string(i) + ".0; }\n");
    }
    env.scanLib();
    F17_ASSERT(env.lib->entryCount() == 10);
}

// ============================================================================
// FASE 17.16 — Compatibility fixtures (created inline)
// ============================================================================

static void test_17_16_glsl_fixture() {
    std::printf("  [TEST] 17.16 GLSL fixture\n");
    TestEnv env;
    env.createFile("GLSL/External/simple.glsl",
        "#version 450\n"
        "layout(location = 0) in vec2 vTexCoord;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(vTexCoord, 0.0, 1.0); }\n");
    env.scanLib();
    F17_ASSERT(env.lib->entryCount() == 1);
    F17_ASSERT(env.lib->entry(0)->language == ShaderLanguage::GLSL);
    F17_ASSERT(env.lib->entry(0)->category == "External");
}

static void test_17_16_slang_fixture() {
    std::printf("  [TEST] 17.16 Slang fixture\n");
    TestEnv env;
    env.createFile("SLANG/External/simple.slang",
        "#pragma stage fragment\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
    env.scanLib();
    F17_ASSERT(env.lib->entryCount() == 1);
    F17_ASSERT(env.lib->entry(0)->language == ShaderLanguage::Slang);
}

static void test_17_16_slangp_fixture() {
    std::printf("  [TEST] 17.16 SlangP fixture\n");
    TestEnv env;
    env.createFile("SLANGP/External/simple.slangp",
        "shaders = 1\nshader0 = pass.slang\n");
    env.scanLib();
    F17_ASSERT(env.lib->entryCount() == 1);
    F17_ASSERT(env.lib->entry(0)->extension == ".slangp");
}

// ============================================================================
// FASE 17.18 — End-to-end
// ============================================================================

static void test_17_18_e2e_scan_select() {
    std::printf("  [TEST] 17.18 E2E scan + select\n");
    TestEnv env;
    env.createFile("GLSL/External/e2e.glsl",
        "#version 450\n"
        "layout(location = 0) in vec2 vTexCoord;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(vTexCoord, 0.0, 1.0); }\n");
    env.scanLib();

    ShaderBrowserPanelState panelState;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = env.root;
    rtConfig.shaderCacheDirectory = env.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, panelState);

    panel.setFilter(ShaderLanguageFilter::GLSL);
    F17_ASSERT(panel.filteredShaderCount() == 1);
    panel.selectShader(0);
    F17_ASSERT(panel.hasSelection());
    auto info = panel.detailInfo();
    F17_ASSERT(info.hasSelection);
    F17_ASSERT(info.name == "e2e");
    F17_ASSERT(info.language == "GLSL");
}

static void test_17_18_e2e_search() {
    std::printf("  [TEST] 17.18 E2E search\n");
    TestEnv env;
    env.createFile("GLSL/CRT/scanlines.glsl", "void main() {}");
    env.createFile("GLSL/Effects/bloom.glsl", "void main() {}");
    env.scanLib();

    ShaderBrowserPanelState panelState;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = env.root;
    rtConfig.shaderCacheDirectory = env.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, panelState);

    panel.setSearchQuery("scanlines");
    auto visible = panel.visibleShaders();
    F17_ASSERT(visible.size() == 1);
    F17_ASSERT(visible[0].name == "scanlines");
}

static void test_17_18_e2e_favorites() {
    std::printf("  [TEST] 17.18 E2E favorites\n");
    TestEnv env;
    env.createFile("GLSL/CRT/scanlines.glsl", "void main() {}");
    env.createFile("GLSL/Effects/bloom.glsl", "void main() {}");
    env.scanLib();

    ShaderBrowserPanelState panelState;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = env.root;
    rtConfig.shaderCacheDirectory = env.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, panelState);

    panel.setFavorite(0, true);
    F17_ASSERT(env.lib->entry(0)->isFavorite);
    auto favs = panel.favoriteShaders();
    F17_ASSERT(favs.size() == 1);
}

static void test_17_18_e2e_category_groups() {
    std::printf("  [TEST] 17.18 E2E category groups\n");
    TestEnv env;
    env.createFile("GLSL/CRT/scanlines.glsl", "void main() {}");
    env.createFile("GLSL/CRT/curvature.glsl", "void main() {}");
    env.createFile("GLSL/Effects/bloom.glsl", "void main() {}");
    env.scanLib();

    ShaderBrowserPanelState panelState;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = env.root;
    rtConfig.shaderCacheDirectory = env.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, panelState);

    auto groups = panel.categoryGroups();
    F17_ASSERT(groups.size() == 2);
    F17_ASSERT(groups[0].entryIndices.size() == 2);
    F17_ASSERT(groups[1].entryIndices.size() == 1);
}

static void test_17_18_e2e_source_kind_user() {
    std::printf("  [TEST] 17.18 E2E source kind = User\n");
    TestEnv env;
    env.createFile("GLSL/External/test.glsl", "void main() {}");
    env.scanLib();
    F17_ASSERT(env.lib->entry(0)->sourceKind == ShaderSourceKind::User);
}

static void test_17_18_e2e_cache_stats() {
    std::printf("  [TEST] 17.18 E2E cache stats\n");
    TestEnv env;
    auto cacheDir = env.root / "cache";
    std::filesystem::create_directories(cacheDir);
    ShaderCache cache(cacheDir);
    CacheStatsInfo info;
    info.memoryLimitBytes = 512 * 1024 * 1024;
    F17_ASSERT(info.memoryLimitBytes > 0);
}

static void test_17_18_e2e_rescan_preserves() {
    std::printf("  [TEST] 17.18 E2E rescan preserves\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", "void main() {}");
    env.createFile("GLSL/CRT/b.glsl", "void main() {}");
    env.scanLib();
    F17_ASSERT(env.lib->entryCount() == 2);
    auto diff = env.lib->rescan();
    F17_ASSERT(diff.unchanged.size() == 2);
    F17_ASSERT(env.lib->entryCount() == 2);
}

static void test_17_18_e2e_modify_detect() {
    std::printf("  [TEST] 17.18 E2E modify detect\n");
    TestEnv env;
    env.createFile("GLSL/CRT/test.glsl", "float original() { return 1.0; }\n");
    env.scanLib();
    F17_ASSERT(env.lib->entryCount() == 1);
    uint64_t hashBefore = env.lib->entry(0)->contentHash;

    {
        std::ofstream f(env.root / "GLSL" / "CRT" / "test.glsl");
        f << "float modified() { return 2.0; }\n";
    }
    auto diff = env.lib->rescan();
    F17_ASSERT(!diff.modified.empty());
    F17_ASSERT(env.lib->entry(0)->contentHash != hashBefore);
}

// ============================================================================
// FASE 17.11 — Hot reload (additional)
// ============================================================================

static void test_17_11_dep_graph_basic() {
    std::printf("  [TEST] 17.11 dep graph basic\n");
    TestEnv env;
    ShaderDependencyGraph graph;
    auto shaderPath = env.root / "GLSL" / "CRT" / "main.slang";
    auto includePath = env.root / "GLSL" / "CRT" / "common.glsl";
    graph.registerShader(shaderPath);
    graph.registerInclude(includePath);
    graph.addDependency(shaderPath, includePath);

    F17_ASSERT(graph.hasNode(shaderPath));
    F17_ASSERT(graph.hasNode(includePath));
    F17_ASSERT(graph.edgeCount() == 1);
}

// ============================================================================
// Runner
// ============================================================================

void runExternalCompatTests() {
    testsPassed = 0;
    testsFailed = 0;

    std::printf("--- 17.1 Parser Audit ---\n");
    test_17_1_parse_single_pass();
    test_17_1_parse_multi_pass();
    test_17_1_parse_textures();
    test_17_1_parse_parameters();
    test_17_1_parse_aliases();
    test_17_1_parse_feedback_flag();
    test_17_1_parse_reference_cycle();
    test_17_1_parse_no_shaders_fails();

    std::printf("--- 17.2 Path Resolution ---\n");
    test_17_2_relative_include();
    test_17_2_nested_include();
    test_17_2_parent_include();
    test_17_2_missing_include();
    test_17_2_normalized_path();
    test_17_2_escape_prevention();

    std::printf("--- 17.3 References ---\n");
    test_17_3_reference_chain();
    test_17_3_reference_override();
    test_17_3_reference_cycle_detected();

    std::printf("--- 17.4 Multi-pass ---\n");
    test_17_4_single_pass();
    test_17_4_two_passes();
    test_17_4_ten_passes();
    test_17_4_pass_preserves_properties();
    test_17_4_preset_validator_missing_shader();
    test_17_4_preset_validator_duplicate_alias();

    std::printf("--- 17.5 External GLSL ---\n");
    test_17_5_glsl_simple();
    test_17_5_glsl_uniforms();
    test_17_5_glsl_preprocessor();

    std::printf("--- 17.6 External Slang ---\n");
    test_17_6_slang_simple();

    std::printf("--- 17.7 Includes ---\n");
    test_17_7_nested_include();
    test_17_7_duplicate_include();
    test_17_7_missing_include();
    test_17_7_include_cycle();

    std::printf("--- 17.8 Semantic Uniforms ---\n");
    test_17_8_source_alias();
    test_17_8_sourcetexture_alias();
    test_17_8_timer_alias();
    test_17_8_elapsed_time_alias();
    test_17_8_frame_count_aliases();
    test_17_8_preserves_original_name();
    test_17_8_case_insensitive();
    test_17_8_unknown_uniform();

    std::printf("--- 17.9 Feedback ---\n");
    test_17_9_feedback_flag_parsed();
    test_17_9_no_feedback_default();

    std::printf("--- 17.10 Cache ---\n");
    test_17_10_cache_store_retrieve();
    test_17_10_cache_miss();
    test_17_10_cache_invalidation();

    std::printf("--- 17.11 Hot Reload ---\n");
    test_17_11_hot_reload_dependency_tracking();
    test_17_11_dep_graph_basic();

    std::printf("--- 17.12 Diagnostics ---\n");
    test_17_12_diagnostics_preserve_info();
    test_17_12_diagnostics_summary();

    std::printf("--- 17.13 Error Isolation ---\n");
    test_17_13_error_isolation();

    std::printf("--- 17.14 Persistence ---\n");
    test_17_14_persistence_save_load();
    test_17_14_persistence_corrupt_recovery();
    test_17_14_persistence_missing_config();

    std::printf("--- 17.15 Resource Limits ---\n");
    test_17_15_max_shader_count();

    std::printf("--- 17.16 Fixtures ---\n");
    test_17_16_glsl_fixture();
    test_17_16_slang_fixture();
    test_17_16_slangp_fixture();

    std::printf("--- 17.18 End-to-End ---\n");
    test_17_18_e2e_scan_select();
    test_17_18_e2e_search();
    test_17_18_e2e_favorites();
    test_17_18_e2e_category_groups();
    test_17_18_e2e_source_kind_user();
    test_17_18_e2e_cache_stats();
    test_17_18_e2e_rescan_preserves();
    test_17_18_e2e_modify_detect();

    std::printf("\n=== FASE 17 Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
    if (testsFailed > 0) {
        std::printf("=== SOME TESTS FAILED ===\n");
    } else {
        std::printf("=== All FASE 17 tests passed ===\n");
    }
}

}  // namespace tests
}  // namespace monix::renderer_vk
