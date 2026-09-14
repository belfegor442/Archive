#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace archive::filesystem {

class FilesystemTracker {
public:
    FilesystemTracker() = default;
    ~FilesystemTracker() = default;

    FilesystemTracker(const FilesystemTracker&) = delete;
    FilesystemTracker& operator=(const FilesystemTracker&) = delete;

    void track_created_dir(const std::string& path);
    void track_created_file(const std::string& path);
    void track_copied_file(const std::string& dest_path);
    void track_replaced_file(const std::string& dest_path);

    void compensate();

    bool has_operations() const;

    const std::vector<std::string>& created_dirs() const { return created_dirs_; }
    const std::vector<std::string>& created_files() const { return created_files_; }
    const std::vector<std::string>& copied_files() const { return copied_files_; }
    const std::vector<std::string>& replaced_files() const { return replaced_files_; }

    void clear();

private:
    std::vector<std::string> created_dirs_;
    std::vector<std::string> created_files_;
    std::vector<std::string> copied_files_;
    std::vector<std::string> replaced_files_;

    static void remove_path_safe(const std::string& path);
};

} // namespace archive::filesystem
