#include "IntegrityService.h"

#include <filesystem>

namespace archive::services {

IntegrityService::IntegrityService(
    storage::ArchiveItemRepository& items,
    storage::VersionRepository& versions,
    storage::StoredObjectRepository& stored_objects,
    filesystem::StorageManager& storage
) : items_(items)
  , versions_(versions)
  , stored_objects_(stored_objects)
  , storage_(storage)
{}

core::VerificationResult IntegrityService::verify_all() {
    core::VerificationResult result;
    auto all_items = items_.find_all();

    for (const auto& item : all_items) {
        auto verification = verify_item(item.id);
        for (const auto& vi : verification.items) {
            result.items.push_back(vi);
        }
        result.valid_count += verification.valid_count;
        result.modified_count += verification.modified_count;
        result.missing_count += verification.missing_count;
        result.corrupted_count += verification.corrupted_count;
    }

    return result;
}

core::VerificationResult IntegrityService::verify_item(const std::string& item_id) {
    core::VerificationResult result;
    auto item = items_.find_by_id(item_id);
    if (!item) return result;

    core::VerificationItem vi;
    vi.id = item->id;
    vi.name = item->name;
    vi.expected_checksum = item->checksum;

    if (item->checksum.empty()) {
        vi.state = core::IntegrityState::Unknown;
        vi.details = "No checksum recorded";
        result.items.push_back(std::move(vi));
        result.corrupted_count++;
        return result;
    }

    if (item->type == core::ItemType::File) {
        auto file_result = verify_file_item(*item);
        result.items.push_back(std::move(file_result));
    } else {
        auto folder_result = verify_folder_item(*item);
        result.items.push_back(std::move(folder_result));
    }

    const auto& last = result.items.back();
    switch (last.state) {
        case core::IntegrityState::Valid:     result.valid_count++; break;
        case core::IntegrityState::Modified:  result.modified_count++; break;
        case core::IntegrityState::Missing:   result.missing_count++; break;
        case core::IntegrityState::Corrupted: result.corrupted_count++; break;
        case core::IntegrityState::Unknown:   result.corrupted_count++; break;
    }

    return result;
}

core::VerificationItem IntegrityService::verify_file_item(const core::ArchiveItem& item) {
    core::VerificationItem vi;
    vi.id = item.id;
    vi.name = item.name;
    vi.expected_checksum = item.checksum;

    if (!std::filesystem::exists(item.storage_path)) {
        vi.state = core::IntegrityState::Missing;
        vi.details = "File not found: " + item.storage_path;
        return vi;
    }

    try {
        vi.actual_checksum = hashing::FileHasher::hash_file(item.storage_path);
    } catch (const std::exception& e) {
        vi.state = core::IntegrityState::Corrupted;
        vi.details = "Hash failed: " + std::string(e.what());
        return vi;
    }

    if (hashing::FileHasher::compare(vi.actual_checksum, vi.expected_checksum)) {
        vi.state = core::IntegrityState::Valid;
        vi.details = "Checksum matches";
    } else {
        vi.state = core::IntegrityState::Modified;
        vi.details = "Checksum mismatch";
    }

    return vi;
}

core::VerificationItem IntegrityService::verify_folder_item(const core::ArchiveItem& item) {
    core::VerificationItem vi;
    vi.id = item.id;
    vi.name = item.name;
    vi.expected_checksum = item.checksum;

    if (!std::filesystem::exists(item.storage_path)) {
        vi.state = core::IntegrityState::Missing;
        vi.details = "Folder not found: " + item.storage_path;
        return vi;
    }

    try {
        vi.actual_checksum = hashing::FileHasher::hash_folder(item.storage_path);
    } catch (const std::exception& e) {
        vi.state = core::IntegrityState::Corrupted;
        vi.details = "Folder hash failed: " + std::string(e.what());
        return vi;
    }

    if (hashing::FileHasher::compare(vi.actual_checksum, vi.expected_checksum)) {
        vi.state = core::IntegrityState::Valid;
        vi.details = "Folder checksum matches";
    } else {
        vi.state = core::IntegrityState::Modified;
        vi.details = "Folder checksum mismatch";
    }

    return vi;
}

core::VerificationResult IntegrityService::verify_version(const std::string& version_id) {
    core::VerificationResult result;
    auto ver = versions_.find_by_id(version_id);
    if (!ver) return result;

    auto item = items_.find_by_id(ver->item_id);
    if (!item) return result;

    core::VerificationItem vi;
    vi.id = ver->id;
    vi.name = item->name + " v" + std::to_string(ver->version_number);
    vi.expected_checksum = ver->checksum;

    if (ver->checksum.empty()) {
        vi.state = core::IntegrityState::Unknown;
        vi.details = "No checksum recorded for version";
        result.items.push_back(std::move(vi));
        result.corrupted_count++;
        return result;
    }

    if (!std::filesystem::exists(ver->storage_path)) {
        vi.state = core::IntegrityState::Missing;
        vi.details = "Version file not found: " + ver->storage_path;
        result.items.push_back(std::move(vi));
        result.missing_count++;
        return result;
    }

    try {
        vi.actual_checksum = hashing::FileHasher::hash_file(ver->storage_path);
    } catch (const std::exception& e) {
        vi.state = core::IntegrityState::Corrupted;
        vi.details = "Hash failed: " + std::string(e.what());
        result.items.push_back(std::move(vi));
        result.corrupted_count++;
        return result;
    }

    if (hashing::FileHasher::compare(vi.actual_checksum, vi.expected_checksum)) {
        vi.state = core::IntegrityState::Valid;
        vi.details = "Version checksum matches";
        result.valid_count++;
    } else {
        vi.state = core::IntegrityState::Modified;
        vi.details = "Version checksum mismatch";
        result.modified_count++;
    }

    result.items.push_back(std::move(vi));
    return result;
}

core::VerificationResult IntegrityService::verify_stored_object(const std::string& object_id) {
    core::VerificationResult result;
    auto obj = stored_objects_.find_by_id(object_id);
    if (!obj) return result;

    core::VerificationItem vi;
    vi.id = obj->id;
    vi.name = obj->storage_path;
    vi.expected_checksum = obj->checksum;

    if (obj->checksum.empty()) {
        vi.state = core::IntegrityState::Unknown;
        vi.details = "No checksum recorded for stored object";
        result.items.push_back(std::move(vi));
        result.corrupted_count++;
        return result;
    }

    if (!std::filesystem::exists(obj->storage_path)) {
        vi.state = core::IntegrityState::Missing;
        vi.details = "Stored object file not found: " + obj->storage_path;
        result.items.push_back(std::move(vi));
        result.missing_count++;
        return result;
    }

    try {
        vi.actual_checksum = hashing::FileHasher::hash_file(obj->storage_path);
    } catch (const std::exception& e) {
        vi.state = core::IntegrityState::Corrupted;
        vi.details = "Hash failed: " + std::string(e.what());
        result.items.push_back(std::move(vi));
        result.corrupted_count++;
        return result;
    }

    if (hashing::FileHasher::compare(vi.actual_checksum, vi.expected_checksum)) {
        vi.state = core::IntegrityState::Valid;
        vi.details = "Stored object checksum matches";
        result.valid_count++;
    } else {
        vi.state = core::IntegrityState::Modified;
        vi.details = "Stored object checksum mismatch";
        result.modified_count++;
    }

    result.items.push_back(std::move(vi));
    return result;
}

core::IntegrityState IntegrityService::get_state(const std::string& item_id) {
    auto result = verify_item(item_id);
    if (result.items.empty()) return core::IntegrityState::Unknown;
    return result.items[0].state;
}

bool IntegrityService::is_item_valid(const std::string& item_id) {
    return get_state(item_id) == core::IntegrityState::Valid;
}

} // namespace archive::services
