#include "AppConfig.h"

#include <cstdlib>
#include <filesystem>

namespace archive::app {

static std::string get_home_dir() {
    const char* home = std::getenv("HOME");
    if (home) return home;

    home = std::getenv("USERPROFILE");
    if (home) return home;

    return ".";
}

AppConfig AppConfig::default_config() {
    AppConfig config;
    config.data_dir = get_home_dir() + "/.archive-data";
    config.db_path = config.data_dir + "/archive.db";
    config.items_dir = config.data_dir + "/items";
    return config;
}

void AppConfig::ensure_directories() const {
    std::filesystem::create_directories(data_dir);
    std::filesystem::create_directories(items_dir);
}

} // namespace archive::app
