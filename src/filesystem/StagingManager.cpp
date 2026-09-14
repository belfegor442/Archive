#include "StagingManager.h"
#include "FileUtils.h"
#include "../core/utils/Uuid.h"
#include "../hashing/FileHasher.h"

#include <fstream>
#include <sstream>
#include <iostream>

namespace archive::filesystem {

StagingManager::StagingManager(const std::string& base_dir)
    : staging_base_(base_dir + "/.staging")
{}

std::string StagingManager::create_staging_dir(const std::string& operation_type) {
    return create_staging_dir(operation_type, "");
}

std::string StagingManager::create_staging_dir(const std::string& operation_type,
                                               const std::string& item_id) {
    std::string op_id = generate_operation_id();
    std::string path = get_staging_path(op_id);

    FileUtils::create_directories(path);
    FileUtils::create_directories(path + "/files");
    FileUtils::create_directories(path + "/.meta");

    StagingOperation meta;
    meta.operation_id = op_id;
    meta.staging_path = path;
    meta.operation_type = operation_type;
    meta.state = state_to_string(StagingState::Preparing);
    meta.created_at = core::utils::now_iso();
    meta.item_id = item_id;

    write_metadata(op_id, meta);

    return op_id;
}

std::string StagingManager::get_staging_path(const std::string& operation_id) const {
    return staging_base_ + "/" + operation_id;
}

std::string StagingManager::get_staging_files_path(const std::string& operation_id) const {
    return get_staging_path(operation_id) + "/files";
}

std::string StagingManager::stage_file(const std::string& operation_id, const std::string& source_path) {
    std::string files_dir = get_staging_files_path(operation_id);
    return FileUtils::copy_file_safe(source_path, files_dir);
}

std::string StagingManager::stage_file_in_dir(const std::string& operation_id,
                                               const std::string& source_path,
                                               const std::string& relative_path) {
    std::string sanitized = FileUtils::sanitize_relative_path(relative_path);
    std::string files_dir = get_staging_files_path(operation_id);
    std::string dest_path = files_dir + "/" + sanitized;
    if (!FileUtils::is_path_within(dest_path, files_dir)) {
        throw std::runtime_error("Path traversal rejected in staging: " + relative_path);
    }
    std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());
    FileUtils::copy_file(source_path, dest_path);
    return dest_path;
}

std::string StagingManager::stage_folder(const std::string& operation_id, const std::string& source_dir) {
    std::string files_dir = get_staging_files_path(operation_id);
    std::error_code ec;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(
            source_dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
        std::error_code status_ec;
        auto status = entry.status(status_ec);
        if (status_ec) continue;

        if (status.type() == std::filesystem::file_type::symlink) {
            continue;
        }

        if (entry.is_regular_file()) {
            std::string relative = std::filesystem::relative(entry.path(), source_dir).string();
            std::string dest_path = files_dir + "/" + relative;
            std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());
            FileUtils::copy_file(entry.path().string(), dest_path);
        }
    }

    if (ec) {
        throw std::runtime_error("Error staging folder: " + ec.message());
    }

    return files_dir;
}

bool StagingManager::staging_dir_exists(const std::string& operation_id) const {
    return std::filesystem::exists(get_staging_path(operation_id));
}

bool StagingManager::validate_staging(const std::string& operation_id, const std::string& dest_dir,
                                       CollisionPolicy policy) {
    std::string staging_files = get_staging_files_path(operation_id);
    if (!std::filesystem::exists(staging_files)) return false;

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(staging_files, ec)) {
        if (entry.is_regular_file()) {
            std::string relative = FileUtils::sanitize_relative_path(
                std::filesystem::relative(entry.path(), staging_files).string());
            std::string dest_path = dest_dir + "/" + relative;

            if (std::filesystem::exists(dest_path)) {
                switch (policy) {
                    case CollisionPolicy::Reject:
                        return false;
                    case CollisionPolicy::SkipIfIdentical: {
                        std::string dest_checksum = compute_file_checksum(dest_path);
                        std::string src_checksum = compute_file_checksum(entry.path().string());
                        if (dest_checksum != src_checksum) {
                            return false;
                        }
                        break;
                    }
                    case CollisionPolicy::BackupAndReplace:
                        break;
                }
            }
        }
    }
    return true;
}

void StagingManager::finalize_staging(const std::string& operation_id, const std::string& dest_dir,
                                       CollisionPolicy policy) {
    std::string staging_files = get_staging_files_path(operation_id);
    if (!std::filesystem::exists(staging_files)) {
        return;
    }

    if (!validate_staging(operation_id, dest_dir, policy)) {
        throw std::runtime_error("Staging validation failed for operation: " + operation_id);
    }

    mark_finalizing(operation_id);

    std::map<std::string, std::string> local_backup_map;
    std::error_code ec;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(staging_files, ec)) {
        if (entry.is_regular_file()) {
            std::string relative = FileUtils::sanitize_relative_path(
                std::filesystem::relative(entry.path(), staging_files).string());
            std::string dest_path = dest_dir + "/" + relative;
            std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());

            if (std::filesystem::exists(dest_path)) {
                switch (policy) {
                    case CollisionPolicy::Reject:
                        throw std::runtime_error("File collision: " + relative);
                    case CollisionPolicy::SkipIfIdentical: {
                        std::string dest_checksum = compute_file_checksum(dest_path);
                        std::string src_checksum = compute_file_checksum(entry.path().string());
                        if (dest_checksum == src_checksum) {
                            continue;
                        }
                        backup_file(dest_path, dest_path + ".backup");
                        local_backup_map[dest_path] = dest_path + ".backup";
                        std::filesystem::copy_file(entry.path(), dest_path,
                            std::filesystem::copy_options::overwrite_existing, ec);
                        break;
                    }
                    case CollisionPolicy::BackupAndReplace:
                        backup_file(dest_path, dest_path + ".backup");
                        local_backup_map[dest_path] = dest_path + ".backup";
                        std::filesystem::copy_file(entry.path(), dest_path,
                            std::filesystem::copy_options::overwrite_existing, ec);
                        break;
                }
            } else {
                std::filesystem::copy_file(entry.path(), dest_path, ec);
            }

            if (ec) {
                throw std::runtime_error("Failed to finalize file: " + relative + " (" + ec.message() + ")");
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(backup_mutex_);
        backup_maps_[operation_id] = local_backup_map;
    }

    verify_finalized(operation_id, dest_dir);
    cleanup_backups(operation_id);
    mark_committed(operation_id);
}

void StagingManager::rollback_staging(const std::string& operation_id) {
    restore_backups(operation_id);
    mark_rolled_back(operation_id);
    cleanup_staging(operation_id);
}

void StagingManager::cleanup_staging(const std::string& operation_id) {
    std::string path = get_staging_path(operation_id);
    if (std::filesystem::exists(path)) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
}

void StagingManager::mark_staged(const std::string& operation_id, const std::string& version_id,
                                  const std::string& checksum) {
    std::map<std::string, std::string> updates;
    updates["state"] = state_to_string(StagingState::Staged);
    if (!version_id.empty()) updates["version_id"] = version_id;
    if (!checksum.empty()) updates["expected_checksum"] = checksum;
    update_metadata(operation_id, updates);
}

void StagingManager::mark_finalizing(const std::string& operation_id) {
    update_metadata(operation_id, {{"state", state_to_string(StagingState::Finalizing)}});
}

void StagingManager::mark_committed(const std::string& operation_id) {
    update_metadata(operation_id, {{"state", state_to_string(StagingState::Committed)}});
}

void StagingManager::mark_rolled_back(const std::string& operation_id) {
    update_metadata(operation_id, {{"state", state_to_string(StagingState::RolledBack)}});
}

void StagingManager::mark_abandoned(const std::string& operation_id) {
    update_metadata(operation_id, {{"state", state_to_string(StagingState::Abandoned)}});
}

std::vector<StagingOperation> StagingManager::detect_abandoned_staging() const {
    std::vector<StagingOperation> abandoned;

    if (!std::filesystem::exists(staging_base_)) {
        return abandoned;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(staging_base_, ec)) {
        if (entry.is_directory()) {
            std::string op_id = entry.path().filename().string();
            StagingOperation op = read_metadata(op_id);

            if (op.state != state_to_string(StagingState::Committed)) {
                abandoned.push_back(std::move(op));
            }
        }
    }

    return abandoned;
}

std::vector<StagingOperation> StagingManager::detect_corrupted_staging() const {
    std::vector<StagingOperation> corrupted;

    if (!std::filesystem::exists(staging_base_)) {
        return corrupted;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(staging_base_, ec)) {
        if (entry.is_directory()) {
            std::string op_id = entry.path().filename().string();
            StagingOperation op = read_metadata(op_id);

            if (op.state == state_to_string(StagingState::Finalizing) ||
                op.state.empty()) {
                corrupted.push_back(std::move(op));
            }
        }
    }

    return corrupted;
}

void StagingManager::cleanup_abandoned() {
    auto operations = detect_abandoned_staging();
    for (const auto& op : operations) {
        if (op.state == state_to_string(StagingState::Committed) ||
            op.state == state_to_string(StagingState::RolledBack)) {
            cleanup_staging(op.operation_id);
        }
    }
}

std::string StagingManager::generate_operation_id() {
    return "op_" + core::utils::generate_id();
}

std::string StagingManager::state_to_string(StagingState state) {
    switch (state) {
        case StagingState::Preparing:   return "preparing";
        case StagingState::Staged:      return "staged";
        case StagingState::Finalizing:  return "finalizing";
        case StagingState::Committed:   return "committed";
        case StagingState::RollingBack: return "rolling_back";
        case StagingState::RolledBack:  return "rolled_back";
        case StagingState::Abandoned:   return "abandoned";
        case StagingState::Corrupted:   return "corrupted";
    }
    return "unknown";
}

StagingState StagingManager::string_to_state(const std::string& str) {
    if (str == "preparing")    return StagingState::Preparing;
    if (str == "staged")       return StagingState::Staged;
    if (str == "finalizing")   return StagingState::Finalizing;
    if (str == "committed")    return StagingState::Committed;
    if (str == "rolling_back") return StagingState::RollingBack;
    if (str == "rolled_back")  return StagingState::RolledBack;
    if (str == "abandoned")    return StagingState::Abandoned;
    if (str == "corrupted")    return StagingState::Corrupted;
    return StagingState::Corrupted;
}

void StagingManager::restore_backups(const std::string& operation_id) {
    std::map<std::string, std::string> backup_map;
    {
        std::lock_guard<std::mutex> lock(backup_mutex_);
        auto it = backup_maps_.find(operation_id);
        if (it != backup_maps_.end()) {
            backup_map = it->second;
            backup_maps_.erase(it);
        }
    }

    for (auto it = backup_map.rbegin(); it != backup_map.rend(); ++it) {
        restore_file(it->second, it->first);
    }
}

void StagingManager::cleanup_backups(const std::string& operation_id) {
    std::map<std::string, std::string> backup_map;
    {
        std::lock_guard<std::mutex> lock(backup_mutex_);
        auto it = backup_maps_.find(operation_id);
        if (it != backup_maps_.end()) {
            backup_map = it->second;
            backup_maps_.erase(it);
        }
    }

    for (const auto& [dest, backup] : backup_map) {
        std::error_code ec;
        std::filesystem::remove(backup, ec);
    }
}

void StagingManager::verify_finalized(const std::string& operation_id, const std::string& dest_dir) {
    std::string staging_files = get_staging_files_path(operation_id);
    if (!std::filesystem::exists(staging_files)) return;

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(staging_files, ec)) {
        if (entry.is_regular_file()) {
            std::string relative = FileUtils::sanitize_relative_path(
                std::filesystem::relative(entry.path(), staging_files).string());
            std::string dest_path = dest_dir + "/" + relative;

            if (!std::filesystem::exists(dest_path)) {
                throw std::runtime_error("Verify failed: destination missing after copy: " + relative);
            }

            uint64_t src_size = entry.file_size(ec);
            uint64_t dest_size = std::filesystem::file_size(dest_path, ec);
            if (src_size != dest_size) {
                throw std::runtime_error("Verify failed: size mismatch for " + relative
                    + " (expected " + std::to_string(src_size) + ", got " + std::to_string(dest_size) + ")");
            }

            std::string src_hash = compute_file_checksum(entry.path().string());
            std::string dest_hash = compute_file_checksum(dest_path);
            if (!src_hash.empty() && !dest_hash.empty() && src_hash != dest_hash) {
                throw std::runtime_error("Verify failed: checksum mismatch for " + relative);
            }
        }
    }
}

void StagingManager::write_metadata(const std::string& operation_id, const StagingOperation& meta) {
    std::string meta_file = get_staging_path(operation_id) + "/.meta/operation.json";
    std::ofstream f(meta_file);
    if (f.is_open()) {
        f << "{\n";
        f << "  \"operation_id\": \"" << meta.operation_id << "\",\n";
        f << "  \"operation_type\": \"" << meta.operation_type << "\",\n";
        f << "  \"state\": \"" << meta.state << "\",\n";
        f << "  \"created_at\": \"" << meta.created_at << "\",\n";
        f << "  \"item_id\": \"" << meta.item_id << "\",\n";
        f << "  \"version_id\": \"" << meta.version_id << "\",\n";
        f << "  \"expected_checksum\": \"" << meta.expected_checksum << "\"\n";
        f << "}\n";
    }
}

StagingOperation StagingManager::read_metadata(const std::string& operation_id) const {
    StagingOperation op;
    op.operation_id = operation_id;
    op.staging_path = get_staging_path(operation_id);

    std::string meta_file = op.staging_path + "/.meta/operation.json";

    if (!std::filesystem::exists(meta_file)) {
        std::string legacy_file = op.staging_path + "/.meta/operation.txt";
        if (std::filesystem::exists(legacy_file)) {
            std::ifstream f(legacy_file);
            if (f.is_open()) {
                std::getline(f, op.operation_type);
                std::string completed_str;
                std::getline(f, completed_str);
                op.state = (completed_str == "1") ?
                    state_to_string(StagingState::Committed) :
                    state_to_string(StagingState::Abandoned);
            }
        }
        return op;
    }

    std::ifstream f(meta_file);
    if (f.is_open()) {
        std::string line;
        while (std::getline(f, line)) {
            auto pos = line.find(':');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            auto trim = [](std::string s) {
                while (!s.empty() && (s.front() == ' ' || s.front() == '"')) s.erase(s.begin());
                while (!s.empty() && (s.back() == ' ' || s.back() == '"' || s.back() == ',' || s.back() == '}')) s.pop_back();
                return s;
            };

            key = trim(key);
            value = trim(value);

            if (key == "operation_type") op.operation_type = value;
            else if (key == "state") op.state = value;
            else if (key == "created_at") op.created_at = value;
            else if (key == "item_id") op.item_id = value;
            else if (key == "version_id") op.version_id = value;
            else if (key == "expected_checksum") op.expected_checksum = value;
        }
    }

    return op;
}

void StagingManager::update_metadata(const std::string& operation_id,
                                      const std::map<std::string, std::string>& updates) {
    StagingOperation meta = read_metadata(operation_id);
    for (const auto& [key, value] : updates) {
        if (key == "state") meta.state = value;
        else if (key == "version_id") meta.version_id = value;
        else if (key == "expected_checksum") meta.expected_checksum = value;
        else if (key == "item_id") meta.item_id = value;
    }
    write_metadata(operation_id, meta);
}

void StagingManager::backup_file(const std::string& src, const std::string& dest) {
    std::error_code ec;
    if (std::filesystem::exists(src)) {
        std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
    }
}

void StagingManager::restore_file(const std::string& backup, const std::string& original) {
    std::error_code ec;
    if (std::filesystem::exists(backup)) {
        std::filesystem::copy_file(backup, original, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(backup, ec);
    }
}

std::string StagingManager::compute_file_checksum(const std::string& path) {
    try {
        return hashing::FileHasher::hash_file(path);
    } catch (...) {
        return "";
    }
}

} // namespace archive::filesystem
