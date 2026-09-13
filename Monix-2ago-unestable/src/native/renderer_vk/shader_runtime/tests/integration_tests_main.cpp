#include <iostream>
#include "renderer_vk/shader_runtime/tests/real_shader_integration_tests.hpp"

int main() {
    printf("FASE8 TEST START\n");
    std::cout << "=== MONIX FASE 8: Real Shader Integration Tests ===\n\n";
    try {
        monix::renderer_vk::tests::runAll();
        std::cout << "\n=== All FASE 8 tests passed ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest suite failed: " << e.what() << "\n";
        return 1;
    }
}
