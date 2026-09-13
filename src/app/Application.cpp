#include "Application.h"

#include <iostream>

namespace archive::app {

Application::Application(AppConfig config)
    : config_(std::move(config))
{}

int Application::run(int argc, char* argv[]) {
    config_.ensure_directories();

    std::cout << "Archive v0.1.0" << std::endl;
    std::cout << "Data directory: " << config_.data_dir << std::endl;
    std::cout << "Database: " << config_.db_path << std::endl;
    std::cout << "Storage: " << config_.items_dir << std::endl;

    return 0;
}

} // namespace archive::app
