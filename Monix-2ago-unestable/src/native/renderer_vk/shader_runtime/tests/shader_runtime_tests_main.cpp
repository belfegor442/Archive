#include <iostream>

#include "renderer_vk/shader_runtime/tests/shader_runtime_tests.hpp"
#include "renderer_vk/shader_runtime/tests/glsl_adapter_tests.hpp"
#include "renderer_vk/shader_runtime/tests/semantic_uniform_tests.hpp"
#include "renderer_vk/shader_runtime/tests/dependency_graph_tests.hpp"
#include "renderer_vk/shader_runtime/tests/hot_reload_tests.hpp"
#include "renderer_vk/shader_runtime/tests/shader_cache_tests.hpp"
#include "renderer_vk/validation/gpu_shader_validator_tests.hpp"

#ifdef RUN_PRODUCTION_HARDENING
#include "renderer_vk/shader_runtime/tests/production_hardening_tests.hpp"
#endif

int main() {
    std::cout << "=== MONIX ShaderRuntime Tests ===\n\n";

    try {
        monix::renderer_vk::tests::ShaderRuntimeTests::runAll();

        std::cout << "\n=== GLSL Adapter Tests ===\n\n";
        using namespace monix::renderer_vk::tests;
        test_glsl_language_detected();
        test_glsl_adapter_available();
        test_glsl_adapter_rejects_slang();
        test_glsl_adapter_name();
        test_glsl_compile_invalid_source();
        test_glsl_compile_valid_source();
        test_glsl_reflection();
        test_glsl_version_detection();
        test_glsl_include_handling();
        test_glsl_spirv_generation();
        test_glsl_regression_slang();
        test_glsl_shader_red_output();
        test_glsl_shader_blue_output();
        test_glsl_shader_red_then_blue();

        std::cout << "\n=== Semantic Uniform Tests ===\n\n";
        test_semantic_detect_input_texture();
        test_semantic_detect_input_size();
        test_semantic_detect_time();
        test_semantic_detect_frame_count();
        test_semantic_detect_frame_direction();
        test_semantic_detect_viewport_size();
        test_semantic_detect_original_texture();
        test_semantic_detect_unknown();
        test_semantic_exact_match_priority();
        test_semantic_alias_match();
        test_semantic_original_name_preserved();
        test_semantic_canonical_name();
        test_semantic_is_known_alias();
        test_semantic_custom_alias();
        test_semantic_shader_names();
        test_semantic_delta_time();
        test_semantic_output_size();
        test_semantic_pass_output();

        std::cout << "\n=== Dependency Graph Tests ===\n\n";
        test_register_shader();
        test_register_include();
        test_register_preset();
        test_dependency_edge();
        test_reverse_dependency();
        test_transitive_dependency();
        test_multiple_dependents();
        test_content_hash_change();
        test_unchanged_file_ignored();
        test_missing_dependency();
        test_invalidate_shader();
        test_invalidate_include();
        test_invalidate_transitive();
        test_set_node_dependencies();
        test_all_presets_shaders();
        test_clear();
        test_deduplicate_edges();

        std::cout << "\n=== Hot Reload Tests ===\n\n";
        test_hot_reload_config();
        test_hot_reload_create();
        test_hot_reload_start_stop();
        test_hot_reload_callback();
        test_hot_reload_relevant_extensions();
        test_hot_reload_content_hash();
        test_hot_reload_file_write_time();
        test_hot_reload_request_id_increments();
        test_hot_reload_enqueue();
        test_hot_reload_add_remove_watch();
        test_hot_reload_dependency_integration();
        test_hot_reload_debounce_window();
        test_hot_reload_file_state_tracking();
        test_hot_reload_full_pipeline();
        test_hot_reload_stale_request_discarded();

        std::cout << "\n=== Shader Cache Tests ===\n\n";
        test_cache_key_generation();
        test_cache_key_different_source();
        test_cache_key_different_stage();
        test_cache_key_different_deps();
        test_cache_key_same_inputs();
        test_cache_store_and_retrieve();
        test_cache_integrity_valid();
        test_cache_integrity_corrupted_spirv();
        test_cache_invalidate();
        test_cache_invalidate_all();
        test_cache_stats();
        test_cache_atomic_write_no_tmp_left();
        test_cache_manifest_content();
        test_cache_manifest_roundtrip();
        test_cache_hit_miss_tracking();

        std::cout << "\n=== GPU Validation Tests ===\n\n";
        test_analyze_empty_data();
        test_analyze_all_zero();
        test_analyze_all_white();
        test_analyze_gradient();
        test_analyze_pixel_formats();
        test_validate_black_rejected();
        test_validate_black_allowed();
        test_validate_constant_rejected();
        test_validate_constant_allowed_non_final();
        test_validate_nan_rejected();
        test_validate_inf_rejected();
        test_validate_no_data();
        test_permissive_allows_black();
        test_strict_rejects_black();
        test_diagnostic_format_pass();
        test_diagnostic_format_fail();
        test_diagnostic_skip();
        test_gpu_test_result_names();
        test_set_get_profile();
        test_run_gpu_test_no_data();
        test_run_gpu_test_with_data();
        test_run_gpu_test_gradient_rejects_constant();
        test_profile_minimums();
        test_profile_minimums_just_passes();
        test_constant_output_allow();
        test_unique_colors_counting();

        std::cout << "\n";
#ifdef RUN_PRODUCTION_HARDENING
        monix::renderer_vk::tests::runAll();
#endif

        std::cout << "\n=== All tests passed ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest suite failed: " << e.what() << "\n";
        return 1;
    }
}
