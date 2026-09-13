#include "test_helpers.h"

void run_enum_tests();
void run_model_tests();
void run_type_tests();

int main() {
    run_enum_tests();
    run_model_tests();
    run_type_tests();

    return ::test::test_summary();
}
