#include "VersionService.h"
#include "../filesystem/FileUtils.h"

#include <random>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace archive::services {

VersionService::VersionService(
    storage::VersionRepository& versions,
    storage::ArchiveItemRepository& items,
    storage::ActivityRepository& activities,
    filesystem::StorageManager& storage
) : versions_(versions)
  , items_(items)
  , activities_(activities)
  , storage_(storage)
{}

core::Version VersionService::create_version(const std::string& item_id, const std::string& file_path, const std::string& notes) {
    auto item = items_.find_by_id(item_id);
    if (!item) throw std::runtime_error("Item not found: " + item_id);

    int new_version = item->current_version + 1;
    std::string stored_path = storage_.store_version(item_id, new_version, file_path);
    std::string checksum = hashing::FileHasher::hash_file(file_path);
    uint64_t size = filesystem::FileUtils::file_size(file_path);

    core::Version ver;
    ver.id = generate_id();
    ver.item_id = item_id;
    ver.version_number = new_version;
    ver.storage_path = stored_path;
    ver.checksum = checksum;
    ver.size = size;
    ver.notes = notes;
    ver.created_at = now_iso();

    versions_.insert(ver);
    items_.increment_version(item_id);

    core::Activity act;
    act.id = generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::VersionCreated;
    act.details = "Version " + std::to_string(new_version) + " created";
    act.created_at = now_iso();
    activities_.insert(act);

    return ver;
}

std::vector<core::Version> VersionService::get_versions(const std::string& item_id) {
    return versions_.find_by_item(item_id);
}

std::optional<core::Version> VersionService::get_latest(const std::string& item_id) {
    return versions_.find_latest(item_id);
}

void VersionService::restore(const std::string& item_id, const std::string& version_id) {
    (void)version_id;
    core::Activity act;
    act.id = generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::VersionRestored;
    act.created_at = now_iso();
    activities_.insert(act);
}

std::string VersionService::generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(0, std::numeric_limits<uint64_t>::max());
    std::ostringstream oss;
    oss << std::hex << dis(gen) << dis(gen);
    std::string id = oss.str();
    id.resize(32);
    return id;
}

std::string VersionService::now_iso() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace archive::services
