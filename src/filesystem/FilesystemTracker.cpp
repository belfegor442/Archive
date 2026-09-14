#include "FilesystemTracker.h"

#include <iostream>
#include <algorithm>

namespace archive::filesystem {

void FilesystemTracker::track_created_dir(const std::string& path) {
    created_dirs_.push_back(path);
}

void FilesystemTracker::track_created_file(const std::string& path) {
    created_files_.push_back(path);
}

void FilesystemTracker::track_copied_file(const std::string& dest_path) {
    copied_files_.push_back(dest_path);
}

void FilesystemTracker::track_replaced_file(const std::string& dest_path) {
    replaced_files_.push_back(dest_path);
}

void FilesystemTracker::compensate() {
    for (auto it = copied_files_.rbegin(); it != copied_files_.rend(); ++it) {
        remove_path_safe(*it);
    }
    copied_files_.clear();

    for (auto it = created_files_.rbegin(); it != created_files_.rend(); ++it) {
        remove_path_safe(*it);
    }
    created_files_.clear();

    for (auto it = created_dirs_.rbegin(); it != created_dirs_.rend(); ++it) {
        remove_path_safe(*it);
    }
    created_dirs_.clear();

    replaced_files_.clear();
}

bool FilesystemTracker::has_operations() const {
    return !created_dirs_.empty() || !created_files_.empty() || !copied_files_.empty();
}

void FilesystemTracker::clear() {
    created_dirs_.clear();
    created_files_.clear();
    copied_files_.clear();
    replaced_files_.clear();
}

void FilesystemTracker::remove_path_safe(const std::string& path) {
    std::error_code ec;
    auto p = std::filesystem::path(path);

    if (std::filesystem::is_regular_file(p, ec)) {
        std::filesystem::remove(p, ec);
    } else if (std::filesystem::is_directory(p, ec)) {
        std::filesystem::remove_all(p, ec);
    }
}

} // namespace archive::filesystem
