#include <cstdio>
#include "renderer_vk/library/tests/shader_library_compiler_tests.hpp"

int main() {
    printf("=== ShaderLibraryCompiler Test Runner ===\n");
    fflush(stdout);
    monix::renderer_vk::runAllShaderLibraryCompilerTests();
    printf("All done.\n");
    fflush(stdout);
    return 0;
}
