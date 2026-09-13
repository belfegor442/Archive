#pragma once

#include "../TransactionalShaderState.hpp"
#include "../../graph/RenderGraph.hpp"
#include "../../compiler/ShaderReflection.hpp"
#include <cassert>
#include <iostream>

namespace monix::renderer_vk::tests {

class TransactionalSwapTests {
public:
    static int runAll() {
        int passed = 0;
        int failed = 0;

        auto run = [&](const char* name, auto fn) {
            try {
                fn();
                std::cout << "  [PASS] " << name << "\n";
                ++passed;
            } catch (const std::exception& e) {
                std::cout << "  [FAIL] " << name << ": " << e.what() << "\n";
                ++failed;
            }
        };

        std::cout << "TransactionalSwap tests:\n";
        run("no_active_initially", testNoActiveInitially);
        run("set_active", testSetActive);
        run("valid_candidate_activation", testValidCandidateActivation);
        run("invalid_candidate_rejection", testInvalidCandidateRejection);
        run("compile_failure_fallback", testCompileFailureFallback);
        run("resource_failure_fallback", testResourceFailureFallback);
        run("pipeline_failure_fallback", testPipelineFailureFallback);
        run("first_valid_load", testFirstValidLoad);
        run("first_invalid_load", testFirstInvalidLoad);
        run("rollback_restores_previous", testRollbackRestoresPrevious);
        run("discard_clears_candidate", testDiscardClearsCandidate);
        run("state_transitions", testStateTransitions);

        std::cout << "\nRegression tests:\n";
        run("backend_failure_preserves_previous", testBackendFailurePreservesPrevious);
        run("two_updates_b_replaces_a_c_fails_b_active", testTwoUpdatesBReplacesACFailedBActive);
        run("two_updates_b_replaces_a_c_gpu_fails_b_active", testTwoUpdatesBReplacesACGPUFailedBActive);
        run("previous_active_accessor", testPreviousActiveAccessor);
        run("rollback_with_no_previous", testRollbackWithNoPrevious);
        run("rollback_after_commit_preserves_new", testRollbackAfterCommitPreservesNew);

        std::cout << "\n" << passed << " passed, " << failed << " failed\n";
        if (failed > 0) throw std::runtime_error("Some tests failed");
        return failed;
    }

private:
    static void assert_(bool cond, const char* msg) {
        if (!cond) throw std::runtime_error(msg);
    }

    static CompiledPreset makeValidPreset(const std::string& name) {
        CompiledPreset preset;
        preset.preset.path = std::filesystem::path("test") / (name + ".slangp");

        CompiledPass pass;
        pass.preset.index = 0;
        pass.preset.shaderPath = std::filesystem::path("test") / (name + ".slang");
        pass.vertex.spirv = {0x03, 0x02, 0x23, 0x07};
        pass.fragment.spirv = {0x03, 0x02, 0x23, 0x07};

        PushConstantReflection pc;
        pc.name = "PushConstants";
        pc.size = 16;
        pass.fragment.reflection.pushConstants.push_back(pc);

        preset.passes.push_back(pass);

        PassNode pn;
        pn.passIndex = 0;
        pn.name = name;
        preset.graph.passes.push_back(pn);

        preset.executionPlan.passOrder.push_back(0);
        return preset;
    }

    static CompiledPreset makeEmptyPreset() {
        CompiledPreset preset;
        preset.preset.path = std::filesystem::path("test") / "empty.slangp";
        return preset;
    }

    static CompiledPreset makeNoSpirvPreset() {
        CompiledPreset preset;
        preset.preset.path = std::filesystem::path("test") / "no_spirv.slangp";

        CompiledPass pass;
        pass.preset.index = 0;
        pass.preset.shaderPath = std::filesystem::path("test") / "no_spirv.slang";
        preset.passes.push_back(pass);

        PassNode pn;
        pn.passIndex = 0;
        preset.graph.passes.push_back(pn);
        preset.executionPlan.passOrder.push_back(0);
        return preset;
    }

    static CompiledPreset makeGraphMismatchPreset() {
        CompiledPreset preset;
        preset.preset.path = std::filesystem::path("test") / "mismatch.slangp";

        CompiledPass pass;
        pass.preset.index = 0;
        pass.vertex.spirv = {0x03, 0x02, 0x23, 0x07};
        pass.fragment.spirv = {0x03, 0x02, 0x23, 0x07};
        preset.passes.push_back(pass);

        preset.executionPlan.passOrder.push_back(0);
        return preset;
    }

    static void testNoActiveInitially() {
        TransactionalShaderState ts;
        assert_(!ts.hasActive(), "Should not have active initially");
        assert_(ts.active() == nullptr, "Active should be nullptr");
        assert_(ts.state() == SwapState::Idle, "Should be Idle");
    }

    static void testSetActive() {
        TransactionalShaderState ts;
        auto preset = makeValidPreset("test_a");
        ts.setActive(std::move(preset));
        assert_(ts.hasActive(), "Should have active after setActive");
        assert_(ts.active() != nullptr, "Active should not be nullptr");
        assert_(ts.state() == SwapState::Idle, "Should be Idle");
    }

    static void testValidCandidateActivation() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("old_shader"));

        ts.beginCompile();
        assert_(ts.state() == SwapState::Compiling, "Should be Compiling");

        auto candidate = makeValidPreset("new_shader");
        auto status = ts.setCandidate(std::move(candidate));
        assert_(status.ok, "Valid candidate should be accepted");
        assert_(ts.state() == SwapState::Pending, "Should be Pending");

        auto result = ts.commit();
        assert_(result.committed, "Commit should succeed");
        assert_(result.hadActiveBefore, "Should have had active before");
        assert_(ts.hasActive(), "Should still have active");
        assert_(ts.state() == SwapState::Idle, "Should be Idle");

        std::string activeName = ts.active()->preset.path.stem().string();
        assert_(activeName == "new_shader", "Active should be new_shader");
    }

    static void testInvalidCandidateRejection() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("old_shader"));

        ts.beginCompile();
        auto candidate = makeEmptyPreset();
        auto status = ts.setCandidate(std::move(candidate));
        assert_(!status.ok, "Empty preset should be rejected");
        assert_(ts.state() == SwapState::Failed, "Should be Failed");

        ts.discard();
        assert_(ts.hasActive(), "Active should still exist after discard");
        std::string activeName = ts.active()->preset.path.stem().string();
        assert_(activeName == "old_shader", "Active should still be old_shader");
    }

    static void testCompileFailureFallback() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("old_shader"));

        ts.beginCompile();
        ts.discard();
        assert_(ts.hasActive(), "Active should still exist after discard");
        std::string activeName = ts.active()->preset.path.stem().string();
        assert_(activeName == "old_shader", "Active should still be old_shader");
    }

    static void testResourceFailureFallback() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("old_shader"));

        ts.beginCompile();
        auto candidate = makeNoSpirvPreset();
        auto status = ts.setCandidate(std::move(candidate));
        assert_(!status.ok, "No SPIR-V preset should be rejected");
        ts.discard();

        assert_(ts.hasActive(), "Active should still exist");
        std::string activeName = ts.active()->preset.path.stem().string();
        assert_(activeName == "old_shader", "Active should still be old_shader");
    }

    static void testPipelineFailureFallback() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("old_shader"));

        ts.beginCompile();
        auto candidate = makeGraphMismatchPreset();
        auto status = ts.setCandidate(std::move(candidate));
        assert_(!status.ok, "Graph mismatch should be rejected");
        ts.discard();

        assert_(ts.hasActive(), "Active should still exist");
        std::string activeName = ts.active()->preset.path.stem().string();
        assert_(activeName == "old_shader", "Active should still be old_shader");
    }

    static void testFirstValidLoad() {
        TransactionalShaderState ts;
        assert_(!ts.hasActive(), "Should not have active initially");

        ts.beginCompile();
        auto candidate = makeValidPreset("first_shader");
        auto status = ts.setCandidate(std::move(candidate));
        assert_(status.ok, "First valid candidate should be accepted");

        auto result = ts.commit();
        assert_(result.committed, "Commit should succeed");
        assert_(!result.hadActiveBefore, "Should not have had active before");
        assert_(ts.hasActive(), "Should now have active");

        std::string activeName = ts.active()->preset.path.stem().string();
        assert_(activeName == "first_shader", "Active should be first_shader");
    }

    static void testFirstInvalidLoad() {
        TransactionalShaderState ts;
        assert_(!ts.hasActive(), "Should not have active initially");

        ts.beginCompile();
        auto candidate = makeEmptyPreset();
        auto status = ts.setCandidate(std::move(candidate));
        assert_(!status.ok, "Empty preset should be rejected");
        ts.discard();

        assert_(!ts.hasActive(), "Should still not have active");
        assert_(ts.active() == nullptr, "Active should be nullptr");
    }

    static void testRollbackRestoresPrevious() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("original_shader"));

        ts.beginCompile();
        auto candidate = makeValidPreset("attempted_shader");
        ts.setCandidate(std::move(candidate));

        ts.rollback();
        assert_(ts.hasActive(), "Active should still exist after rollback");
        std::string activeName = ts.active()->preset.path.stem().string();
        assert_(activeName == "original_shader", "Active should be original_shader");
        assert_(ts.state() == SwapState::Idle, "Should be Idle after rollback");
    }

    static void testDiscardClearsCandidate() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("old_shader"));

        ts.beginCompile();
        auto candidate = makeValidPreset("new_shader");
        ts.setCandidate(std::move(candidate));

        ts.discard();
        assert_(ts.hasActive(), "Active should still exist");
        std::string activeName = ts.active()->preset.path.stem().string();
        assert_(activeName == "old_shader", "Active should still be old_shader");
    }

    static void testStateTransitions() {
        TransactionalShaderState ts;

        assert_(ts.state() == SwapState::Idle, "Initial state should be Idle");

        ts.beginCompile();
        assert_(ts.state() == SwapState::Compiling, "Should be Compiling");

        auto candidate = makeValidPreset("test");
        ts.setCandidate(std::move(candidate));
        assert_(ts.state() == SwapState::Pending, "Should be Pending");

        ts.commit();
        assert_(ts.state() == SwapState::Idle, "Should be Idle after commit");

        ts.beginCompile();
        ts.discard();
        assert_(ts.state() == SwapState::Idle, "Should be Idle after discard");
    }

    static void testBackendFailurePreservesPrevious() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("shader_a"));

        ts.beginCompile();
        auto candidate = makeValidPreset("shader_b");
        ts.setCandidate(std::move(candidate));

        assert_(ts.previousActive() != nullptr, "previousActive should exist before rollback");
        std::string prevName = ts.previousActive()->preset.path.stem().string();
        assert_(prevName == "shader_a", "previousActive should be shader_a");

        ts.rollback();
        assert_(ts.hasActive(), "Active should exist after rollback");
        std::string activeName = ts.active()->preset.path.stem().string();
        assert_(activeName == "shader_a", "Active should be shader_a after rollback");
        assert_(ts.previousActive() == nullptr, "previousActive should be cleared after rollback");
    }

    static void testTwoUpdatesBReplacesACFailedBActive() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("shader_a"));

        ts.beginCompile();
        auto candidateB = makeValidPreset("shader_b");
        ts.setCandidate(std::move(candidateB));
        ts.commit();
        std::string nameAfterB = ts.active()->preset.path.stem().string();
        assert_(nameAfterB == "shader_b", "Should be shader_b after first commit");

        ts.beginCompile();
        auto candidateC = makeEmptyPreset();
        auto statusC = ts.setCandidate(std::move(candidateC));
        assert_(!statusC.ok, "Empty preset C should be rejected");
        ts.discard();

        assert_(ts.hasActive(), "Active should still exist after C rejection");
        std::string activeAfterC = ts.active()->preset.path.stem().string();
        assert_(activeAfterC == "shader_b", "Active should be shader_b, not shader_a");
    }

    static void testTwoUpdatesBReplacesACGPUFailedBActive() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("shader_a"));

        ts.beginCompile();
        auto candidateB = makeValidPreset("shader_b");
        ts.setCandidate(std::move(candidateB));
        ts.commit();

        ts.beginCompile();
        auto candidateC = makeValidPreset("shader_c");
        ts.setCandidate(std::move(candidateC));

        ts.rollback();
        assert_(ts.hasActive(), "Active should exist after rollback");
        std::string activeAfterRollback = ts.active()->preset.path.stem().string();
        assert_(activeAfterRollback == "shader_b", "Active should be shader_b after rollback, not shader_a");
    }

    static void testPreviousActiveAccessor() {
        TransactionalShaderState ts;
        assert_(ts.previousActive() == nullptr, "No previous active initially");

        ts.setActive(makeValidPreset("shader_a"));
        assert_(ts.previousActive() == nullptr, "No previous active after setActive");

        ts.beginCompile();
        assert_(ts.previousActive() != nullptr, "previousActive should be set after beginCompile");
        std::string prevName = ts.previousActive()->preset.path.stem().string();
        assert_(prevName == "shader_a", "previousActive should be shader_a");
    }

    static void testRollbackWithNoPrevious() {
        TransactionalShaderState ts;
        ts.beginCompile();
        auto candidate = makeValidPreset("shader_a");
        ts.setCandidate(std::move(candidate));

        ts.rollback();
        assert_(!ts.hasActive(), "Should not have active after rollback with no previous");
        assert_(ts.active() == nullptr, "Active should be nullptr");
    }

    static void testRollbackAfterCommitPreservesNew() {
        TransactionalShaderState ts;
        ts.setActive(makeValidPreset("shader_a"));

        ts.beginCompile();
        auto candidateB = makeValidPreset("shader_b");
        ts.setCandidate(std::move(candidateB));
        ts.commit();

        std::string afterCommit = ts.active()->preset.path.stem().string();
        assert_(afterCommit == "shader_b", "Should be shader_b after commit");

        ts.beginCompile();
        auto candidateC = makeValidPreset("shader_c");
        ts.setCandidate(std::move(candidateC));
        ts.rollback();

        std::string afterRollback = ts.active()->preset.path.stem().string();
        assert_(afterRollback == "shader_b", "Should be shader_b after rollback of C");
    }
};

}  // namespace monix::renderer_vk::tests
