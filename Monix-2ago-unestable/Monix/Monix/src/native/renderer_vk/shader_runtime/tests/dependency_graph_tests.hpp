#pragma once

#include "../dependencies/ShaderDependencyGraph.hpp"

#include <cassert>
#include <cstdio>

namespace monix::renderer_vk::tests {

inline void test_register_shader() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader_a.slang");
    assert(graph.hasNode("shader_a.slang"));
    assert(graph.nodeType("shader_a.slang") == DepNodeType::Shader);
    assert(graph.nodeCount() == 1);
    printf("  [PASS] register_shader\n");
}

inline void test_register_include() {
    ShaderDependencyGraph graph;
    graph.registerInclude("common.inc");
    assert(graph.hasNode("common.inc"));
    assert(graph.nodeType("common.inc") == DepNodeType::Include);
    assert(graph.nodeCount() == 1);
    printf("  [PASS] register_include\n");
}

inline void test_register_preset() {
    ShaderDependencyGraph graph;
    graph.registerPreset("preset.slangp");
    assert(graph.hasNode("preset.slangp"));
    assert(graph.nodeType("preset.slangp") == DepNodeType::Preset);
    assert(graph.nodeCount() == 1);
    printf("  [PASS] register_preset\n");
}

inline void test_dependency_edge() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader.slang");
    graph.registerInclude("common.inc");
    graph.addDependency("shader.slang", "common.inc");
    assert(graph.edgeCount() == 1);
    auto deps = graph.getDependencies("shader.slang");
    assert(deps.size() == 1);
    printf("  [PASS] dependency_edge\n");
}

inline void test_reverse_dependency() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader.slang");
    graph.registerInclude("common.inc");
    graph.addDependency("shader.slang", "common.inc");
    auto dependents = graph.getDependents("common.inc");
    assert(dependents.size() == 1);
    printf("  [PASS] reverse_dependency\n");
}

inline void test_transitive_dependency() {
    ShaderDependencyGraph graph;
    graph.registerPreset("preset.slangp");
    graph.registerShader("shader.slang");
    graph.registerInclude("common.inc");
    graph.registerInclude("math.inc");
    graph.addDependency("preset.slangp", "shader.slang");
    graph.addDependency("shader.slang", "common.inc");
    graph.addDependency("common.inc", "math.inc");

    auto affected = graph.invalidate("math.inc");
    assert(affected.anyFound);
    assert(affected.affectedShaders.size() >= 1);
    assert(affected.affectedPresets.size() >= 1);
    printf("  [PASS] transitive_dependency\n");
}

inline void test_multiple_dependents() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader_a.slang");
    graph.registerShader("shader_b.slang");
    graph.registerInclude("common.inc");
    graph.addDependency("shader_a.slang", "common.inc");
    graph.addDependency("shader_b.slang", "common.inc");

    auto dependents = graph.getDependents("common.inc");
    assert(dependents.size() == 2);
    printf("  [PASS] multiple_dependents\n");
}

inline void test_content_hash_change() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader.slang");
    graph.setNodeHash("shader.slang", 12345);
    assert(graph.nodeHash("shader.slang") == 12345);
    graph.setNodeHash("shader.slang", 67890);
    assert(graph.nodeHash("shader.slang") == 67890);
    printf("  [PASS] content_hash_change\n");
}

inline void test_unchanged_file_ignored() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader.slang");
    graph.registerInclude("common.inc");
    graph.addDependency("shader.slang", "common.inc");
    graph.setNodeHash("common.inc", 42);
    auto affected = graph.invalidate("common.inc");
    assert(affected.anyFound);
    assert(!affected.affectedShaders.empty());
    printf("  [PASS] unchanged_file_ignored\n");
}

inline void test_missing_dependency() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader.slang");
    auto deps = graph.getDependencies("nonexistent.inc");
    assert(deps.empty());
    auto dependents = graph.getDependents("nonexistent.inc");
    assert(dependents.empty());
    printf("  [PASS] missing_dependency\n");
}

inline void test_invalidate_shader() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader.slang");
    graph.registerInclude("common.inc");
    graph.addDependency("shader.slang", "common.inc");
    auto affected = graph.invalidate("common.inc");
    assert(affected.anyFound);
    assert(!affected.affectedShaders.empty());
    printf("  [PASS] invalidate_shader\n");
}

inline void test_invalidate_include() {
    ShaderDependencyGraph graph;
    graph.registerInclude("a.inc");
    graph.registerInclude("b.inc");
    graph.addDependency("a.inc", "b.inc");
    graph.registerShader("shader.slang");
    graph.addDependency("shader.slang", "a.inc");
    auto affected = graph.invalidate("b.inc");
    assert(affected.anyFound);
    assert(!affected.affectedShaders.empty());
    printf("  [PASS] invalidate_include\n");
}

inline void test_invalidate_transitive() {
    ShaderDependencyGraph graph;
    graph.registerPreset("preset.slangp");
    graph.registerShader("shader.slang");
    graph.registerInclude("level1.inc");
    graph.registerInclude("level2.inc");
    graph.registerInclude("level3.inc");
    graph.addDependency("preset.slangp", "shader.slang");
    graph.addDependency("shader.slang", "level1.inc");
    graph.addDependency("level1.inc", "level2.inc");
    graph.addDependency("level2.inc", "level3.inc");

    auto affected = graph.invalidate("level3.inc");
    assert(affected.anyFound);
    size_t totalAffected = affected.affectedShaders.size() + affected.affectedPresets.size();
    assert(totalAffected >= 2);
    printf("  [PASS] invalidate_transitive\n");
}

inline void test_set_node_dependencies() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader.slang");
    graph.registerInclude("a.inc");
    graph.registerInclude("b.inc");
    graph.setNodeDependencies("shader.slang", {"a.inc", "b.inc"});
    auto deps = graph.getDependencies("shader.slang");
    assert(deps.size() == 2);
    assert(graph.edgeCount() == 2);
    printf("  [PASS] set_node_dependencies\n");
}

inline void test_all_presets_shaders() {
    ShaderDependencyGraph graph;
    graph.registerPreset("p1.slangp");
    graph.registerPreset("p2.slangp");
    graph.registerShader("s1.slang");
    graph.registerShader("s2.slang");
    graph.registerInclude("inc.inc");
    assert(graph.allPresets().size() == 2);
    assert(graph.allShaders().size() == 2);
    assert(graph.allNodes().size() == 5);
    printf("  [PASS] all_presets_shaders\n");
}

inline void test_clear() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader.slang");
    graph.registerInclude("common.inc");
    graph.addDependency("shader.slang", "common.inc");
    assert(graph.nodeCount() > 0);
    graph.clear();
    assert(graph.nodeCount() == 0);
    assert(graph.edgeCount() == 0);
    printf("  [PASS] clear\n");
}

inline void test_deduplicate_edges() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader.slang");
    graph.registerInclude("common.inc");
    graph.addDependency("shader.slang", "common.inc");
    graph.addDependency("shader.slang", "common.inc");
    assert(graph.edgeCount() == 1);
    printf("  [PASS] deduplicate_edges\n");
}

}  // namespace monix::renderer_vk::tests
