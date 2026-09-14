#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <map>

namespace archive::filesystem {

enum class StagingState {
    Preparing,
    Staged,
    Finalizing,
    Committed,
    RollingBack,
    RolledBack,
    Abandoned,
    Corrupted
};

enum class CollisionPolicy {
    Reject,
    BackupAndReplace,
    SkipIfIdentical
};

struct StagingOperation {
    std::string operation_id;
    std::string staging_path;
    std::string operation_type;
    std::string state;
    std::string created_at;
    std::string item_id;
    std::string version_id;
    std::string expected_checksum;
    std::vector<std::string> staged_files;
};

struct BackupEntry {
    std::string destination;
    std::string backup_path;
    std::string state;
};

class StagingManager {
public:
    StagingManager(const std::string& base_dir);

    std::string create_staging_dir(const std::string& operation_type);
    std::string create_staging_dir(const std::string& operation_type, const std::string& item_id);
    std::string get_staging_path(const std::string& operation_id) const;
    std::string get_staging_files_path(const std::string& operation_id) const;

    std::string stage_file(const std::string& operation_id, const std::string& source_path);
    std::string stage_file_in_dir(const std::string& operation_id, const std::string& source_path,
                                  const std::string& relative_path);
    std::string stage_folder(const std::string& operation_id, const std::string& source_dir);

    bool staging_dir_exists(const std::string& operation_id) const;

    bool validate_staging(const std::string& operation_id, const std::string& dest_dir,
                          CollisionPolicy policy = CollisionPolicy::Reject);
    void finalize_staging(const std::string& operation_id, const std::string& dest_dir,
                          CollisionPolicy policy = CollisionPolicy::Reject);
    void rollback_staging(const std::string& operation_id);
    void cleanup_staging(const std::string& operation_id);

    void mark_staged(const std::string& operation_id, const std::string& version_id = "",
                     const std::string& checksum = "");
    void mark_finalizing(const std::string& operation_id);
    void mark_committed(const std::string& operation_id);
    void mark_rolling_back(const std::string& operation_id);
    void mark_rolled_back(const std::string& operation_id);
    void mark_abandoned(const std::string& operation_id);

    std::vector<StagingOperation> detect_abandoned_staging() const;
    std::vector<StagingOperation> detect_corrupted_staging() const;
    void cleanup_abandoned();
    void cleanup_committed();

    static std::string generate_operation_id();
    static std::string state_to_string(StagingState state);
    static StagingState string_to_state(const std::string& str);

    std::string staging_base() const { return staging_base_; }

    std::vector<BackupEntry> read_backup_journal(const std::string& operation_id) const;
    void write_backup_journal(const std::string& operation_id,
                              const std::vector<BackupEntry>& entries);
    void restore_backups(const std::string& operation_id);
    void cleanup_backups(const std::string& operation_id);
    void verify_finalized(const std::string& operation_id, const std::string& dest_dir);

    void backup_file_strict(const std::string& src, const std::string& dest);
    void restore_file_strict(const std::string& backup, const std::string& original);
    static std::string compute_file_checksum(const std::string& path);

private:
    std::string staging_base_;

    void write_metadata(const std::string& operation_id, const StagingOperation& meta);
    StagingOperation read_metadata(const std::string& operation_id) const;
    void update_metadata(const std::string& operation_id,
                         const std::map<std::string, std::string>& updates);

    std::string get_backup_path(const std::string& operation_id,
                                const std::string& relative) const;
    void append_backup_entry(const std::string& operation_id, const BackupEntry& entry);
};

} // namespace archive::filesystem
