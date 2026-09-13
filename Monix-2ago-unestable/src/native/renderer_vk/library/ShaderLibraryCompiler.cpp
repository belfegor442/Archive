#include "ShaderLibraryCompiler.hpp"

#include "../core/Hash.hpp"
#include "../core/FileSystem.hpp"
#include "../preset/SlangPresetParser.hpp"
#include "../preset/PresetValidator.hpp"
#include "../preset/IncludeResolver.hpp"
#include "../public/ShaderRenderer.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>

namespace monix::renderer_vk {

ShaderLibraryCompiler::ShaderLibraryCompiler(ShaderLibrary& library, ShaderRuntime& runtime)
    : library_(library)
    , runtime_(&runtime)
{
}

ShaderLibraryCompiler::ShaderLibraryCompiler(ShaderLibrary& library, ShaderRuntime& runtime, ShaderRenderer* renderer)
    : library_(library)
    , runtime_(&runtime)
    , renderer_(renderer)
{
}

ShaderLibraryCompiler ShaderLibraryCompiler::forTests(ShaderLibrary& library) {
    static ShaderRuntimeConfig testConfig;
    static bool configured = false;
    static std::unique_ptr<ShaderRuntime> testRuntime;

    if (!configured) {
        auto cwd = std::filesystem::current_path();
        testConfig.rootDirectory = cwd;
        testConfig.shaderCacheDirectory = cwd / "build" / "shader-cache-test";
        testConfig.slangcPath = cwd / "tools" / "slangc.exe";
        testConfig.compileShadersToSpirv = true;

        testRuntime = std::make_unique<ShaderRuntime>(testConfig);
        testRuntime->initialize();
        configured = true;
    }

    return ShaderLibraryCompiler(library, *testRuntime);
}

CompileEntryResult ShaderLibraryCompiler::compileEntry(size_t index) {
    auto* e = library_.entryMutable(index);
    if (!e) {
        CompileEntryResult r;
        r.errorMessage = "Invalid index: " + std::to_string(index);
        return r;
    }

    if (e->language == ShaderLanguage::Unknown) {
        e->status = ShaderEntryStatus::Error;
        e->error = "Unsupported language";
        e->diagnostics = ShaderDiagnostics::makeError(
            ShaderDiagnosticKind::UnsupportedLanguage,
            e->path.string(),
            0,
            e->error);
        CompileEntryResult r;
        r.success = false;
        r.diagnostics = e->diagnostics;
        r.errorMessage = e->error;
        return r;
    }

    if (!std::filesystem::exists(e->path)) {
        e->status = ShaderEntryStatus::Error;
        e->error = "File not found: " + e->path.string();
        e->diagnostics = ShaderDiagnostics::makeError(
            ShaderDiagnosticKind::LoadError,
            e->path.string(),
            0,
            e->error);
        CompileEntryResult r;
        r.success = false;
        r.diagnostics = e->diagnostics;
        r.errorMessage = e->error;
        return r;
    }

    e->status = ShaderEntryStatus::Compiling;
    e->error.clear();
    e->diagnostics.clear();

    CompileEntryResult result;
    if (e->extension == ".slangp" || e->extension == ".glslp") {
        result = compilePreset(*e);
    } else {
        result = compileSlangOrGlsl(*e);
    }

    if (result.success) {
        e->status = ShaderEntryStatus::Compiled;
        e->compiledModule = std::move(result.module);
        e->diagnostics = result.diagnostics;
    } else {
        e->status = ShaderEntryStatus::Error;
        e->error = result.errorMessage;
        e->diagnostics = result.diagnostics;
    }

    return result;
}

CompileEntryResult ShaderLibraryCompiler::compileEntryByPath(const std::filesystem::path& path) {
    for (size_t i = 0; i < library_.entryCount(); ++i) {
        if (library_.entry(i)->path == path) {
            return compileEntry(i);
        }
    }
    CompileEntryResult r;
    r.errorMessage = "Path not in library: " + path.string();
    return r;
}

CompileEntryResult ShaderLibraryCompiler::compileSlangOrGlsl(ShaderLibraryEntry& entry) {
    CompileEntryResult result;

    if (!runtime_) {
        result.errorMessage = "ShaderRuntime not available";
        result.diagnostics = ShaderDiagnostics::makeError(
            ShaderDiagnosticKind::UnsupportedLanguage,
            entry.path.string(),
            0,
            result.errorMessage);
        return result;
    }

    auto compileResult = runtime_->compileShader(
        entry.language,
        entry.path,
        "");

    if (!compileResult) {
        result.success = false;
        result.errorMessage = compileResult.error();
        result.diagnostics = ShaderDiagnostics::makeError(
            ShaderDiagnosticKind::CompileError,
            entry.path.string(),
            0,
            result.errorMessage);
        return result;
    }

    result.success = true;
    result.module = std::move(compileResult.value());
    if (result.module.diagnostics.hasErrors()) {
        result.diagnostics = result.module.diagnostics;
    }
    return result;
}

CompileEntryResult ShaderLibraryCompiler::compilePreset(ShaderLibraryEntry& entry) {
    CompileEntryResult result;

    SlangPresetParser parser;
    auto astResult = parser.parseFile(entry.path);
    if (!astResult) {
        result.success = false;
        result.errorMessage = astResult.error();
        result.diagnostics = ShaderDiagnostics::makeError(
            ShaderDiagnosticKind::PreprocessError,
            entry.path.string(),
            0,
            result.errorMessage);
        return result;
    }

    auto irResult = parser.buildIr(astResult.value());
    if (!irResult) {
        result.success = false;
        result.errorMessage = irResult.error();
        result.diagnostics = ShaderDiagnostics::makeError(
            ShaderDiagnosticKind::PreprocessError,
            entry.path.string(),
            0,
            result.errorMessage);
        return result;
    }

    IncludeResolver resolver;
    resolver.addRoot(irResult.value().baseDirectory);
    resolver.addRoot(entry.path.parent_path());
    resolver.addRoot(entry.path.parent_path().parent_path());
    for (auto& pass : irResult.value().passes) {
        if (!std::filesystem::exists(pass.shaderPath)) {
            auto resolved = resolver.resolve({}, pass.shaderPath.filename().string());
            if (resolved) {
                pass.shaderPath = resolved.value();
            }
        }
    }

    PresetValidator validator;
    auto validation = validator.validate(irResult.value());
    if (validation.hasErrors()) {
        result.success = false;
        result.errorMessage = validation.summary();
        result.diagnostics = ShaderDiagnostics::makeError(
            ShaderDiagnosticKind::CompileError,
            entry.path.string(),
            0,
            result.errorMessage);
        return result;
    }

    if (!runtime_) {
        result.errorMessage = "ShaderRuntime not available for preset compilation";
        result.diagnostics = ShaderDiagnostics::makeError(
            ShaderDiagnosticKind::UnsupportedLanguage,
            entry.path.string(),
            0,
            result.errorMessage);
        return result;
    }

    result.success = true;
    result.module.language = ShaderLanguage::Slang;
    result.module.sourcePath = entry.path.string();
    result.module.compiled = true;
    return result;
}

CompileAndActivateResult ShaderLibraryCompiler::compileAndActivate(
    size_t index,
    TransactionalShaderState& txState)
{
    CompileAndActivateResult result;

    auto* e = library_.entryMutable(index);
    if (!e) {
        result.rejectionReason = "Invalid index: " + std::to_string(index);
        return result;
    }

    txState.beginCompile();

    auto compileResult = compileEntry(index);
    if (!compileResult.success) {
        txState.rollback();
        result.rejectionReason = compileResult.errorMessage;
        result.diagnostics = compileResult.diagnostics;
        return result;
    }

    auto* e2 = library_.entryMutable(index);
    if (!e2) {
        txState.rollback();
        result.rejectionReason = "Entry disappeared during compilation";
        return result;
    }

    if (renderer_) {
        if (e2->extension == ".slangp") {
            auto loadResult = renderer_->loadPreset(e2->path);
            if (!loadResult) {
                e2->status = ShaderEntryStatus::Error;
                e2->error = loadResult.message;
                e2->diagnostics = ShaderDiagnostics::makeError(
                    ShaderDiagnosticKind::CompileError,
                    e2->path.string(),
                    0,
                    e2->error);
                txState.rollback();
                result.rejectionReason = e2->error;
                result.diagnostics = e2->diagnostics;
                return result;
            }
        } else {
            auto loadResult = renderer_->loadIndividualShader(e2->compiledModule, e2->path);
            if (!loadResult) {
                e2->status = ShaderEntryStatus::Error;
                e2->error = loadResult.message;
                e2->diagnostics = ShaderDiagnostics::makeError(
                    ShaderDiagnosticKind::CompileError,
                    e2->path.string(),
                    0,
                    e2->error);
                txState.rollback();
                result.rejectionReason = e2->error;
                result.diagnostics = e2->diagnostics;
                return result;
            }
        }
    }

    e2->status = ShaderEntryStatus::Active;
    e2->isActive = true;
    library_.clearActiveExcept(index);
    txState.commit();
    result.committed = true;
    result.diagnostics = compileResult.diagnostics;
    return result;
}

void ShaderLibraryCompiler::handleFileChanged(
    const std::filesystem::path& path,
    TransactionalShaderState* txState)
{
    const ShaderLibraryEntry* existing = library_.find(path);
    if (!existing) {
        return;
    }

    size_t targetIndex = SIZE_MAX;
    for (size_t i = 0; i < library_.entryCount(); ++i) {
        if (library_.entry(i)->path == path) {
            targetIndex = i;
            break;
        }
    }
    if (targetIndex == SIZE_MAX) return;

    auto* e = library_.entryMutable(targetIndex);
    if (!e) return;

    updateContentHash(*e);

    if (!e->isActive) {
        e->status = ShaderEntryStatus::Pending;
        e->error.clear();
        e->diagnostics.clear();
        return;
    }

    auto compileResult = compileEntry(targetIndex);
    if (!compileResult.success) {
        return;
    }

    if (txState) {
        txState->beginCompile();

        CompileAndActivateResult activateResult;
        activateResult.diagnostics = compileResult.diagnostics;

        if (e->extension == ".slangp" && renderer_) {
            auto loadResult = renderer_->loadPreset(e->path);
            activateResult.committed = static_cast<bool>(loadResult);
        } else if (renderer_) {
            auto loadResult = renderer_->loadIndividualShader(e->compiledModule, e->path);
            activateResult.committed = static_cast<bool>(loadResult);
        } else {
            activateResult.committed = true;
        }

        if (!activateResult.committed) {
            txState->rollback();
            auto* e3 = library_.entryMutable(targetIndex);
            if (e3) {
                e3->status = ShaderEntryStatus::Active;
                e3->isActive = true;
            }
        } else {
            txState->commit();
        }
    }
}

const ShaderDiagnostics* ShaderLibraryCompiler::diagnostics(size_t index) const {
    auto* e = library_.entry(index);
    if (!e) return nullptr;
    return &e->diagnostics;
}

std::string ShaderLibraryCompiler::errorSummary(size_t index) const {
    auto* e = library_.entry(index);
    if (!e) return "Invalid index";
    if (e->error.empty()) return e->diagnostics.summary();
    return e->error;
}

void ShaderLibraryCompiler::updateContentHash(ShaderLibraryEntry& entry) {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(entry.path, ec);
    if (!ec) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        entry.lastWriteTime = static_cast<uint64_t>(sctp.time_since_epoch().count());
    }

    std::error_code sizeEc;
    entry.fileSize = std::filesystem::file_size(entry.path, sizeEc);

    auto content = FileSystem::readText(entry.path);
    if (content) {
        entry.contentHash = Hash::fnv1a(content.value());
    }
}

}  // namespace monix::renderer_vk
