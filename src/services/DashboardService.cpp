#include "DashboardService.h"

namespace archive::services {

DashboardService::DashboardService(storage::ArchiveItemRepository& items)
    : items_(items)
{}

core::DashboardStats DashboardService::get_stats() {
    return items_.get_stats();
}

} // namespace archive::services
