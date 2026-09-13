#pragma once

#include <string>

#include "../core/models/ArchiveItem.h"
#include "../core/enums/ItemStatus.h"
#include "../storage/ArchiveItemRepository.h"
#include "../storage/ActivityRepository.h"
#include "../filesystem/StorageManager.h"

namespace archive::services {

class UpdateService {
public:
    UpdateService(
        storage::ArchiveItemRepository& items,
        storage::ActivityRepository& activities,
        filesystem::StorageManager& storage
    );

    void update_metadata(const std::string& item_id, const std::string& name,
                         const std::string& description);
    void move_to_trash(const std::string& item_id);
    void restore_from_trash(const std::string& item_id);
    void permanent_delete(const std::string& item_id);
    void set_category(const std::string& item_id, const std::string& category_id);
    void toggle_favorite(const std::string& item_id);

private:
    storage::ArchiveItemRepository& items_;
    storage::ActivityRepository& activities_;
    filesystem::StorageManager& storage_;

    std::string generate_id();
    std::string now_iso();
};

} // namespace archive::services
