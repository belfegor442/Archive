#include "transactional_swap_tests.hpp"
#include <cstdlib>

int main() {
    try {
        return monix::renderer_vk::tests::TransactionalSwapTests::runAll();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 1;
    }
}
