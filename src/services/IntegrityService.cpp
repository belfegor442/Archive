#include "IntegrityService.h"

#include <filesystem>

namespace archive::services {

IntegrityService::IntegrityService(storage::ArchiveItemRepository& items)
    : items_(items)
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

    if (!std::filesystem::exists(item->storage_path)) {
        vi.state = core::IntegrityState::Missing;
        vi.details = "Storage file not found: " + item->storage_path;
        result.items.push_back(std::move(vi));
        result.missing_count++;
        return result;
    }

    try {
        vi.actual_checksum = hashing::FileHasher::hash_file(item->storage_path);
    } catch (const std::exception& e) {
        vi.state = core::IntegrityState::Corrupted;
        vi.details = "Hash failed: " + std::string(e.what());
        result.items.push_back(std::move(vi));
        result.corrupted_count++;
        return result;
    }

    if (vi.actual_checksum == vi.expected_checksum) {
        vi.state = core::IntegrityState::Valid;
        vi.details = "Checksum matches";
        result.valid_count++;
    } else {
        vi.state = core::IntegrityState::Modified;
        vi.details = "Checksum mismatch";
        result.modified_count++;
    }

    result.items.push_back(std::move(vi));
    return result;
}

} // namespace archive::services
