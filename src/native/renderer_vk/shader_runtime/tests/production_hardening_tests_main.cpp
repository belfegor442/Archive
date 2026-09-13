#include <iostream>
#include "renderer_vk/shader_runtime/tests/production_hardening_tests.hpp"

int main() {
    printf("PROD TEST START\n");
    std::cout << "=== MONIX Production Hardening Tests ===\n\n";
    try {
        monix::renderer_vk::tests::runAll();
        std::cout << "\n=== All production hardening tests passed ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest suite failed: " << e.what() << "\n";
        return 1;
    }
}
