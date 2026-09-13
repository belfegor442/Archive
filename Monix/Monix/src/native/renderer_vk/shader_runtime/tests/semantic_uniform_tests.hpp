#pragma once

#include "../core/SemanticUniforms.hpp"

#include <cassert>
#include <cstdio>
#include <string>

namespace monix::renderer_vk::tests {

inline void test_semantic_detect_input_texture() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("Source") == ShaderSemantic::InputTexture);
    assert(resolver.detectSemantic("source") == ShaderSemantic::InputTexture);
    assert(resolver.detectSemantic("InputTexture") == ShaderSemantic::InputTexture);
    assert(resolver.detectSemantic("SourceTexture") == ShaderSemantic::InputTexture);
    printf("  [PASS] semantic_detect_input_texture\n");
}

inline void test_semantic_detect_input_size() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("InputSize") == ShaderSemantic::InputSize);
    assert(resolver.detectSemantic("inputSize") == ShaderSemantic::InputSize);
    assert(resolver.detectSemantic("SourceSize") == ShaderSemantic::SourceSize);
    printf("  [PASS] semantic_detect_input_size\n");
}

inline void test_semantic_detect_time() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("Time") == ShaderSemantic::Time);
    assert(resolver.detectSemantic("time") == ShaderSemantic::Time);
    assert(resolver.detectSemantic("Timer") == ShaderSemantic::Time);
    assert(resolver.detectSemantic("ElapsedTime") == ShaderSemantic::Time);
    printf("  [PASS] semantic_detect_time\n");
}

inline void test_semantic_detect_frame_count() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("FrameCount") == ShaderSemantic::FrameCount);
    assert(resolver.detectSemantic("frameCount") == ShaderSemantic::FrameCount);
    assert(resolver.detectSemantic("frame_count") == ShaderSemantic::FrameCount);
    assert(resolver.detectSemantic("FrameCounter") == ShaderSemantic::FrameCount);
    printf("  [PASS] semantic_detect_frame_count\n");
}

inline void test_semantic_detect_frame_direction() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("FrameDirection") == ShaderSemantic::FrameDirection);
    assert(resolver.detectSemantic("frameDirection") == ShaderSemantic::FrameDirection);
    assert(resolver.detectSemantic("direction") == ShaderSemantic::FrameDirection);
    printf("  [PASS] semantic_detect_frame_direction\n");
}

inline void test_semantic_detect_viewport_size() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("ViewportSize") == ShaderSemantic::ViewportSize);
    assert(resolver.detectSemantic("viewportSize") == ShaderSemantic::ViewportSize);
    assert(resolver.detectSemantic("viewport_size") == ShaderSemantic::ViewportSize);
    printf("  [PASS] semantic_detect_viewport_size\n");
}

inline void test_semantic_detect_original_texture() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("Original") == ShaderSemantic::OriginalTexture);
    assert(resolver.detectSemantic("original") == ShaderSemantic::OriginalTexture);
    assert(resolver.detectSemantic("OriginalTexture") == ShaderSemantic::OriginalTexture);
    printf("  [PASS] semantic_detect_original_texture\n");
}

inline void test_semantic_detect_unknown() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("RandomUniform123") == ShaderSemantic::None);
    assert(resolver.detectSemantic("myCustomParam") == ShaderSemantic::None);
    printf("  [PASS] semantic_detect_unknown\n");
}

inline void test_semantic_exact_match_priority() {
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("InputTexture");
    assert(match.semantic == ShaderSemantic::InputTexture);
    assert(match.exactMatch == true);
    assert(match.aliasMatch == false);
    printf("  [PASS] semantic_exact_match_priority\n");
}

inline void test_semantic_alias_match() {
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("Source");
    assert(match.semantic == ShaderSemantic::InputTexture);
    assert(match.aliasMatch == true);
    assert(match.exactMatch == false);
    printf("  [PASS] semantic_alias_match\n");
}

inline void test_semantic_original_name_preserved() {
    auto resolver = SemanticUniformResolver::makeDefault();
    auto match = resolver.resolve("Source");
    assert(match.originalName == "Source");
    assert(match.semantic == ShaderSemantic::InputTexture);
    printf("  [PASS] semantic_original_name_preserved\n");
}

inline void test_semantic_canonical_name() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(std::string(resolver.canonicalName(ShaderSemantic::InputTexture)) == "inputtexture");
    assert(std::string(resolver.canonicalName(ShaderSemantic::Time)) == "time");
    assert(std::string(resolver.canonicalName(ShaderSemantic::FrameCount)) == "framecount");
    printf("  [PASS] semantic_canonical_name\n");
}

inline void test_semantic_is_known_alias() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.isKnownAlias("Source") == true);
    assert(resolver.isKnownAlias("source") == true);
    assert(resolver.isKnownAlias("InputTexture") == true);
    assert(resolver.isKnownAlias("Time") == true);
    assert(resolver.isKnownAlias("Timer") == true);
    assert(resolver.isKnownAlias("RandomUnknown") == false);
    printf("  [PASS] semantic_is_known_alias\n");
}

inline void test_semantic_custom_alias() {
    auto resolver = SemanticUniformResolver::makeDefault();
    resolver.addAlias(ShaderSemantic::Time, "ElapsedTimeMs");
    assert(resolver.detectSemantic("ElapsedTimeMs") == ShaderSemantic::Time);
    assert(resolver.isKnownAlias("ElapsedTimeMs") == true);
    printf("  [PASS] semantic_custom_alias\n");
}

inline void test_semantic_shader_names() {
    assert(std::string(shaderSemanticName(ShaderSemantic::None)) == "None");
    assert(std::string(shaderSemanticName(ShaderSemantic::InputTexture)) == "InputTexture");
    assert(std::string(shaderSemanticName(ShaderSemantic::Time)) == "Time");
    assert(std::string(shaderSemanticName(ShaderSemantic::FrameCount)) == "FrameCount");
    assert(std::string(shaderSemanticName(ShaderSemantic::ViewportSize)) == "ViewportSize");
    printf("  [PASS] semantic_shader_names\n");
}

inline void test_semantic_delta_time() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("DeltaTime") == ShaderSemantic::DeltaTime);
    assert(resolver.detectSemantic("delta_time") == ShaderSemantic::DeltaTime);
    assert(resolver.detectSemantic("FrameDelta") == ShaderSemantic::DeltaTime);
    printf("  [PASS] semantic_delta_time\n");
}

inline void test_semantic_output_size() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("OutputSize") == ShaderSemantic::OutputSize);
    assert(resolver.detectSemantic("outputSize") == ShaderSemantic::OutputSize);
    printf("  [PASS] semantic_output_size\n");
}

inline void test_semantic_pass_output() {
    auto resolver = SemanticUniformResolver::makeDefault();
    assert(resolver.detectSemantic("PassOutput") == ShaderSemantic::PassOutput);
    assert(resolver.detectSemantic("PreviousPass") == ShaderSemantic::PassOutput);
    assert(resolver.detectSemantic("previousPass") == ShaderSemantic::PassOutput);
    printf("  [PASS] semantic_pass_output\n");
}

}  // namespace monix::renderer_vk::tests
