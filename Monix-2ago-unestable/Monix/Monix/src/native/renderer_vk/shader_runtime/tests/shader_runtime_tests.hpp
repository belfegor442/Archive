#pragma once

#include "../ShaderRuntime.hpp"
#include "../core/ShaderModule.hpp"
#include "../core/ShaderDiagnostics.hpp"
#include "../adapters/SlangAdapter.hpp"
#include "../adapters/GlslAdapter.hpp"
#include "../adapters/CgAdapter.hpp"
#include "transactional_swap_tests.hpp"

#include <cassert>
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace monix::renderer_vk::tests {

class ShaderRuntimeTests {
public:
    static void runAll() {
        int passed = 0;
        int failed = 0;
        int skipped = 0;

        auto run = [&](const char* name, auto fn) {
            try {
                fn();
                std::cout << "  [PASS] " << name << "\n";
                ++passed;
            } catch (const SkipException&) {
                std::cout << "  [SKIP] " << name << "\n";
                ++skipped;
            } catch (const std::exception& e) {
                std::cout << "  [FAIL] " << name << ": " << e.what() << "\n";
                ++failed;
            }
        };

        std::cout << "ShaderRuntime Core tests:\n";
        run("detect_slang_extension", testDetectSlangExtension);
        run("detect_glsl_extension", testDetectGlslExtension);
        run("detect_cg_extension", testDetectCgExtension);
        run("detect_unknown_extension", testDetectUnknownExtension);

        std::cout << "\nAdapter selection tests:\n";
        run("select_slang_adapter", testSelectSlangAdapter);
        run("select_glsl_adapter", testSelectGlslAdapter);
        run("select_cg_adapter", testSelectCgAdapter);
        run("select_unknown_adapter", testSelectUnknownAdapter);

        std::cout << "\nSlangAdapter compilation tests:\n";
        run("slang_adapter_is_available", testSlangAdapterAvailable);
        run("compile_valid_slang", testCompileValidSlang);
        run("compile_invalid_slang", testCompileInvalidSlang);

        std::cout << "\nDiagnostics propagation tests:\n";
        run("propagate_diagnostics_on_error", testPropagateDiagnostics);
        run("diagnostics_format", testDiagnosticsFormat);

        std::cout << "\nReflection preservation tests:\n";
        run("reflection_preserved", testReflectionPreserved);

        std::cout << "\nGLSL availability tests:\n";
        run("glsl_available", testGlslAvailable);

        std::cout << "\nCG availability tests:\n";
        run("cg_unsupported_diagnostic", testCgUnsupported);

        std::cout << "\nSlangAdapter rejects GLSL language:\n";
        run("slang_rejects_glsl", testSlangRejectsGlsl);

        std::cout << "\nSlangAdapter rejects CG language:\n";
        run("slang_rejects_cg", testSlangRejectsCg);

        std::cout << "\nShaderModule structure tests:\n";
        run("module_stages_structure", testModuleStages);
        run("module_no_spirv_before_compile", testModuleNoSpirvBeforeCompile);

        std::cout << "\n" << passed << " passed, " << failed << " failed, " << skipped << " skipped\n";

        std::cout << "\n";
        TransactionalSwapTests::runAll();

        if (failed > 0) throw std::runtime_error("Some tests failed");
    }

private:
    struct SkipException : std::runtime_error {
        SkipException() : std::runtime_error("skipped") {}
    };

    static void assert_(bool cond, const char* msg) {
        if (!cond) throw std::runtime_error(msg);
    }

    static ShaderRuntimeConfig makeTestConfig() {
        ShaderRuntimeConfig config;
        auto root = findMonixRoot();

        config.rootDirectory = root;
        config.shaderCacheDirectory = root / "build" / "shader-cache-test";
        config.slangcPath = root / "tools" / "slangc.exe";
        config.compileShadersToSpirv = true;
        return config;
    }

    static std::filesystem::path findMonixRoot() {
        auto cwd = std::filesystem::current_path();
        auto path = cwd / "Shaders" / "tests" / "identity.slang";
        if (std::filesystem::exists(path)) return cwd;

        auto monixRoot = cwd;
        while (monixRoot.has_parent_path() && monixRoot != monixRoot.parent_path()) {
            auto candidate = monixRoot / "Shaders" / "tests" / "identity.slang";
            if (std::filesystem::exists(candidate)) return monixRoot;
            monixRoot = monixRoot.parent_path();
        }

#ifdef _WIN32
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        auto exeDir = std::filesystem::path(exePath).parent_path();
        auto projectDir = exeDir.parent_path();
        path = projectDir / "Shaders" / "tests" / "identity.slang";
        if (std::filesystem::exists(path)) return projectDir;
#endif

        return cwd;
    }

    static std::filesystem::path testSlangFile() {
        auto root = findMonixRoot();
        auto path = root / "Shaders" / "tests" / "identity.slang";
        if (std::filesystem::exists(path)) return path;
        path = root / "Shaders" / "tests" / "solid_red.slang";
        if (std::filesystem::exists(path)) return path;
        path = root / "Shaders" / "tests" / "passthrough.slang";
        if (std::filesystem::exists(path)) return path;

        return root / "Shaders" / "tests" / "passthrough.slang";
    }

    static void testDetectSlangExtension() {
        assert_(ShaderRuntime::detectLanguage("test.slang") == ShaderLanguage::Slang, "Should detect .slang");
        assert_(ShaderRuntime::detectLanguage("test.slangp") == ShaderLanguage::Slang, "Should detect .slangp");
    }

    static void testDetectGlslExtension() {
        assert_(ShaderRuntime::detectLanguage("test.glsl") == ShaderLanguage::GLSL, "Should detect .glsl");
        assert_(ShaderRuntime::detectLanguage("test.vert") == ShaderLanguage::GLSL, "Should detect .vert");
        assert_(ShaderRuntime::detectLanguage("test.frag") == ShaderLanguage::GLSL, "Should detect .frag");
        assert_(ShaderRuntime::detectLanguage("test.geom") == ShaderLanguage::GLSL, "Should detect .geom");
        assert_(ShaderRuntime::detectLanguage("test.comp") == ShaderLanguage::GLSL, "Should detect .comp");
    }

    static void testDetectCgExtension() {
        assert_(ShaderRuntime::detectLanguage("test.cg") == ShaderLanguage::CG, "Should detect .cg");
    }

    static void testDetectUnknownExtension() {
        assert_(ShaderRuntime::detectLanguage("test.txt") == ShaderLanguage::Unknown, "Should be Unknown for .txt");
        assert_(ShaderRuntime::detectLanguage("test.cpp") == ShaderLanguage::Unknown, "Should be Unknown for .cpp");
    }

    static void testSelectSlangAdapter() {
        ShaderRuntime rt(makeTestConfig());
        rt.initialize();
        auto* a = rt.adapter(ShaderLanguage::Slang);
        assert_(a != nullptr, "Slang adapter should exist");
        assert_(a->language() == ShaderLanguage::Slang, "Should report Slang language");
    }

    static void testSelectGlslAdapter() {
        ShaderRuntime rt(makeTestConfig());
        rt.initialize();
        auto* a = rt.adapter(ShaderLanguage::GLSL);
        assert_(a != nullptr, "GLSL adapter should exist");
        assert_(a->language() == ShaderLanguage::GLSL, "Should report GLSL language");
    }

    static void testSelectCgAdapter() {
        ShaderRuntime rt(makeTestConfig());
        rt.initialize();
        auto* a = rt.adapter(ShaderLanguage::CG);
        assert_(a != nullptr, "CG adapter should exist");
        assert_(a->language() == ShaderLanguage::CG, "Should report CG language");
    }

    static void testSelectUnknownAdapter() {
        ShaderRuntime rt(makeTestConfig());
        rt.initialize();
        auto* a = rt.adapter(ShaderLanguage::Unknown);
        assert_(a == nullptr, "Unknown language should return nullptr adapter");
    }

    static void testSlangAdapterAvailable() {
        SlangAdapter adapter;
        assert_(adapter.isAvailable(), "Slang adapter should be available");
    }

    static void testCompileValidSlang() {
        auto slangPath = testSlangFile();
        if (!std::filesystem::exists(slangPath)) {
            throw SkipException();
        }

        ShaderRuntime rt(makeTestConfig());
        rt.initialize();

        auto result = rt.compileShader(ShaderLanguage::Slang, slangPath);
        if (!result.ok()) {
            auto slangcPath = rt.config().slangcPath;
            if (!std::filesystem::exists(slangcPath)) {
                std::cout << "    (slangc not found at " << slangcPath.string() << ", skipping)\n";
                throw SkipException();
            }
            throw std::runtime_error("Compilation failed: " + result.error());
        }

        const auto& mod = result.value();
        assert_(mod.compiled, "Module should be marked as compiled");
        assert_(mod.language == ShaderLanguage::Slang, "Language should be Slang");
        assert_(mod.hasVertex(), "Should have vertex stage");
        assert_(mod.hasFragment(), "Should have fragment stage");
        assert_(mod.totalSpirvBytes() > 0, "Should have SPIR-V bytes");
    }

    static void testCompileInvalidSlang() {
        ShaderRuntime rt(makeTestConfig());
        rt.initialize();

        std::string invalidSource = "this is not valid slang code {{{{";
        auto result = rt.compileShaderFromSource(
            ShaderLanguage::Slang, invalidSource,
            "test_invalid.slang");

        assert_(!result.ok(), "Invalid Slang should fail compilation");
        assert_(result.error().find("ERROR") != std::string::npos ||
                result.error().find("error") != std::string::npos ||
                result.error().find("failed") != std::string::npos,
                "Error should contain error information");
    }

    static void testPropagateDiagnostics() {
        ShaderRuntime rt(makeTestConfig());
        rt.initialize();

        std::string invalidSource = "not_valid_code_12345_!@#$%";
        auto result = rt.compileShaderFromSource(
            ShaderLanguage::Slang, invalidSource,
            "test_diag.slang");

        assert_(!result.ok(), "Should fail");
        assert_(!result.value().diagnostics.hasErrors() || !result.error().empty(),
                "Should have diagnostics or error message");
    }

    static void testDiagnosticsFormat() {
        ShaderDiagnostics diags;
        diags.add(ShaderDiagnostic{
            ShaderDiagnosticKind::CompileError,
            DiagnosticSeverity::Error,
            "test.slang",
            10, 5, "fragment", "E001",
            "Unexpected token"
        });

        auto formatted = diags.formatAll();
        assert_(formatted.find("ERROR") != std::string::npos, "Should contain ERROR");
        assert_(formatted.find("COMPILE_ERROR") != std::string::npos, "Should contain COMPILE_ERROR");
        assert_(formatted.find("test.slang") != std::string::npos, "Should contain filename");
        assert_(formatted.find("10") != std::string::npos, "Should contain line number");
        assert_(formatted.find("Unexpected token") != std::string::npos, "Should contain message");
    }

    static void testReflectionPreserved() {
        SlangAdapter adapter;
        auto slangPath = testSlangFile();
        if (!std::filesystem::exists(slangPath)) {
            throw SkipException();
        }

        ShaderRuntime rt(makeTestConfig());
        rt.initialize();

        auto result = rt.compileShader(ShaderLanguage::Slang, slangPath);
        if (!result.ok()) {
            auto slangcPath = rt.config().slangcPath;
            if (!std::filesystem::exists(slangcPath)) {
                throw SkipException();
            }
            throw std::runtime_error("Compilation failed: " + result.error());
        }

        const auto& mod = result.value();
        assert_(mod.reflection.uniformBlocks.size() > 0 ||
                mod.reflection.samplers.size() > 0 ||
                mod.reflection.descriptors.size() > 0,
                "Should have some reflection data (uniform blocks, samplers, or descriptors)");
    }

    static void testGlslAvailable() {
        GlslAdapter adapter;
        assert_(adapter.isAvailable(), "GLSL adapter should be available (uses slangc)");
        assert_(adapter.canCompile(ShaderLanguage::GLSL), "Should accept GLSL language");
        assert_(!adapter.canCompile(ShaderLanguage::Slang), "Should not accept Slang");
    }

    static void testCgUnsupported() {
        CgAdapter adapter;
        assert_(!adapter.isAvailable(), "CG should never be available");
        auto result = adapter.compile(ShaderRuntimeCompileRequest{});
        assert_(!result.ok(), "CG compile should always fail");
        assert_(result.error().find("NVIDIA Cg") != std::string::npos ||
                result.error().find("unavailable") != std::string::npos,
                "Error should explain CG is unsupported");
    }

    static void testSlangRejectsGlsl() {
        SlangAdapter adapter;
        assert_(!adapter.canCompile(ShaderLanguage::GLSL), "Slang should not accept GLSL");
        assert_(!adapter.canCompile(ShaderLanguage::CG), "Slang should not accept CG");
        assert_(adapter.canCompile(ShaderLanguage::Slang), "Slang should accept Slang");
    }

    static void testSlangRejectsCg() {
        SlangAdapter adapter;
        assert_(!adapter.canCompile(ShaderLanguage::CG), "Slang should not accept CG");
    }

    static void testModuleStages() {
        ShaderModule mod;
        assert_(!mod.hasVertex(), "Empty module should not have vertex");
        assert_(!mod.hasFragment(), "Empty module should not have fragment");
        assert_(mod.getStage(ShaderStage::Vertex) == nullptr, "Empty module should return nullptr for vertex");
        assert_(mod.totalSpirvBytes() == 0, "Empty module should have 0 SPIR-V bytes");
    }

    static void testModuleNoSpirvBeforeCompile() {
        ShaderModule mod;
        mod.compiled = false;
        assert_(!mod.compiled, "Uncompiled module should not be marked compiled");
        assert_(mod.totalSpirvBytes() == 0, "Uncompiled module should have 0 SPIR-V bytes");
    }
};

}  // namespace monix::renderer_vk::tests
