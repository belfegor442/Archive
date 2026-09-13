#include "FileUtils.h"

#include <stdexcept>

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

void FileUtils::create_directories(const std::string& path) {
    std::filesystem::create_directories(path);
}

void FileUtils::copy_file(const std::string& source, const std::string& dest) {
    std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);
}

void FileUtils::move_file(const std::string& source, const std::string& dest) {
    std::filesystem::rename(source, dest);
}

void FileUtils::remove_file(const std::string& path) {
    std::filesystem::remove(path);
}

void FileUtils::remove_directory(const std::string& path) {
    std::filesystem::remove_all(path);
}

int FileUtils::count_files(const std::string& path) {
    int count = 0;
    if (std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) count++;
        }
    }
    return count;
}

} // namespace archive::filesystem
