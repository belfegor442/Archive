#include <iostream>
#include "renderer_vk/tests/gpu_validation_tests.hpp"

int main() {
    printf("FASE12 GPU TEST START\n");
    std::cout << "=== FASE 12: Real GPU Validation ===\n\n";
    try {
        monix::renderer_vk::tests::runAllGpuValidationTests();
        std::cout << "\n=== FASE 12 GPU validation complete ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nGPU test suite failed: " << e.what() << "\n";
        return 1;
    }
}
