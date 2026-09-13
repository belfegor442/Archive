#include <cstdio>
#include "renderer_vk/library/tests/shader_library_tests.hpp"

int main() {
    printf("=== ShaderLibrary Test Runner ===\n");
    fflush(stdout);
    monix::renderer_vk::runAllShaderLibraryTests();
    printf("All done.\n");
    fflush(stdout);
    return 0;
}
