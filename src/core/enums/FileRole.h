#pragma once

#include <string>
#include <stdexcept>

namespace archive::core {

enum class FileRole { Source, Header, Script, Config, Build, Documentation, Asset, Data, Binary, Unknown };

inline std::string to_string(FileRole role) {
    switch (role) {
        case FileRole::Source:        return "Source";
        case FileRole::Header:        return "Header";
        case FileRole::Script:        return "Script";
        case FileRole::Config:        return "Config";
        case FileRole::Build:         return "Build";
        case FileRole::Documentation: return "Documentation";
        case FileRole::Asset:         return "Asset";
        case FileRole::Data:          return "Data";
        case FileRole::Binary:        return "Binary";
        case FileRole::Unknown:       return "Unknown";
    }
    throw std::invalid_argument("Unknown FileRole");
}

inline FileRole file_role_from_string(const std::string& s) {
    if (s == "Source")        return FileRole::Source;
    if (s == "Header")        return FileRole::Header;
    if (s == "Script")        return FileRole::Script;
    if (s == "Config")        return FileRole::Config;
    if (s == "Build")         return FileRole::Build;
    if (s == "Documentation") return FileRole::Documentation;
    if (s == "Asset")         return FileRole::Asset;
    if (s == "Data")          return FileRole::Data;
    if (s == "Binary")        return FileRole::Binary;
    if (s == "Unknown")       return FileRole::Unknown;
    throw std::invalid_argument("Unknown FileRole: " + s);
}

} // namespace archive::core
