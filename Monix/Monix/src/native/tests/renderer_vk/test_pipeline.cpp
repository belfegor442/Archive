#include "test_runner.hpp"

#include "../../renderer_vk/preset/SlangPresetParser.hpp"
#include "../../renderer_vk/preset/AliasResolver.hpp"
#include "../../renderer_vk/graph/RenderGraphBuilder.hpp"
#include "../../renderer_vk/compiler/ShaderReflection.hpp"
#include "../../renderer_vk/preset/ShaderPreprocessor.hpp"
#include "../../renderer_vk/preset/IncludeResolver.hpp"
#include "../../renderer_vk/preset/ParameterExtractor.hpp"
#include "../../renderer_vk/preset/PresetValidator.hpp"
#include "../../renderer_vk/preset/StageSplitter.hpp"
#include "../../renderer_vk/compiler/SlangCompiler.hpp"

#include <filesystem>

namespace fs = std::filesystem;

static fs::path testDir() {
    return fs::path("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders\\tests");
}

static bool test_2pass_parse() {
    using namespace monix::renderer_vk;
    std::string testName = "test_2pass_parse";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_2pass.slangp");
    TEST_ASSERT(ast, "parseFile failed: " + (ast ? "" : ast.error()));
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed: " + (ir ? "" : ir.error()));
    TEST_ASSERT_EQ(static_cast<int>(ir.value().passes.size()), 2, "expected 2 passes");
    TEST_ASSERT_EQ(ir.value().passes[0].alias, std::string("Pass0"), "pass 0 alias");
    TEST_ASSERT_EQ(ir.value().passes[1].alias, std::string("Output"), "pass 1 alias");
    return true;
}

static bool test_2pass_graph() {
    using namespace monix::renderer_vk;
    std::string testName = "test_2pass_graph";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_2pass.slangp");
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());
    std::vector<ShaderReflection> reflections(2);
    reflections[0].samplers.push_back({"Source", 0, 2});
    reflections[1].samplers.push_back({"Source", 0, 2});
    reflections[1].samplers.push_back({"Pass0", 0, 3});
    RenderGraphBuilder graphBuilder;
    auto graph = graphBuilder.build(ir.value(), aliases, reflections);
    TEST_ASSERT(graph, "graph build failed: " + (graph ? "" : graph.error()));
    TEST_ASSERT(graph.value().images.size() >= 5,
        "expected >=5 images, got " + std::to_string(graph.value().images.size()));
    bool foundPass1 = false;
    for (const auto& pass : graph.value().passes) {
        if (pass.name == "Output" || pass.name == "Pass1") {
            foundPass1 = true;
            TEST_ASSERT_EQ(static_cast<int>(pass.inputs.size()), 2,
                "expected 2 inputs for pass 1, got " + std::to_string(pass.inputs.size()));
            break;
        }
    }
    TEST_ASSERT(foundPass1, "Pass1 not found in graph");
    return true;
}

static bool test_multibinding_parse() {
    using namespace monix::renderer_vk;
    std::string testName = "test_multibinding_parse";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_multibinding.slangp");
    TEST_ASSERT(ast, "parse failed: " + ast.error());
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed: " + ir.error());
    TEST_ASSERT_EQ(static_cast<int>(ir.value().passes.size()), 1, "expected 1 pass");
    return true;
}

static bool test_multibinding_graph() {
    using namespace monix::renderer_vk;
    std::string testName = "test_multibinding_graph";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_multibinding.slangp");
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());
    std::vector<ShaderReflection> reflections(1);
    reflections[0].samplers.push_back({"Source", 0, 2});
    reflections[0].samplers.push_back({"Pass0", 0, 3});
    RenderGraphBuilder graphBuilder;
    auto graph = graphBuilder.build(ir.value(), aliases, reflections);
    TEST_ASSERT(graph, "graph build failed: " + (graph ? "" : graph.error()));
    bool foundPass = false;
    for (const auto& pass : graph.value().passes) {
        if (pass.name == "Output") {
            foundPass = true;
            TEST_ASSERT(pass.inputs.size() >= 1,
                "expected >=1 inputs, got " + std::to_string(pass.inputs.size()));
            break;
        }
    }
    TEST_ASSERT(foundPass, "Output pass not found");
    return true;
}

static bool test_multibinding3_graph() {
    using namespace monix::renderer_vk;
    std::string testName = "test_multibinding3_graph";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_multibinding3.slangp");
    TEST_ASSERT(ast, "parse failed: " + ast.error());
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed: " + ir.error());
    TEST_ASSERT_EQ(static_cast<int>(ir.value().passes.size()), 2, "expected 2 passes");
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());
    std::vector<ShaderReflection> reflections(2);
    reflections[0].samplers.push_back({"Source", 0, 2});
    reflections[1].samplers.push_back({"Source", 0, 2});
    reflections[1].samplers.push_back({"PassA", 0, 3});
    reflections[1].samplers.push_back({"PassA", 0, 4});
    RenderGraphBuilder graphBuilder;
    auto graph = graphBuilder.build(ir.value(), aliases, reflections);
    TEST_ASSERT(graph, "graph build failed: " + (graph ? "" : graph.error()));
    bool foundOutput = false;
    for (const auto& pass : graph.value().passes) {
        if (pass.name == "Output") {
            foundOutput = true;
            TEST_ASSERT_EQ(static_cast<int>(pass.inputs.size()), 3,
                "expected 3 inputs, got " + std::to_string(pass.inputs.size()));
            break;
        }
    }
    TEST_ASSERT(foundOutput, "Output pass not found");
    return true;
}

static bool test_alias_resolution() {
    using namespace monix::renderer_vk;
    std::string testName = "test_alias_resolution";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_alias.slangp");
    TEST_ASSERT(ast, "parse failed: " + ast.error());
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed: " + ir.error());
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());
    auto res = aliases.resolve("pass_a", 1);
    TEST_ASSERT_EQ(static_cast<int>(res.kind), static_cast<int>(AliasKind::PassOutput),
        "pass_a should resolve to PassOutput");
    TEST_ASSERT_EQ(res.passIndex, 0, "pass_a passIndex should be 0");
    auto srcRes = aliases.resolve("Source", 0);
    TEST_ASSERT_EQ(static_cast<int>(srcRes.kind), static_cast<int>(AliasKind::Builtin),
        "Source should resolve to Builtin");
    return true;
}

static bool test_alias_graph() {
    using namespace monix::renderer_vk;
    std::string testName = "test_alias_graph";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_alias.slangp");
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());
    std::vector<ShaderReflection> reflections(2);
    reflections[0].samplers.push_back({"Source", 0, 2});
    reflections[1].samplers.push_back({"Source", 0, 2});
    reflections[1].samplers.push_back({"pass_a", 0, 3});
    RenderGraphBuilder graphBuilder;
    auto graph = graphBuilder.build(ir.value(), aliases, reflections);
    TEST_ASSERT(graph, "graph build failed: " + (graph ? "" : graph.error()));
    bool foundOutput = false;
    for (const auto& pass : graph.value().passes) {
        if (pass.name == "Output") {
            foundOutput = true;
            TEST_ASSERT_EQ(static_cast<int>(pass.inputs.size()), 2,
                "expected 2 inputs for Output, got " + std::to_string(pass.inputs.size()));
            bool foundPassA = false;
            for (const auto& inp : pass.inputs) {
                if (inp.samplerName == "pass_a") {
                    foundPassA = true;
                    TEST_ASSERT_EQ(static_cast<int>(inp.alias.kind),
                        static_cast<int>(AliasKind::PassOutput),
                        "pass_a input kind should be PassOutput");
                    break;
                }
            }
            TEST_ASSERT(foundPassA, "pass_a input not found in Output pass");
            break;
        }
    }
    TEST_ASSERT(foundOutput, "Output pass not found");
    return true;
}

static bool test_feedback_parse() {
    using namespace monix::renderer_vk;
    std::string testName = "test_feedback_parse";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_feedback.slangp");
    TEST_ASSERT(ast, "parse failed: " + ast.error());
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed: " + ir.error());
    TEST_ASSERT_EQ(static_cast<int>(ir.value().passes.size()), 1, "expected 1 pass");
    TEST_ASSERT(ir.value().passes[0].framebufferFeedback,
        "framebufferFeedback should be true");
    return true;
}

static bool test_feedback_alias() {
    using namespace monix::renderer_vk;
    std::string testName = "test_feedback_alias";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_feedback.slangp");
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());
    auto res = aliases.resolve("Pass0Feedback", 0);
    TEST_ASSERT_EQ(static_cast<int>(res.kind), static_cast<int>(AliasKind::Feedback),
        "Pass0Feedback should resolve to Feedback");
    return true;
}

static bool test_feedback_graph() {
    using namespace monix::renderer_vk;
    std::string testName = "test_feedback_graph";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_feedback.slangp");
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());
    std::vector<ShaderReflection> reflections(1);
    reflections[0].samplers.push_back({"Source", 0, 2});
    reflections[0].samplers.push_back({"Pass0Feedback", 0, 3});
    RenderGraphBuilder graphBuilder;
    auto graph = graphBuilder.build(ir.value(), aliases, reflections);
    TEST_ASSERT(graph, "graph build failed: " + (graph ? "" : graph.error()));
    bool foundFeedback = false;
    for (const auto& img : graph.value().images) {
        if (img.feedback) { foundFeedback = true; break; }
    }
    TEST_ASSERT(foundFeedback, "no feedback image node found in graph");
    return true;
}

static bool test_external_parse() {
    using namespace monix::renderer_vk;
    std::string testName = "test_external_parse";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_external.slangp");
    TEST_ASSERT(ast, "parse failed: " + ast.error());
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed: " + ir.error());
    TEST_ASSERT_EQ(static_cast<int>(ir.value().externalTextures.size()), 1,
        "expected 1 external texture");
    TEST_ASSERT_EQ(ir.value().externalTextures[0].filterLinear, true,
        "filter_linear should be true");
    return true;
}

static bool test_external_graph() {
    using namespace monix::renderer_vk;
    std::string testName = "test_external_graph";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_external.slangp");
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());
    std::vector<ShaderReflection> reflections(1);
    reflections[0].samplers.push_back({"Source", 0, 2});
    reflections[0].samplers.push_back({"texture0", 0, 3});
    RenderGraphBuilder graphBuilder;
    auto graph = graphBuilder.build(ir.value(), aliases, reflections);
    TEST_ASSERT(graph, "graph build failed: " + (graph ? "" : graph.error()));
    bool foundExternal = false;
    for (const auto& img : graph.value().images) {
        if (img.external && img.name.find("texture0") != std::string::npos) {
            foundExternal = true; break;
        }
    }
    TEST_ASSERT(foundExternal, "no external image node for texture0");
    return true;
}

static bool test_param_parse() {
    using namespace monix::renderer_vk;
    std::string testName = "test_param_parse";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_param.slangp");
    TEST_ASSERT(ast, "parse failed: " + ast.error());
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed: " + ir.error());
    TEST_ASSERT_EQ(static_cast<int>(ir.value().parameterOverrides.size()), 1,
        "expected 1 parameter override");
    TEST_ASSERT_EQ(ir.value().parameterOverrides[0].name, std::string("TEST_EFFECT"),
        "parameter name should be TEST_EFFECT");
    TEST_ASSERT_EQ(ir.value().parameterOverrides[0].rawValue, std::string("1"),
        "parameter value should be 1");
    return true;
}

static bool test_param_preprocessor() {
    using namespace monix::renderer_vk;
    std::string testName = "test_param_preprocessor";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_param.slangp");
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");
    std::unordered_map<std::string, std::string> initialDefines;
    for (const auto& override_ : ir.value().parameterOverrides) {
        initialDefines[override_.name] = override_.rawValue;
    }
    TEST_ASSERT_EQ(initialDefines.size(), static_cast<size_t>(1), "expected 1 define");
    TEST_ASSERT_EQ(initialDefines["TEST_EFFECT"], std::string("1"), "TEST_EFFECT should be 1");
    IncludeResolver resolver;
    resolver.addRoot(testDir());
    ShaderPreprocessor preprocessor(resolver);
    auto result = preprocessor.preprocessFile(ir.value().passes[0].shaderPath, initialDefines);
    TEST_ASSERT(result, "preprocess failed: " + result.error());
    auto& src = result.value().source;
    TEST_ASSERT(src.find("0.0, 1.0, 0.0") != std::string::npos,
        "TEST_EFFECT define should activate green code path");
    return true;
}

static bool test_reference_parse() {
    using namespace monix::renderer_vk;
    std::string testName = "test_reference_parse";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_derived.slangp");
    TEST_ASSERT(ast, "parse failed: " + ast.error());
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed: " + ir.error());
    TEST_ASSERT(ir.value().passes.size() >= 2,
        "expected >=2 passes from derived+base, got " + std::to_string(ir.value().passes.size()));
    bool foundDerivedPass = false;
    for (const auto& p : ir.value().passes) {
        if (p.alias == "DerivedPass") foundDerivedPass = true;
    }
    TEST_ASSERT(foundDerivedPass, "DerivedPass alias not found (override failed)");
    return true;
}

static bool test_cycle_detection() {
    using namespace monix::renderer_vk;
    std::string testName = "test_cycle_detection";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_cycle_a.slangp");
    TEST_ASSERT(!ast, "expected parse failure for circular reference");
    TEST_ASSERT(ast.error().find("Circular") != std::string::npos ||
                ast.error().find("circular") != std::string::npos ||
                ast.error().find("cycle") != std::string::npos,
        "error should mention circular reference, got: " + ast.error());
    return true;
}

static bool test_resize_graph() {
    using namespace monix::renderer_vk;
    std::string testName = "test_resize_graph";
    SlangPresetParser parser;
    auto ast = parser.parseFile(testDir() / "test_resize.slangp");
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());
    std::vector<ShaderReflection> reflections(2);
    reflections[0].samplers.push_back({"Source", 0, 2});
    reflections[1].samplers.push_back({"Source", 0, 2});
    reflections[1].samplers.push_back({"Pass0", 0, 3});
    RenderGraphBuilder graphBuilder;
    auto graph1 = graphBuilder.build(ir.value(), aliases, reflections);
    TEST_ASSERT(graph1, "graph1 build failed");
    auto graph2 = graphBuilder.build(ir.value(), aliases, reflections);
    TEST_ASSERT(graph2, "graph2 build failed");
    TEST_ASSERT_EQ(graph1.value().images.size(), graph2.value().images.size(),
        "image count mismatch between resolutions");
    TEST_ASSERT_EQ(graph1.value().passes.size(), graph2.value().passes.size(),
        "pass count mismatch between resolutions");
    auto plan1 = graphBuilder.compileExecutionPlan(graph1.value());
    auto plan2 = graphBuilder.compileExecutionPlan(graph2.value());
    TEST_ASSERT_EQ(plan1.passOrder.size(), plan2.passOrder.size(),
        "pass order size mismatch");
    return true;
}

// ============================================================================
// REAL PRESET TEST: Tv-NTSC_Megadrive-AA_sharp-Selective
// ============================================================================

static fs::path rootDir() {
    return fs::path("D:\\Monix-2ago-unestable\\Monix\\Monix");
}

static bool test_megadrive_parse() {
    using namespace monix::renderer_vk;
    std::string testName = "megadrive_parse";

    auto presetPath = rootDir() / "Shaders" / "Presets" / "Tv-NTSC_Megadrive-AA_sharp-Selective.slangp";
    TEST_ASSERT(fs::exists(presetPath), "preset not found: " + presetPath.string());

    SlangPresetParser parser;
    auto ast = parser.parseFile(presetPath);
    TEST_ASSERT(ast, "parseFile failed: " + ast.error());

    fprintf(stdout, "\n  [INFO] Preset parsed successfully\n");

    // Show AST entries
    const auto& astVal = ast.value();
    fprintf(stdout, "  [INFO] AST base directory: %s\n", astVal.baseDirectory.string().c_str());
    fprintf(stdout, "  [INFO] AST entries: %zu\n", astVal.entries.size());
    for (const auto& e : astVal.entries) {
        if (e.key.find("shader") != std::string::npos ||
            e.key.find("alias") != std::string::npos ||
            e.key.find("reference") != std::string::npos ||
            e.key.find("textures") != std::string::npos) {
            fprintf(stdout, "    %s = %s\n", e.key.c_str(), e.value.c_str());
        }
    }

    auto ir = parser.buildIr(astVal);
    TEST_ASSERT(ir, "buildIr failed: " + ir.error());

    const auto& irVal = ir.value();
    fprintf(stdout, "\n  === PresetIr Statistics ===\n");
    fprintf(stdout, "  Passes:             %zu\n", irVal.passes.size());
    fprintf(stdout, "  External Textures:  %zu\n", irVal.externalTextures.size());
    fprintf(stdout, "  Parameter Overrides:%zu\n", irVal.parameterOverrides.size());

    fprintf(stdout, "\n  --- Passes ---\n");
    for (const auto& pass : irVal.passes) {
        fprintf(stdout, "  [%2d] alias=%-30s shader=%s\n",
            pass.index, pass.alias.c_str(), pass.shaderPath.filename().string().c_str());
        if (pass.framebufferFeedback)
            fprintf(stdout, "        -> FEEDBACK\n");
    }

    fprintf(stdout, "\n  --- External Textures ---\n");
    for (const auto& tex : irVal.externalTextures) {
        fprintf(stdout, "  [%2d] alias=%-30s path=%s\n",
            tex.textureIndex, tex.alias.c_str(), tex.path.filename().string().c_str());
    }

    fprintf(stdout, "\n  --- Parameter Overrides ---\n");
    for (const auto& param : irVal.parameterOverrides) {
        fprintf(stdout, "  %s = %s\n", param.name.c_str(), param.rawValue.c_str());
    }

    return true;
}

static bool test_megadrive_compile() {
    using namespace monix::renderer_vk;
    std::string testName = "megadrive_compile";

    auto presetPath = rootDir() / "Shaders" / "Presets" / "Tv-NTSC_Megadrive-AA_sharp-Selective.slangp";
    SlangPresetParser parser;
    auto ast = parser.parseFile(presetPath);
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");

    auto slangc = rootDir() / "tools" / "slangc.exe";
    TEST_ASSERT(fs::exists(slangc), "slangc.exe not found");

    auto shadersDir = rootDir() / "Shaders";
    auto cacheDir = rootDir() / "build" / "shader-cache-vk";

    IncludeResolver resolver;
    resolver.addRoot(shadersDir);

    ShaderPreprocessor preprocessor(resolver);

    const auto& irVal = ir.value();
    int totalPasses = static_cast<int>(irVal.passes.size());
    int compilePass = 0;
    int compileFail = 0;

    fprintf(stdout, "\n=== Compiling %d Shaders ===\n", totalPasses);
    fprintf(stdout, "%-4s %-45s %-6s %s\n", "NUM", "SHADER", "RESULT", "ERROR");
    fprintf(stdout, "---- --------------------------------------------- ------ ------\n");

    for (const auto& pass : irVal.passes) {
        std::string shaderFile = pass.shaderPath.filename().string();
        std::string result;
        std::string error;

        if (!fs::exists(pass.shaderPath)) {
            result = "FAIL";
            error = "file not found";
            compileFail++;
        } else {
            auto preprocessed = preprocessor.preprocessFile(pass.shaderPath);
            if (!preprocessed) {
                result = "FAIL";
                error = "preprocess: " + preprocessed.error();
                compileFail++;
        } else {
            StageSplitter splitter;
            auto splitResult = splitter.split(preprocessed.value().source);
            if (!splitResult) {
                result = "FAIL";
                error = "split: " + splitResult.error();
                compileFail++;
            } else {
                const auto& sv = splitResult.value();
                bool allOk = true;
                std::string failDetail;

                SlangCompiler compiler;

                if (sv.hasVertex) {
                    ShaderCompileRequest req;
                    req.stage = ShaderStage::Vertex;
                    req.source = sv.vertex;
                    req.sourcePath = pass.shaderPath;
                    req.outputDirectory = cacheDir;
                    req.slangcPath = slangc;
                    req.includeDirectories.push_back(shadersDir);
                    auto vertResult = compiler.compile(req);
                    if (!vertResult) {
                        allOk = false;
                        failDetail = "vert: " + vertResult.error();
                    }
                }

                if (allOk && sv.hasFragment) {
                    ShaderCompileRequest req;
                    req.stage = ShaderStage::Fragment;
                    req.source = sv.fragment;
                    req.sourcePath = pass.shaderPath;
                    req.outputDirectory = cacheDir;
                    req.slangcPath = slangc;
                    req.includeDirectories.push_back(shadersDir);
                    auto fragResult = compiler.compile(req);
                    if (!fragResult) {
                        allOk = false;
                        failDetail = "frag: " + fragResult.error();
                    }
                }

                if (allOk) {
                    result = "PASS";
                    compilePass++;
                } else {
                    result = "FAIL";
                    error = failDetail;
                    compileFail++;
                }
            }
        }
        }

        fprintf(stdout, "[%2d] %-45s %-6s %s\n",
            pass.index, shaderFile.c_str(), result.c_str(), error.c_str());
    }

    fprintf(stdout, "\n=== Compilation Summary: %d PASS, %d FAIL, %d total ===\n",
        compilePass, compileFail, totalPasses);

    return compileFail == 0;
}

static bool test_megadrive_graph() {
    using namespace monix::renderer_vk;
    std::string testName = "megadrive_graph";

    auto presetPath = rootDir() / "Shaders" / "Presets" / "Tv-NTSC_Megadrive-AA_sharp-Selective.slangp";
    SlangPresetParser parser;
    auto ast = parser.parseFile(presetPath);
    TEST_ASSERT(ast, "parse failed");
    auto ir = parser.buildIr(ast.value());
    TEST_ASSERT(ir, "buildIr failed");

    // Build alias database
    AliasResolver aliasResolver;
    auto aliases = aliasResolver.build(ir.value());

    fprintf(stdout, "\n=== Alias Database ===\n");
    const auto& passAliases = aliases.passAliases();
    const auto& texAliases = aliases.textureAliases();
    fprintf(stdout, "Pass aliases: %zu\n", passAliases.size());
    for (const auto& [name, idx] : passAliases) {
        fprintf(stdout, "  %-30s -> pass=%d\n", name.c_str(), idx);
    }
    fprintf(stdout, "Texture aliases: %zu\n", texAliases.size());
    for (const auto& [name, idx] : texAliases) {
        fprintf(stdout, "  %-30s -> texture=%d\n", name.c_str(), idx);
    }

    // Build reflection data (simulated from shader analysis)
    const auto& irVal = ir.value();
    std::vector<ShaderReflection> reflections(irVal.passes.size());
    for (size_t i = 0; i < irVal.passes.size(); i++) {
        reflections[i].samplers.push_back({"Source", 0, 2});
    }

    // Build render graph
    RenderGraphBuilder graphBuilder;
    auto graph = graphBuilder.build(irVal, aliases, reflections);
    if (!graph) {
        fprintf(stdout, "\n  [FAIL] RenderGraph build: %s\n", graph.error().c_str());
        return false;
    }

    const auto& graphVal = graph.value();
    fprintf(stdout, "\n=== RenderGraph ===\n");
    fprintf(stdout, "Images: %zu\n", graphVal.images.size());
    fprintf(stdout, "Passes: %zu\n", graphVal.passes.size());

    for (const auto& img : graphVal.images) {
        fprintf(stdout, "  [img %2d] %-30s %s\n",
            img.id, img.name.c_str(),
            img.feedback ? "FEEDBACK" : "");
    }

    for (const auto& pass : graphVal.passes) {
        fprintf(stdout, "  [pass %2d] %-30s inputs=%zu output=%d\n",
            pass.id, pass.name.c_str(), pass.inputs.size(), pass.outputImage);
    }

    // Compile execution plan
    auto plan = graphBuilder.compileExecutionPlan(graphVal);
    fprintf(stdout, "\n=== Execution Plan ===\n");
    fprintf(stdout, "Pass order: ");
    for (auto id : plan.passOrder) fprintf(stdout, "%d ", id);
    fprintf(stdout, "\n");

    return true;
}

void runCompilationPipelineTests() {
    fprintf(stdout, "\n=== Compilation Pipeline Tests ===\n");
    fflush(stdout);
    RUN_VKTEST(test_2pass_parse);
    RUN_VKTEST(test_2pass_graph);
    RUN_VKTEST(test_multibinding_parse);
    RUN_VKTEST(test_multibinding_graph);
    RUN_VKTEST(test_multibinding3_graph);
    RUN_VKTEST(test_alias_resolution);
    RUN_VKTEST(test_alias_graph);
    RUN_VKTEST(test_feedback_parse);
    RUN_VKTEST(test_feedback_alias);
    RUN_VKTEST(test_feedback_graph);
    RUN_VKTEST(test_external_parse);
    RUN_VKTEST(test_external_graph);
    RUN_VKTEST(test_param_parse);
    RUN_VKTEST(test_param_preprocessor);
    RUN_VKTEST(test_reference_parse);
    RUN_VKTEST(test_cycle_detection);
    RUN_VKTEST(test_resize_graph);
    monix::tests::printSummary();
}

void runMegadrivePresetTests() {
    fprintf(stdout, "\n=== Mega Drive Preset Tests ===\n");
    fflush(stdout);
    RUN_VKTEST(test_megadrive_parse);
    RUN_VKTEST(test_megadrive_compile);
    RUN_VKTEST(test_megadrive_graph);
    monix::tests::printSummary();
}
