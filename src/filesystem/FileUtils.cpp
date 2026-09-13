#include "FileUtils.h"

#include <stdexcept>
#include <sstream>

namespace archive::filesystem {

bool FileUtils::file_exists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool FileUtils::directory_exists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

uint64_t FileUtils::file_size(const std::string& path) {
    return std::filesystem::file_size(path);
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
    std::filesystem::create_directories(path, ec);
    if (ec) {
        throw std::runtime_error("Failed to create directories: " + path + " (" + ec.message() + ")");
    }
}

void FileUtils::copy_file(const std::string& source, const std::string& dest) {
    std::error_code ec;
    std::filesystem::copy_file(source, dest, std::filesystem::copy_options::none, ec);
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
    std::filesystem::rename(source, dest, ec);
    if (ec) {
        throw std::runtime_error("Failed to move file: " + source + " -> " + dest + " (" + ec.message() + ")");
    }
}

void FileUtils::remove_file(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("Failed to remove file: " + path + " (" + ec.message() + ")");
    }
}

void FileUtils::remove_directory(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
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

} // namespace archive::filesystem
