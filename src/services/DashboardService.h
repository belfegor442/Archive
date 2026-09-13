#pragma once

#include "../core/types/DashboardStats.h"
#include "../storage/ArchiveItemRepository.h"

namespace archive::services {

class DashboardService {
public:
    explicit DashboardService(storage::ArchiveItemRepository& items);

    core::DashboardStats get_stats();

private:
    storage::ArchiveItemRepository& items_;
};

} // namespace archive::services
