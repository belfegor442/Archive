#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>

namespace archive::filesystem {

struct StagingOperation {
    std::string operation_id;
    std::string staging_path;
    std::string operation_type;
    std::string created_at;
    bool completed = false;
};

class StagingManager {
public:
    StagingManager(const std::string& base_dir);

    std::string create_staging_dir(const std::string& operation_type);
    std::string get_staging_path(const std::string& operation_id) const;
    std::string get_staging_files_path(const std::string& operation_id) const;

    std::string stage_file(const std::string& operation_id, const std::string& source_path);
    std::string stage_file_in_dir(const std::string& operation_id, const std::string& source_path,
                                  const std::string& relative_path);
    std::string stage_folder(const std::string& operation_id, const std::string& source_dir);

    bool staging_dir_exists(const std::string& operation_id) const;

    void finalize_staging(const std::string& operation_id, const std::string& dest_dir);
    void cleanup_staging(const std::string& operation_id);

    std::vector<StagingOperation> detect_abandoned_staging() const;
    void cleanup_abandoned();

    static std::string generate_operation_id();

    std::string staging_base() const { return staging_base_; }

private:
    std::string staging_base_;

    void mark_operation_file(const std::string& operation_id, const std::string& type, bool completed);
};

} // namespace archive::filesystem
