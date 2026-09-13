#include "GlslAdapter.hpp"

#include "../../compiler/SlangCompiler.hpp"
#include "../../compiler/ShaderCache.hpp"
#include "../../core/FileSystem.hpp"
#include "../../preset/ShaderPreprocessor.hpp"
#include "../../preset/StageSplitter.hpp"
#include "../../preset/IncludeResolver.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace monix::renderer_vk {
namespace {

std::string stripPragmaParameters(const std::string& source) {
    std::istringstream input(source);
    std::ostringstream output;
    std::string line;
    while (std::getline(input, line)) {
        auto trimmed = line;
        auto pos = trimmed.find_first_not_of(" \t\r");
        if (pos != std::string::npos) trimmed = trimmed.substr(pos);
        if (trimmed.rfind("#pragma parameter", 0) == 0) continue;
        if (trimmed.rfind("#pragma", 0) == 0 &&
            trimmed.find("stage") == std::string::npos) continue;
        output << line << "\n";
    }
    return output.str();
}

std::string stripParameterUniformBlock(const std::string& source) {
    std::istringstream input(source);
    std::ostringstream output;
    std::string line;
    int depth = 0;
    bool skipping = false;
    while (std::getline(input, line)) {
        auto trimmed = line;
        auto pos = trimmed.find_first_not_of(" \t\r");
        if (pos != std::string::npos) trimmed = trimmed.substr(pos);
        if (trimmed.find("#ifdef PARAMETER_UNIFORM") != std::string::npos ||
            trimmed.find("#if defined(PARAMETER_UNIFORM)") != std::string::npos) {
            skipping = true;
            depth = 1;
            continue;
        }
        if (skipping) {
            if (trimmed.find("#if") == 0 && trimmed.find("#ifdef") == 0) ++depth;
            else if (trimmed.find("#if ") == 0) ++depth;
            if (trimmed.find("#endif") != std::string::npos) {
                --depth;
                if (depth == 0) { skipping = false; }
            }
            continue;
        }
        output << line << "\n";
    }
    return output.str();
}

}  // namespace

bool GlslAdapter::canCompile(ShaderLanguage lang) const {
    return lang == ShaderLanguage::GLSL;
}

bool GlslAdapter::isAvailable() const {
    return true;
}

std::string GlslAdapter::unavailabilityReason() const {
    return "";
}

Result<ShaderModule> GlslAdapter::compile(const ShaderRuntimeCompileRequest& request) const {
    ShaderModule module;
    module.language = ShaderLanguage::GLSL;
    module.sourcePath = request.sourcePath.string();

    if (!request.compilerPath.empty() && !std::filesystem::exists(request.compilerPath)) {
        module.diagnostics.add(ShaderDiagnostic{
            ShaderDiagnosticKind::CompileError,
            DiagnosticSeverity::Error,
            request.compilerPath.string(),
            0, 0, "", "COMPILER_NOT_FOUND",
            "GLSL compiler not found at: " + request.compilerPath.string()
        });
        module.compiled = false;
        return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
    }

    IncludeResolver resolver;
    for (const auto& root : request.includeDirectories) {
        resolver.addRoot(root);
    }
    resolver.addRoot(request.sourcePath.parent_path());

    std::filesystem::path sourceFile = request.sourcePath;
    if (!request.source.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(request.outputDirectory, ec);
        sourceFile = request.outputDirectory / (request.sourcePath.stem().string() + ".input.glsl");
        std::ofstream ofs(sourceFile);
        if (ofs.is_open()) {
            ofs << request.source;
            ofs.close();
        }
    }

    ShaderPreprocessor preprocessor(resolver);

    auto rawText = FileSystem::readText(sourceFile);
    if (!rawText) {
        module.diagnostics.add(ShaderDiagnostic{
            ShaderDiagnosticKind::PreprocessError,
            DiagnosticSeverity::Error,
            request.sourcePath.string(),
            0, 0, "", "READ_FAILED",
            rawText.status().message
        });
        module.compiled = false;
        return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
    }

    auto retroSplit = StageSplitter::splitRetroArchRaw(rawText.value());
    bool isRetroArchStyle = retroSplit.isRetroArchStyle && !retroSplit.vertex.empty() && !retroSplit.fragment.empty();

    SlangCompiler compiler;
    compiler.setCache(cache_);

    if (isRetroArchStyle) {
        auto cleaned = stripPragmaParameters(rawText.value());

        const char* vertShader =
            "#version 450\n"
            "layout(location=0) in vec2 aPos;\n"
            "layout(location=1) in vec2 aUV;\n"
            "layout(location=0) out vec2 vTexCoord;\n"
            "layout(set=0,binding=0) uniform UBO { mat4 mvp; } ubo;\n"
            "void main() {\n"
            "    gl_Position = ubo.mvp * vec4(aPos, 0.0, 1.0);\n"
            "    vTexCoord = aUV;\n"
            "}\n";

        std::string fragDefines = "#define FRAGMENT 1\n#define __VERSION__ 450\n#define GL_FRAGMENT_PRECISION_HIGH 1\n#define PARAMETER_UNIFORM 1\n";

        {
            ShaderCompileRequest vertReq;
            vertReq.stage = ShaderStage::Vertex;
            vertReq.language = ShaderLanguage::GLSL;
            vertReq.source = std::string(vertShader);
            vertReq.sourcePath = request.sourcePath;
            vertReq.outputDirectory = request.outputDirectory;
            vertReq.slangcPath = request.compilerPath;
            vertReq.includeDirectories = request.includeDirectories;
            vertReq.dependencies = {};
            vertReq.debugInfo = request.debugInfo;

            auto vertResult = compiler.compile(vertReq);
            if (!vertResult) {
                module.diagnostics.add(ShaderDiagnostic{
                    ShaderDiagnosticKind::CompileError,
                    DiagnosticSeverity::Error,
                    request.sourcePath.string(),
                    0, 0, "vertex", "GLSL_COMPILE_FAILED",
                    vertResult.status().message
                });
                module.compiled = false;
                return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
            }

            ShaderModuleStage sms;
            sms.stage = ShaderStage::Vertex;
            sms.source = vertReq.source;
            sms.spirv = std::move(vertResult.value().spirv);
            sms.reflection = std::move(vertResult.value().reflection);
            module.cacheHit = module.cacheHit || vertResult.value().cacheHit;
            module.stages.push_back(std::move(sms));
        }

        {
            ShaderCompileRequest fragReq;
            fragReq.stage = ShaderStage::Fragment;
            fragReq.language = ShaderLanguage::GLSL;
            fragReq.source = fragDefines + cleaned;
            fragReq.sourcePath = request.sourcePath;
            fragReq.outputDirectory = request.outputDirectory;
            fragReq.slangcPath = request.compilerPath;
            fragReq.includeDirectories = request.includeDirectories;
            fragReq.dependencies = {};
            fragReq.debugInfo = request.debugInfo;

            auto fragResult = compiler.compile(fragReq);
            if (!fragResult) {
                module.diagnostics.add(ShaderDiagnostic{
                    ShaderDiagnosticKind::CompileError,
                    DiagnosticSeverity::Error,
                    request.sourcePath.string(),
                    0, 0, "fragment", "GLSL_COMPILE_FAILED",
                    fragResult.status().message
                });
                module.compiled = false;
                return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
            }

            ShaderModuleStage sms;
            sms.stage = ShaderStage::Fragment;
            sms.source = fragReq.source;
            sms.spirv = std::move(fragResult.value().spirv);
            sms.reflection = std::move(fragResult.value().reflection);
            module.cacheHit = module.cacheHit || fragResult.value().cacheHit;
            module.stages.push_back(std::move(sms));
        }
    } else {
        auto preprocessed = preprocessor.preprocessFile(sourceFile, {});
        if (!preprocessed) {
            module.diagnostics.add(ShaderDiagnostic{
                ShaderDiagnosticKind::PreprocessError,
                DiagnosticSeverity::Error,
                request.sourcePath.string(),
                0, 0, "", "PREPROCESS_FAILED",
                preprocessed.status().message
            });
            module.compiled = false;
            return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
        }

        for (const auto& diag : preprocessed.value().diagnostics.entries()) {
            ShaderDiagnostic sd;
            sd.kind = ShaderDiagnosticKind::PreprocessError;
            sd.severity = diag.severity;
            sd.file = diag.file.string();
            sd.line = diag.line;
            sd.message = diag.message;
            module.diagnostics.add(std::move(sd));
        }

        StageSplitter splitter;
        auto stages = splitter.split(preprocessed.value().source);
        if (!stages) {
            module.diagnostics.add(ShaderDiagnostic{
                ShaderDiagnosticKind::PreprocessError,
                DiagnosticSeverity::Error,
                request.sourcePath.string(),
                0, 0, "", "STAGE_SPLIT_FAILED",
                stages.status().message
            });
            module.compiled = false;
            return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
        }

        {
            ShaderCompileRequest vertReq;
            vertReq.stage = ShaderStage::Vertex;
            vertReq.language = ShaderLanguage::GLSL;
            vertReq.source = stages.value().vertex;
            vertReq.sourcePath = request.sourcePath;
            vertReq.outputDirectory = request.outputDirectory;
            vertReq.slangcPath = request.compilerPath;
            vertReq.includeDirectories = request.includeDirectories;
            vertReq.dependencies = preprocessed.value().dependencies;
            vertReq.debugInfo = request.debugInfo;

            auto vertResult = compiler.compile(vertReq);
            if (!vertResult) {
                module.diagnostics.add(ShaderDiagnostic{
                    ShaderDiagnosticKind::CompileError,
                    DiagnosticSeverity::Error,
                    request.sourcePath.string(),
                    0, 0, "vertex", "GLSL_COMPILE_FAILED",
                    vertResult.status().message
                });
                module.compiled = false;
                return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
            }

            ShaderModuleStage sms;
            sms.stage = ShaderStage::Vertex;
            sms.source = stages.value().vertex;
            sms.spirv = std::move(vertResult.value().spirv);
            sms.reflection = std::move(vertResult.value().reflection);
            module.cacheHit = module.cacheHit || vertResult.value().cacheHit;
            module.stages.push_back(std::move(sms));
        }

        {
            ShaderCompileRequest fragReq;
            fragReq.stage = ShaderStage::Fragment;
            fragReq.language = ShaderLanguage::GLSL;
            fragReq.source = stages.value().fragment;
            fragReq.sourcePath = request.sourcePath;
            fragReq.outputDirectory = request.outputDirectory;
            fragReq.slangcPath = request.compilerPath;
            fragReq.includeDirectories = request.includeDirectories;
            fragReq.dependencies = preprocessed.value().dependencies;
            fragReq.debugInfo = request.debugInfo;

            auto fragResult = compiler.compile(fragReq);
            if (!fragResult) {
                module.diagnostics.add(ShaderDiagnostic{
                    ShaderDiagnosticKind::CompileError,
                    DiagnosticSeverity::Error,
                    request.sourcePath.string(),
                    0, 0, "fragment", "GLSL_COMPILE_FAILED",
                    fragResult.status().message
                });
                module.compiled = false;
                return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
            }

            ShaderModuleStage sms;
            sms.stage = ShaderStage::Fragment;
            sms.source = stages.value().fragment;
            sms.spirv = std::move(fragResult.value().spirv);
            sms.reflection = std::move(fragResult.value().reflection);
            module.cacheHit = module.cacheHit || fragResult.value().cacheHit;
            module.stages.push_back(std::move(sms));
        }
    }

    if (!module.stages.empty()) {
        module.reflection = module.stages.back().reflection;
    }

    module.compiled = true;
    return Result<ShaderModule>(std::move(module));
}

}  // namespace monix::renderer_vk
