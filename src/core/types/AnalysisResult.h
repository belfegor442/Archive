#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace archive::core {

struct AnalysisResult {
    int total_files = 0;
    int total_folders = 0;
    int64_t total_size = 0;
    std::map<std::string, int> by_extension;
    std::map<std::string, int> by_mime;
    std::map<std::string, int> by_project;
    std::vector<std::string> detected_groups;
    std::string root_path;
};

} // namespace archive::core
