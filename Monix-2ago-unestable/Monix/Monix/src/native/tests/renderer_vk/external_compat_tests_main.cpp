#include "external_compat_tests.hpp"
#include <cstdio>

int main() {
    std::printf("=== FASE 17 External Shader Compatibility Tests ===\n\n");
    monix::renderer_vk::tests::runExternalCompatTests();
    std::printf("\n=== FASE 17 External Compatibility tests complete ===\n");
    return 0;
}
