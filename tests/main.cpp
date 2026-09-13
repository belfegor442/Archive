#include "test_helpers.h"

void run_enum_tests();
void run_model_tests();
void run_type_tests();
void run_storage_tests();
void run_hashing_tests();

int main() {
    run_enum_tests();
    run_model_tests();
    run_type_tests();
    run_storage_tests();
    run_hashing_tests();

    return ::test::test_summary();
}
