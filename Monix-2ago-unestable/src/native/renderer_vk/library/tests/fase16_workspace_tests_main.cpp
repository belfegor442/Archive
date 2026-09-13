#include "fase16_workspace_tests.hpp"
#include <cstdio>

int main() {
    std::printf("=== FASE 16 Workspace Test Runner ===\n\n");
    monix::renderer_vk::tests::runFase16WorkspaceTests();
    std::printf("\n=== FASE 16 Workspace tests complete ===\n");
    return 0;
}
