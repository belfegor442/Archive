#include "FilesystemTracker.h"

#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace archive::filesystem {

FilesystemTracker::FilesystemTracker(const std::string& backup_dir)
    : backup_dir_(backup_dir)
{}

void FilesystemTracker::track_created_dir(const std::string& path) {
    created_dirs_.push_back(path);
}

void FilesystemTracker::track_created_file(const std::string& path) {
    created_files_.push_back(path);
}

void FilesystemTracker::track_copied_file(const std::string& dest_path) {
    copied_files_.push_back(dest_path);
}

void FilesystemTracker::track_replaced_file(const std::string& dest_path, const std::string& original_path) {
    replaced_files_.push_back(dest_path);
    backup_original(dest_path, original_path);
}

void FilesystemTracker::compensate() {
    restore_replaced();

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
    backup_map_.clear();

    remove_backup_dir();
}

void FilesystemTracker::mark_success() {
    replaced_files_.clear();
    backup_map_.clear();
    remove_backup_dir();
}

bool FilesystemTracker::has_operations() const {
    return !created_dirs_.empty() || !created_files_.empty() || !copied_files_.empty()
        || !replaced_files_.empty();
}

void FilesystemTracker::clear() {
    created_dirs_.clear();
    created_files_.clear();
    copied_files_.clear();
    replaced_files_.clear();
    backup_map_.clear();
}

void FilesystemTracker::backup_original(const std::string& dest_path, const std::string& original_path) {
    if (backup_dir_.empty()) return;
    if (!std::filesystem::exists(original_path)) return;

    std::error_code ec;
    std::string safe_name;
    for (char c : dest_path) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') {
            safe_name += '_';
        } else {
            safe_name += c;
        }
    }
    auto backup_path = std::filesystem::path(backup_dir_) / safe_name;

    std::filesystem::create_directories(backup_dir_, ec);
    if (ec) return;

    std::filesystem::copy_file(original_path, backup_path,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (!ec) {
        backup_map_[dest_path] = backup_path.string();
    }
}

void FilesystemTracker::restore_replaced() {
    for (auto it = replaced_files_.rbegin(); it != replaced_files_.rend(); ++it) {
        auto backup_it = backup_map_.find(*it);
        if (backup_it != backup_map_.end()) {
            std::error_code ec;
            std::filesystem::remove(*it, ec);
            std::filesystem::copy_file(backup_it->second, *it,
                std::filesystem::copy_options::overwrite_existing, ec);
        }
    }
}

void FilesystemTracker::remove_backup_dir() {
    if (backup_dir_.empty()) return;
    std::error_code ec;
    std::filesystem::remove_all(backup_dir_, ec);
}

void FilesystemTracker::remove_path_safe(const std::string& path) {
    std::error_code ec;
    auto p = std::filesystem::path(path);

    if (is_symlink_or_junction(path)) {
        std::filesystem::remove(p, ec);
        return;
    }

    if (std::filesystem::is_regular_file(p, ec)) {
        std::filesystem::remove(p, ec);
    } else if (std::filesystem::is_directory(p, ec)) {
        std::filesystem::remove_all(p, ec);
    }
}

bool FilesystemTracker::is_symlink_or_junction(const std::string& path) {
    std::error_code ec;
    auto p = std::filesystem::path(path);
    if (std::filesystem::is_symlink(p, ec)) return true;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
        return true;
    }
#endif
    return false;
}

} // namespace archive::filesystem
