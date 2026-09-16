#include "StagingManager.h"
#include "FileUtils.h"
#include "../core/utils/Uuid.h"
#include "../hashing/FileHasher.h"

#include <fstream>
#include <sstream>
#include <cstdio>

namespace archive::filesystem {

namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + s.size() / 4);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string json_unescape(const std::string& s, size_t& pos) {
    std::string out;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            pos++;
            switch (s[pos]) {
                case '"':  out += '"'; break;
                case '\\': out += '\\'; break;
                case '/':  out += '/'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    if (pos + 4 >= s.size()) {
                        throw std::runtime_error("Journal parse error: incomplete \\u escape at position " + std::to_string(pos - 1));
                    }
                    unsigned int cp = 0;
                    for (int i = 1; i <= 4; i++) {
                        char h = s[pos + i];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp += h - '0';
                        else if (h >= 'a' && h <= 'f') cp += 10 + h - 'a';
                        else if (h >= 'A' && h <= 'F') cp += 10 + h - 'A';
                        else {
                            throw std::runtime_error("Journal parse error: invalid hex digit '" + std::string(1, h) + "' in \\u escape at position " + std::to_string(pos + i));
                        }
                    }
                    if (cp < 0x80) {
                        out += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        out += static_cast<char>(0xC0 | (cp >> 6));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (cp >> 12));
                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    pos += 4;
                    break;
                }
                default:
                    throw std::runtime_error("Journal parse error: invalid escape sequence '\\" + std::string(1, s[pos]) + "' at position " + std::to_string(pos - 1));
            }
        } else {
            out += s[pos];
        }
        pos++;
    }
    return out;
}

void skip_whitespace(const std::string& s, size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\n' || s[pos] == '\r' || s[pos] == '\t')) {
        pos++;
    }
}

std::string parse_json_string(const std::string& s, size_t& pos) {
    skip_whitespace(s, pos);
    if (pos >= s.size() || s[pos] != '"') {
        throw std::runtime_error("Journal parse error: expected '\"' at position " + std::to_string(pos));
    }
    pos++;
    std::string value = json_unescape(s, pos);
    if (pos >= s.size() || s[pos] != '"') {
        throw std::runtime_error("Journal parse error: unterminated string at position " + std::to_string(pos));
    }
    pos++;
    return value;
}

std::string find_json_value(const std::string& obj, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t search_pos = 0;
    while (search_pos < obj.size()) {
        auto key_pos = obj.find(search, search_pos);
        if (key_pos == std::string::npos) {
            return "";
        }

        bool in_string = false;
        if (key_pos > 0) {
            char before = obj[key_pos - 1];
            if (before != ' ' && before != '\n' && before != '\r' && before != '\t' && before != ',' && before != '{') {
                search_pos = key_pos + 1;
                continue;
            }
        }

        size_t pos = key_pos + search.size();
        skip_whitespace(obj, pos);
        if (pos >= obj.size() || obj[pos] != ':') {
            search_pos = key_pos + 1;
            continue;
        }
        pos++;
        skip_whitespace(obj, pos);
        if (pos >= obj.size() || obj[pos] != '"') {
            search_pos = key_pos + 1;
            continue;
        }
        return parse_json_string(obj, pos);
    }
    return "";
}

} // anonymous namespace

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
    std::filesystem::create_directories(FileUtils::long_path(std::filesystem::path(dest_path).parent_path().string()));
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
            std::string relative = FileUtils::sanitize_relative_path(
                std::filesystem::relative(entry.path(), source_dir).string());
            std::string dest_path = files_dir + "/" + relative;
            if (!FileUtils::is_path_within(dest_path, files_dir)) {
                throw std::runtime_error("Path traversal rejected in stage_folder: " + relative);
            }
            std::filesystem::create_directories(FileUtils::long_path(std::filesystem::path(dest_path).parent_path().string()));
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

    try {
        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(staging_files, ec)) {
            if (entry.is_regular_file()) {
                std::string relative = FileUtils::sanitize_relative_path(
                    std::filesystem::relative(entry.path(), staging_files).string());
                std::string dest_path = dest_dir + "/" + relative;
                std::filesystem::create_directories(FileUtils::long_path(std::filesystem::path(dest_path).parent_path().string()));

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
                            throw std::runtime_error(
                                "SkipIfIdentical: destination differs from staged file: " + relative);
                        }
                        case CollisionPolicy::BackupAndReplace: {
                            std::string backup_path = get_backup_path(operation_id, relative);
                            backup_file_strict(dest_path, backup_path);

                            BackupEntry bk;
                            bk.destination = dest_path;
                            bk.backup_path = backup_path;
                            bk.state = "created";
                            append_backup_entry(operation_id, bk);

                            std::filesystem::copy_file(FileUtils::long_path(entry.path().string()), FileUtils::long_path(dest_path),
                                std::filesystem::copy_options::overwrite_existing, ec);
                            if (ec) {
                                throw std::runtime_error("Failed to copy to destination: " + relative
                                    + " (" + ec.message() + ")");
                            }
                            break;
                        }
                    }
                } else {
                    std::filesystem::copy_file(entry.path(), dest_path, ec);
                    if (ec) {
                        throw std::runtime_error("Failed to copy new file: " + relative
                            + " (" + ec.message() + ")");
                    }
                }
            }
        }

        verify_finalized(operation_id, dest_dir);
    } catch (...) {
        mark_rolling_back(operation_id);
        restore_backups(operation_id);
        mark_rolled_back(operation_id);
        throw;
    }
    cleanup_backups(operation_id);
    mark_committed(operation_id);
}

void StagingManager::rollback_staging(const std::string& operation_id) {
    mark_rolling_back(operation_id);
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

void StagingManager::mark_rolling_back(const std::string& operation_id) {
    update_metadata(operation_id, {{"state", state_to_string(StagingState::RollingBack)}});
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
        if (op.state == state_to_string(StagingState::RolledBack)) {
            cleanup_staging(op.operation_id);
        }
    }
}

void StagingManager::cleanup_committed() {
    if (!std::filesystem::exists(staging_base_)) return;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(staging_base_, ec)) {
        if (entry.is_directory()) {
            std::string op_id = entry.path().filename().string();
            StagingOperation op = read_metadata(op_id);
            if (op.state == state_to_string(StagingState::Committed)) {
                cleanup_staging(op_id);
            }
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

std::vector<BackupEntry> StagingManager::read_backup_journal(const std::string& operation_id) const {
    std::vector<BackupEntry> entries;
    std::string journal = get_staging_path(operation_id) + "/.meta/backups.json";

    if (!std::filesystem::exists(journal)) {
        return entries;
    }

    std::ifstream f(journal);
    if (!f.is_open()) return entries;

    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    f.close();

    size_t pos = 0;
    skip_whitespace(content, pos);
    if (pos >= content.size() || content[pos] != '[') {
        throw std::runtime_error("Journal parse error: expected '[' at start");
    }
    pos++;

    while (true) {
        skip_whitespace(content, pos);
        if (pos >= content.size()) {
            throw std::runtime_error("Journal parse error: unexpected end of input");
        }
        if (content[pos] == ']') {
            break;
        }
        if (content[pos] == ',') {
            pos++;
            continue;
        }
        if (content[pos] != '{') {
            throw std::runtime_error("Journal parse error: expected '{' or ']' at position " + std::to_string(pos));
        }

        size_t depth = 1;
        size_t obj_start = pos;
        pos++;
        while (pos < content.size() && depth > 0) {
            if (content[pos] == '"') {
                pos++;
                while (pos < content.size() && content[pos] != '"') {
                    if (content[pos] == '\\') pos++;
                    pos++;
                }
                pos++;
            } else {
                if (content[pos] == '{') depth++;
                else if (content[pos] == '}') depth--;
                pos++;
            }
        }
        if (depth != 0) {
            throw std::runtime_error("Journal parse error: unterminated object");
        }
        size_t obj_end = pos - 1;

        std::string obj = content.substr(obj_start + 1, obj_end - obj_start - 1);

        BackupEntry be;
        be.destination = find_json_value(obj, "destination");
        be.backup_path = find_json_value(obj, "backup_path");
        be.state = find_json_value(obj, "state");

        if (be.destination.empty()) {
            throw std::runtime_error("Journal parse error: entry missing required 'destination' field");
        }
        if (be.backup_path.empty()) {
            throw std::runtime_error("Journal parse error: entry missing required 'backup_path' field");
        }

        entries.push_back(std::move(be));
    }

    return entries;
}

void StagingManager::write_backup_journal(const std::string& operation_id,
                                           const std::vector<BackupEntry>& entries) {
    std::string journal = get_staging_path(operation_id) + "/.meta/backups.json";
    std::string tmp = journal + ".tmp";

    std::error_code dir_ec;
    std::filesystem::create_directories(std::filesystem::path(journal).parent_path(), dir_ec);

    std::ofstream f(tmp);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to write backup journal: " + tmp);
    }

    f << "[\n";
    for (size_t i = 0; i < entries.size(); i++) {
        const auto& e = entries[i];
        f << "  {\n";
        f << "    \"destination\": \"" << json_escape(e.destination) << "\",\n";
        f << "    \"backup_path\": \"" << json_escape(e.backup_path) << "\",\n";
        f << "    \"state\": \"" << json_escape(e.state) << "\"\n";
        f << "  }";
        if (i + 1 < entries.size()) f << ",";
        f << "\n";
    }
    f << "]\n";
    f.flush();
    if (f.fail()) {
        f.close();
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        throw std::runtime_error("Failed to flush backup journal: " + tmp);
    }
    f.close();

    std::error_code ec;
    std::filesystem::rename(tmp, journal, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        throw std::runtime_error("Failed to atomically write journal: " + journal + " (" + ec.message() + ")");
    }
}

void StagingManager::append_backup_entry(const std::string& operation_id,
                                          const BackupEntry& entry) {
    auto entries = read_backup_journal(operation_id);
    entries.push_back(entry);
    write_backup_journal(operation_id, entries);
}

std::string StagingManager::get_backup_path(const std::string& operation_id,
                                             const std::string& relative) const {
    std::string safe_name;
    for (char c : relative) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') {
            safe_name += '_';
        } else {
            safe_name += c;
        }
    }
    return get_staging_path(operation_id) + "/.meta/backups/" + safe_name;
}

void StagingManager::restore_backups(const std::string& operation_id) {
    auto entries = read_backup_journal(operation_id);

    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        if (it->state == "created" && !it->backup_path.empty() && !it->destination.empty()) {
            restore_file_strict(it->backup_path, it->destination);
        }
    }

    std::string journal = get_staging_path(operation_id) + "/.meta/backups.json";
    std::error_code ec;
    std::filesystem::remove(journal, ec);

    std::string backups_dir = get_staging_path(operation_id) + "/.meta/backups";
    if (std::filesystem::exists(backups_dir)) {
        std::filesystem::remove_all(backups_dir, ec);
    }
}

void StagingManager::cleanup_backups(const std::string& operation_id) {
    auto entries = read_backup_journal(operation_id);

    for (const auto& e : entries) {
        if (!e.backup_path.empty()) {
            std::error_code ec;
            std::filesystem::remove(e.backup_path, ec);
        }
    }

    std::string journal = get_staging_path(operation_id) + "/.meta/backups.json";
    std::error_code ec;
    std::filesystem::remove(journal, ec);

    std::string backups_dir = get_staging_path(operation_id) + "/.meta/backups";
    if (std::filesystem::exists(backups_dir)) {
        std::filesystem::remove_all(backups_dir, ec);
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
    std::string tmp = meta_file + ".tmp";
    std::ofstream f(tmp);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to write metadata: " + tmp);
    }
    f << "{\n";
    f << "  \"operation_id\": \"" << json_escape(meta.operation_id) << "\",\n";
    f << "  \"operation_type\": \"" << json_escape(meta.operation_type) << "\",\n";
    f << "  \"state\": \"" << json_escape(meta.state) << "\",\n";
    f << "  \"created_at\": \"" << json_escape(meta.created_at) << "\",\n";
    f << "  \"item_id\": \"" << json_escape(meta.item_id) << "\",\n";
    f << "  \"version_id\": \"" << json_escape(meta.version_id) << "\",\n";
    f << "  \"expected_checksum\": \"" << json_escape(meta.expected_checksum) << "\"\n";
    f << "}\n";
    f.flush();
    if (f.fail()) {
        f.close();
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        throw std::runtime_error("Failed to flush metadata: " + tmp);
    }
    f.close();

    std::error_code ec;
#ifdef _WIN32
    std::filesystem::remove(meta_file, ec);
#endif
    std::filesystem::rename(tmp, meta_file, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        throw std::runtime_error("Failed to write metadata: " + meta_file + " (" + ec.message() + ")");
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
    if (!f.is_open()) return op;

    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    f.close();

    op.operation_type = find_json_value(content, "operation_type");
    op.state = find_json_value(content, "state");
    op.created_at = find_json_value(content, "created_at");
    op.item_id = find_json_value(content, "item_id");
    op.version_id = find_json_value(content, "version_id");
    op.expected_checksum = find_json_value(content, "expected_checksum");

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

void StagingManager::backup_file_strict(const std::string& src, const std::string& dest) {
    std::error_code ec;
    if (!std::filesystem::exists(src)) {
        throw std::runtime_error("Backup source does not exist: " + src);
    }

    std::filesystem::create_directories(std::filesystem::path(dest).parent_path(), ec);
    if (ec) {
        throw std::runtime_error("Failed to create backup directory: " + ec.message());
    }

    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        throw std::runtime_error("Failed to create backup: " + src + " -> " + dest + " (" + ec.message() + ")");
    }

    if (!std::filesystem::exists(dest)) {
        throw std::runtime_error("Backup verification failed: file does not exist after copy: " + dest);
    }

    auto src_size = std::filesystem::file_size(src, ec);
    auto dest_size = std::filesystem::file_size(dest, ec);
    if (!ec && src_size != dest_size) {
        throw std::runtime_error("Backup verification failed: size mismatch for " + dest);
    }
}

void StagingManager::restore_file_strict(const std::string& backup, const std::string& original) {
    std::error_code ec;
    if (!std::filesystem::exists(backup)) {
        return;
    }

    std::filesystem::copy_file(backup, original,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        throw std::runtime_error("Failed to restore backup: " + backup + " -> " + original
            + " (" + ec.message() + ")");
    }

    std::filesystem::remove(backup, ec);
}

std::string StagingManager::compute_file_checksum(const std::string& path) {
    try {
        return hashing::FileHasher::hash_file(path);
    } catch (...) {
        return "";
    }
}

} // namespace archive::filesystem
