#include "TransactionalShaderState.hpp"

#include <sstream>

namespace monix::renderer_vk {

bool TransactionalShaderState::hasActive() const {
    return active_.has_value();
}

const CompiledPreset* TransactionalShaderState::active() const {
    return active_ ? &*active_ : nullptr;
}

const CompiledPreset* TransactionalShaderState::candidate() const {
    return candidate_ ? &*candidate_ : nullptr;
}

void TransactionalShaderState::setActive(CompiledPreset preset) {
    active_ = std::move(preset);
    state_ = SwapState::Idle;
}

void TransactionalShaderState::beginCompile() {
    candidate_.reset();
    previousActive_.reset();
    if (active_) {
        previousActive_ = active_;
    }
    state_ = SwapState::Compiling;
}

Status TransactionalShaderState::setCandidate(CompiledPreset preset) {
    if (state_ != SwapState::Compiling) {
        return Status::failure("Cannot set candidate: not in Compiling state");
    }

    auto validation = validateCompiledPreset(preset);
    if (!validation) {
        state_ = SwapState::Failed;
        return validation;
    }

    candidate_ = std::move(preset);
    state_ = SwapState::Pending;
    return Status::success();
}

TransactionalSwapResult TransactionalShaderState::commit() {
    TransactionalSwapResult result;
    result.hadActiveBefore = active_.has_value();

    if (state_ == SwapState::Failed) {
        result.committed = false;
        result.rejectionReason = "Candidate was previously rejected";
        candidate_.reset();
        state_ = SwapState::Idle;
        previousActive_.reset();
        return result;
    }

    if (state_ != SwapState::Pending) {
        result.committed = false;
        result.rejectionReason = "No valid candidate pending commit (state=" +
                                 std::string(swapStateName(state_)) + ")";
        state_ = SwapState::Idle;
        previousActive_.reset();
        return result;
    }

    state_ = SwapState::Committing;
    active_ = std::move(candidate_);
    candidate_.reset();
    previousActive_.reset();
    state_ = SwapState::Idle;
    result.committed = true;
    return result;
}

void TransactionalShaderState::discard() {
    candidate_.reset();
    state_ = SwapState::Failed;

    if (active_) {
        state_ = SwapState::Idle;
    }
    previousActive_.reset();
}

void TransactionalShaderState::rollback() {
    candidate_.reset();
    if (previousActive_) {
        active_ = std::move(previousActive_);
    }
    previousActive_.reset();
    state_ = SwapState::Idle;
}

const CompiledPreset* TransactionalShaderState::previousActive() const {
    return previousActive_ ? &*previousActive_ : nullptr;
}

Status TransactionalShaderState::validateCompiledPreset(const CompiledPreset& preset) const {
    ShaderDiagnostics diags;

    if (preset.passes.empty()) {
        return Status::failure("VALIDATION_ERROR: Preset has no passes");
    }

    if (!validatePasses(preset, diags)) {
        return Status::failure("VALIDATION_ERROR: " + diags.formatAll());
    }

    if (!validateGraph(preset, diags)) {
        return Status::failure("VALIDATION_ERROR: " + diags.formatAll());
    }

    if (!validateExecutionPlan(preset, diags)) {
        return Status::failure("VALIDATION_ERROR: " + diags.formatAll());
    }

    return Status::success();
}

bool TransactionalShaderState::validatePasses(const CompiledPreset& preset, ShaderDiagnostics& diags) const {
    bool allValid = true;

    for (size_t i = 0; i < preset.passes.size(); ++i) {
        const auto& pass = preset.passes[i];

        if (pass.vertex.spirv.empty() && pass.fragment.spirv.empty()) {
            diags.add(ShaderDiagnostic{
                ShaderDiagnosticKind::ValidationError,
                DiagnosticSeverity::Error,
                pass.preset.shaderPath.string(),
                0, 0,
                "all",
                "EMPTY_SPIRV",
                "Pass " + std::to_string(i) + ": both vertex and fragment SPIR-V are empty"
            });
            allValid = false;
            continue;
        }

        if (pass.fragment.spirv.empty()) {
            diags.add(ShaderDiagnostic{
                ShaderDiagnosticKind::ValidationError,
                DiagnosticSeverity::Error,
                pass.preset.shaderPath.string(),
                0, 0,
                "fragment",
                "EMPTY_FRAGMENT_SPIRV",
                "Pass " + std::to_string(i) + ": fragment SPIR-V is empty"
            });
            allValid = false;
        }

        if (pass.fragment.reflection.uniformBlocks.empty() &&
            pass.fragment.reflection.samplers.empty() &&
            pass.fragment.reflection.descriptors.empty()) {
            diags.add(ShaderDiagnostic{
                ShaderDiagnosticKind::ValidationError,
                DiagnosticSeverity::Warning,
                pass.preset.shaderPath.string(),
                0, 0,
                "reflection",
                "NO_REFLECTION_DATA",
                "Pass " + std::to_string(i) + ": no uniform blocks, samplers, or descriptors detected"
            });
        }
    }

    return allValid;
}

bool TransactionalShaderState::validateGraph(const CompiledPreset& preset, ShaderDiagnostics& diags) const {
    if (preset.graph.passes.empty()) {
        diags.add(ShaderDiagnostic{
            ShaderDiagnosticKind::ValidationError,
            DiagnosticSeverity::Error,
            preset.preset.path.string(),
            0, 0,
            "graph",
            "EMPTY_GRAPH",
            "Render graph has no passes"
        });
        return false;
    }

    if (preset.graph.passes.size() != preset.passes.size()) {
        diags.add(ShaderDiagnostic{
            ShaderDiagnosticKind::ValidationError,
            DiagnosticSeverity::Error,
            preset.preset.path.string(),
            0, 0,
            "graph",
            "GRAPH_PASS_MISMATCH",
            "Graph has " + std::to_string(preset.graph.passes.size()) +
            " passes but preset has " + std::to_string(preset.passes.size())
        });
        return false;
    }

    return true;
}

bool TransactionalShaderState::validateExecutionPlan(const CompiledPreset& preset, ShaderDiagnostics& diags) const {
    if (preset.executionPlan.passOrder.empty()) {
        diags.add(ShaderDiagnostic{
            ShaderDiagnosticKind::ValidationError,
            DiagnosticSeverity::Error,
            preset.preset.path.string(),
            0, 0,
            "execution",
            "EMPTY_EXECUTION_PLAN",
            "Execution plan has no passes in order"
        });
        return false;
    }

    for (int idx : preset.executionPlan.passOrder) {
        if (idx < 0 || static_cast<size_t>(idx) >= preset.passes.size()) {
            diags.add(ShaderDiagnostic{
                ShaderDiagnosticKind::ValidationError,
                DiagnosticSeverity::Error,
                preset.preset.path.string(),
                0, 0,
                "execution",
                "INVALID_PASS_INDEX",
                "Execution plan references pass index " + std::to_string(idx) +
                " but preset has " + std::to_string(preset.passes.size()) + " passes"
            });
            return false;
        }
    }

    return true;
}

std::string TransactionalShaderState::formatRejection(const CompiledPreset& preset, const Status& error) const {
    std::ostringstream oss;
    oss << "[ShaderValidation]\n";
    oss << "Preset: " << preset.preset.path.filename().string() << "\n";
    oss << "Passes: " << preset.passes.size() << "\n";
    oss << "Error: " << error.message << "\n";
    oss << "\nACTION:\n";
    oss << "Candidate rejected\n";
    if (active_) {
        oss << "Previous active preset retained: " << active_->preset.path.filename().string() << "\n";
    } else {
        oss << "No active preset (first load)\n";
    }
    return oss.str();
}

}  // namespace monix::renderer_vk
