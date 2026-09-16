#include "test_helpers.h"

void run_enum_tests();
void run_model_tests();
void run_type_tests();
void run_storage_tests();
void run_hashing_tests();
void run_service_tests();
void run_integration_tests();
void run_atomic_tests();
void run_new_enum_tests();
void run_new_model_tests();
void run_new_storage_tests();
void run_classifier_tests();
void run_scanner_tests();
void run_planner_tests();
void run_executor_tests();
void run_file_analysis_tests();
void run_organize_flow_tests();

int main() {
    run_enum_tests();
    run_model_tests();
    run_type_tests();
    run_storage_tests();
    run_hashing_tests();
    run_service_tests();
    run_integration_tests();
    run_atomic_tests();
    run_new_enum_tests();
    run_new_model_tests();
    run_new_storage_tests();
    run_classifier_tests();
    run_scanner_tests();
    run_planner_tests();
    run_executor_tests();
    run_file_analysis_tests();
    run_organize_flow_tests();

    return ::test::test_summary();
}
