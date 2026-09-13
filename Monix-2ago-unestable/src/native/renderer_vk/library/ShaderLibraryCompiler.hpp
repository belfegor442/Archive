#pragma once

#include "ShaderLibrary.hpp"
#include "../shader_runtime/ShaderRuntime.hpp"
#include "../shader_runtime/core/ShaderModule.hpp"
#include "../shader_runtime/core/ShaderDiagnostics.hpp"
#include "../shader_runtime/TransactionalShaderState.hpp"
#include "../core/Result.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace monix::renderer_vk {

class ShaderRenderer;

struct CompileEntryResult {
    bool success = false;
    ShaderModule module;
    ShaderDiagnostics diagnostics;
    std::string errorMessage;
};

struct CompileAndActivateResult {
    bool committed = false;
    std::string rejectionReason;
    ShaderDiagnostics diagnostics;
};

class ShaderLibraryCompiler {
public:
    ShaderLibraryCompiler(ShaderLibrary& library, ShaderRuntime& runtime);
    ShaderLibraryCompiler(ShaderLibrary& library, ShaderRuntime& runtime, ShaderRenderer* renderer);

    CompileEntryResult compileEntry(size_t index);
    CompileEntryResult compileEntryByPath(const std::filesystem::path& path);

    CompileAndActivateResult compileAndActivate(
        size_t index,
        TransactionalShaderState& txState);

    void handleFileChanged(
        const std::filesystem::path& path,
        TransactionalShaderState* txState = nullptr);

    const ShaderDiagnostics* diagnostics(size_t index) const;
    std::string errorSummary(size_t index) const;

    void setRenderer(ShaderRenderer* renderer) { renderer_ = renderer; }
    ShaderRenderer* renderer() const { return renderer_; }

    static ShaderLibraryCompiler forTests(ShaderLibrary& library);

private:
    CompileEntryResult compileSlangOrGlsl(ShaderLibraryEntry& entry);
    CompileEntryResult compilePreset(ShaderLibraryEntry& entry);

    void updateContentHash(ShaderLibraryEntry& entry);

    ShaderLibrary& library_;
    ShaderRuntime* runtime_;
    ShaderRenderer* renderer_ = nullptr;
};

}  // namespace monix::renderer_vk
