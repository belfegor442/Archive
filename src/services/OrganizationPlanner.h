#pragma once

#include <string>
#include <vector>
#include "../core/models/Scan.h"
#include "../core/models/ScanItem.h"
#include "../core/models/OrgPlan.h"
#include "../core/models/OrgMove.h"
#include "../core/models/Classification.h"
#include "../core/types/OrgPlanSummary.h"
#include "../core/types/MoveDetail.h"
#include "../storage/DatabaseManager.h"
#include "../storage/ScanRepository.h"
#include "../storage/ScanItemRepository.h"
#include "../storage/ClassificationRepository.h"
#include "../storage/OrgPlanRepository.h"
#include "../storage/OrgMoveRepository.h"
#include "FileAnalysisEngine.h"

namespace archive::services {

class OrganizationPlanner {
public:
    OrganizationPlanner(storage::DatabaseManager& db,
                        storage::ScanRepository& scans,
                        storage::ScanItemRepository& scan_items,
                        storage::ClassificationRepository& classifications,
                        storage::OrgPlanRepository& plans,
                        storage::OrgMoveRepository& moves);

    core::OrgPlan create_plan(const std::string& scan_id, const std::string& root_path, int intensity = 50);
    core::OrgPlan create_plan_with_analyses(const std::string& scan_id, const std::string& root_path,
                                             const std::vector<FileAnalysis>& analyses, int intensity = 50);
    core::OrgPlanSummary get_summary(const std::string& plan_id) const;
    std::vector<core::MoveDetail> get_moves(const std::string& plan_id) const;
    void approve_plan(const std::string& plan_id);
    void cancel_plan(const std::string& plan_id);

    static double confidence_threshold_for_intensity(int intensity);

private:
    storage::DatabaseManager& db_;
    storage::ScanRepository& scans_;
    storage::ScanItemRepository& scan_items_;
    storage::ClassificationRepository& classifications_;
    storage::OrgPlanRepository& plans_;
    storage::OrgMoveRepository& moves_;
    FileAnalysisEngine analysis_engine_;

    std::vector<core::OrgMove> generate_moves(const std::vector<core::ScanItem>& items,
                                              const std::vector<core::Classification>& classifications,
                                              const std::string& root_path, int intensity);
    std::vector<core::OrgMove> generate_moves_with_analyses(
        const std::vector<core::ScanItem>& items,
        const std::vector<core::Classification>& classifications,
        const std::vector<FileAnalysis>& analyses,
        const std::string& root_path, int intensity);
    void resolve_conflicts(std::vector<core::OrgMove>& moves);
    std::string build_dest_path(const std::string& taxonomy_path, const std::string& filename,
                                 const std::string& root_path);
};

} // namespace archive::services
