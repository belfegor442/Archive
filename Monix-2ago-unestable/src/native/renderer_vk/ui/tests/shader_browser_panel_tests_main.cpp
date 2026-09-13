#include "shader_browser_panel_tests.hpp"

#include <cstdio>

int main() {
    std::printf("=== ShaderBrowserPanel Test Runner ===\n\n");
    monix::renderer_vk::tests::runShaderBrowserPanelTests();
    std::printf("\n=== All ShaderBrowserPanel tests complete ===\n");
    return 0;
}
