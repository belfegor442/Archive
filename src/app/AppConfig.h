#pragma once

#include <string>

namespace archive::app {

struct AppConfig {
    std::string data_dir;
    std::string db_path;
    std::string items_dir;

    static AppConfig default_config();
    void ensure_directories() const;
};

} // namespace archive::app
