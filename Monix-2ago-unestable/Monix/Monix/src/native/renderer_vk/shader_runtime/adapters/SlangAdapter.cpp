#include "SlangAdapter.hpp"

#include "../../compiler/SlangCompiler.hpp"
#include "../../compiler/ShaderCache.hpp"
#include "../../preset/ShaderPreprocessor.hpp"
#include "../../preset/StageSplitter.hpp"
#include "../../preset/IncludeResolver.hpp"

#include <filesystem>
#include <fstream>

namespace monix::renderer_vk {

bool SlangAdapter::canCompile(ShaderLanguage lang) const {
    return lang == ShaderLanguage::Slang;
}

bool SlangAdapter::isAvailable() const {
    return true;
}

std::string SlangAdapter::unavailabilityReason() const {
    return "";
}

Result<ShaderModule> SlangAdapter::compile(const ShaderRuntimeCompileRequest& request) const {
    ShaderModule module;
    module.language = ShaderLanguage::Slang;
    module.sourcePath = request.sourcePath.string();

    if (!request.compilerPath.empty() && !std::filesystem::exists(request.compilerPath)) {
        module.diagnostics.add(ShaderDiagnostic{
            ShaderDiagnosticKind::CompileError,
            DiagnosticSeverity::Error,
            request.compilerPath.string(),
            0, 0, "", "COMPILER_NOT_FOUND",
            "Slang compiler not found at: " + request.compilerPath.string()
        });
        module.compiled = false;
        return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
    }

    IncludeResolver resolver;
    for (const auto& root : request.includeDirectories) {
        resolver.addRoot(root);
    }

    ShaderPreprocessor preprocessor(resolver);
    auto preprocessed = preprocessor.preprocessFile(request.sourcePath, {});
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

    SlangCompiler compiler;
    compiler.setCache(cache_);

    {
        ShaderCompileRequest vertReq;
        vertReq.stage = ShaderStage::Vertex;
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
                0, 0, "vertex", "SLANG_COMPILE_FAILED",
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
                0, 0, "fragment", "SLANG_COMPILE_FAILED",
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

    if (!module.stages.empty()) {
        module.reflection = module.stages.back().reflection;
    }

    module.compiled = true;
    return Result<ShaderModule>(std::move(module));
}

}  // namespace monix::renderer_vk
