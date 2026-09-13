#include <iostream>
#include "renderer_vk/tests/fase13_validation_tests.hpp"

int main() {
    printf("FASE13 VALIDATION TEST START\n");
    std::cout << "=== FASE 13: Vulkan Validation & Runtime Hardening ===\n\n";
    try {
        monix::renderer_vk::tests::runAllFase13ValidationTests();
        std::cout << "\n=== FASE 13 validation complete ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nFASE 13 test suite failed: " << e.what() << "\n";
        return 1;
    }
}
