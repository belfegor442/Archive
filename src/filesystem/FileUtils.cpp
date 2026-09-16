#include "FileUtils.h"

#include <stdexcept>
#include <sstream>
#include <vector>

namespace archive::filesystem {

bool FileUtils::file_exists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool FileUtils::directory_exists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

uint64_t FileUtils::file_size(const std::string& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(long_path(path), ec);
    if (ec) return 0;
    return size;
}

std::string FileUtils::file_name(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

std::string FileUtils::extension(const std::string& path) {
    return std::filesystem::path(path).extension().string();
}

std::string FileUtils::parent_dir(const std::string& path) {
    auto parent = std::filesystem::path(path).parent_path();
    return parent.empty() ? "." : parent.string();
}

std::string FileUtils::stem(const std::string& path) {
    return std::filesystem::path(path).stem().string();
}

void FileUtils::create_directories(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(long_path(path), ec);
    if (ec) {
        throw std::runtime_error("Failed to create directories: " + path + " (" + ec.message() + ")");
    }
}

void FileUtils::copy_file(const std::string& source, const std::string& dest) {
    std::error_code ec;
    std::filesystem::copy_file(long_path(source), long_path(dest), std::filesystem::copy_options::none, ec);
    if (ec) {
        if (ec == std::errc::file_exists) {
            throw std::runtime_error("Destination already exists: " + dest);
        }
        throw std::runtime_error("Failed to copy file: " + source + " -> " + dest + " (" + ec.message() + ")");
    }
}

std::string FileUtils::copy_file_safe(const std::string& source, const std::string& dest_dir) {
    std::string name = file_name(source);
    std::string dest = unique_path(dest_dir, stem(source), extension(source));
    copy_file(source, dest);
    return dest;
}

void FileUtils::move_file(const std::string& source, const std::string& dest) {
    std::error_code ec;
    std::filesystem::rename(long_path(source), long_path(dest), ec);
    if (ec) {
        throw std::runtime_error("Failed to move file: " + source + " -> " + dest + " (" + ec.message() + ")");
    }
}

void FileUtils::remove_file(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(long_path(path), ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("Failed to remove file: " + path + " (" + ec.message() + ")");
    }
}

void FileUtils::remove_directory(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove_all(long_path(path), ec);
    if (ec) {
        throw std::runtime_error("Failed to remove directory: " + path + " (" + ec.message() + ")");
    }
}

int FileUtils::count_files(const std::string& path) {
    int count = 0;
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
            if (entry.is_regular_file()) count++;
        }
    }
    return count;
}

uint64_t FileUtils::total_size(const std::string& path) {
    uint64_t total = 0;
    std::error_code ec;
    if (std::filesystem::is_regular_file(path, ec)) {
        return std::filesystem::file_size(path, ec);
    }
    if (std::filesystem::is_directory(path, ec)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
            if (entry.is_regular_file()) {
                total += entry.file_size(ec);
            }
        }
    }
    return total;
}

std::string FileUtils::unique_path(const std::string& dir, const std::string& name,
                                   const std::string& ext) {
    std::string candidate = dir + "/" + name + ext;
    if (!std::filesystem::exists(candidate)) {
        return candidate;
    }
    for (int i = 1; i < 10000; i++) {
        candidate = dir + "/" + name + "_" + std::to_string(i) + ext;
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Could not generate unique path for: " + name + ext);
}

std::string FileUtils::sanitize_relative_path(const std::string& relative) {
    if (!relative.empty() && (relative[0] == '/' || relative[0] == '\\')) {
        throw std::runtime_error("Absolute path rejected in relative context: " + relative);
    }
    auto p = std::filesystem::path(relative);
    std::vector<std::string> parts;
    for (const auto& part : p) {
        std::string s = part.string();
        if (s == "." || s == "") continue;
        if (s == "..") {
            throw std::runtime_error("Path traversal detected: " + relative);
        }
        parts.push_back(s);
    }
    std::string result;
    for (const auto& part : parts) {
        if (!result.empty()) result += "/";
        result += part;
    }
    return result;
}

std::string FileUtils::sanitize_filename(const std::string& name) {
    std::string safe;
    safe.reserve(name.size());
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') {
            safe += '_';
        } else {
            safe += c;
        }
    }
    return safe;
}

bool FileUtils::is_path_within(const std::string& path, const std::string& base) {
    auto is_sep = [](char c) { return c == '/' || c == '\\'; };

    auto get_components = [&](const std::string& s) {
        std::vector<std::string> parts;
        std::string current;
        for (char c : s) {
            if (is_sep(c)) {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            parts.push_back(current);
        }

        std::vector<std::string> resolved;
        for (const auto& part : parts) {
            if (part == "..") {
                if (!resolved.empty()) resolved.pop_back();
            } else if (part != ".") {
                resolved.push_back(part);
            }
        }
        return resolved;
    };

    bool path_abs = !path.empty() && is_sep(path[0]);
    bool base_abs = !base.empty() && is_sep(base[0]);
    if (path_abs != base_abs) return false;

    auto path_parts = get_components(path);
    auto base_parts = get_components(base);

    if (path_parts.size() < base_parts.size()) return false;

    for (size_t i = 0; i < base_parts.size(); i++) {
        if (path_parts[i] != base_parts[i]) return false;
    }

    return true;
}

std::string FileUtils::long_path(const std::string& path) {
#ifdef _WIN32
    if (path.size() >= 240) {
        std::string p = path;
        for (auto& c : p) { if (c == '/') c = '\\'; }
        if (p.size() >= 2 && p[1] == ':') {
            return "\\\\?\\" + p;
        }
        if (p.size() >= 2 && p[0] == '\\' && p[1] == '\\') {
            return p;
        }
        std::error_code ec;
        auto abs = std::filesystem::absolute(p, ec).string();
        if (!ec) {
            for (auto& c : abs) { if (c == '/') c = '\\'; }
            return "\\\\?\\" + abs;
        }
    }
#endif
    return path;
}

} // namespace archive::filesystem
