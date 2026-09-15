#pragma once

#include <string>
#include <cstdint>

namespace archive::core {

struct ScanItem {
    std::string id;
    std::string scan_id;
    std::string path;
    std::string filename;
    std::string extension;
    std::string mime_type;
    int64_t size = 0;
    std::string role;
    std::string detected_project;
    std::string content_preview;
    std::string checksum;
    std::string created_at;
    std::string modified_at;

    ScanItem() = default;
};

} // namespace archive::core
