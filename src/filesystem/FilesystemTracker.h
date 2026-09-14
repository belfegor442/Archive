#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <map>

namespace archive::filesystem {

class FilesystemTracker {
public:
    explicit FilesystemTracker(const std::string& backup_dir = "");
    ~FilesystemTracker() = default;

    FilesystemTracker(const FilesystemTracker&) = delete;
    FilesystemTracker& operator=(const FilesystemTracker&) = delete;

    void track_created_dir(const std::string& path);
    void track_created_file(const std::string& path);
    void track_copied_file(const std::string& dest_path);
    void track_replaced_file(const std::string& dest_path, const std::string& original_path);

    void compensate();
    void mark_success();

    bool has_operations() const;

    const std::vector<std::string>& created_dirs() const { return created_dirs_; }
    const std::vector<std::string>& created_files() const { return created_files_; }
    const std::vector<std::string>& copied_files() const { return copied_files_; }
    const std::vector<std::string>& replaced_files() const { return replaced_files_; }

    void clear();

private:
    std::string backup_dir_;
    std::vector<std::string> created_dirs_;
    std::vector<std::string> created_files_;
    std::vector<std::string> copied_files_;
    std::vector<std::string> replaced_files_;
    std::map<std::string, std::string> backup_map_;

    void backup_original(const std::string& dest_path, const std::string& original_path);
    void restore_replaced();
    void remove_backup_dir();

    static void remove_path_safe(const std::string& path);
    static bool is_symlink_or_junction(const std::string& path);
};

} // namespace archive::filesystem
