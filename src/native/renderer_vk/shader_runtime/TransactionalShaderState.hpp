#pragma once

#include "../compiler/CompiledPreset.hpp"
#include "../core/Result.hpp"
#include "../shader_runtime/core/ShaderDiagnostics.hpp"
#include "../runtime/RuntimeManagers.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace monix::renderer_vk {

enum class SwapState : std::uint8_t {
    Idle,
    Compiling,
    Validating,
    Pending,
    Committing,
    Failed
};

inline const char* swapStateName(SwapState s) {
    switch (s) {
    case SwapState::Idle:      return "Idle";
    case SwapState::Compiling: return "Compiling";
    case SwapState::Validating:return "Validating";
    case SwapState::Pending:   return "Pending";
    case SwapState::Committing:return "Committing";
    case SwapState::Failed:    return "Failed";
    }
    return "Unknown";
}

struct TransactionalSwapResult {
    bool committed = false;
    bool hadActiveBefore = false;
    std::string rejectionReason;
    ShaderDiagnostics diagnostics;
};

class TransactionalShaderState {
public:
    TransactionalShaderState() = default;
    ~TransactionalShaderState() = default;

    TransactionalShaderState(const TransactionalShaderState&) = delete;
    TransactionalShaderState& operator=(const TransactionalShaderState&) = delete;

    bool hasActive() const;
    const CompiledPreset* active() const;
    const CompiledPreset* candidate() const;
    SwapState state() const { return state_; }

    void setActive(CompiledPreset preset);

    void beginCompile();
    Status setCandidate(CompiledPreset preset);
    TransactionalSwapResult commit();
    void discard();
    void rollback();

    const CompiledPreset* previousActive() const;

    Status validateCompiledPreset(const CompiledPreset& preset) const;
    std::string formatRejection(const CompiledPreset& preset, const Status& error) const;

private:
    bool validatePasses(const CompiledPreset& preset, ShaderDiagnostics& diags) const;
    bool validateGraph(const CompiledPreset& preset, ShaderDiagnostics& diags) const;
    bool validateExecutionPlan(const CompiledPreset& preset, ShaderDiagnostics& diags) const;

    SwapState state_ = SwapState::Idle;
    std::optional<CompiledPreset> active_;
    std::optional<CompiledPreset> candidate_;
    std::optional<CompiledPreset> previousActive_;
};

}  // namespace monix::renderer_vk
