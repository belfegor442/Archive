#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include "../core/models/ScanItem.h"
#include "../core/models/Classification.h"
#include "../core/models/ClassificationRule.h"
#include "../core/types/AnalysisResult.h"
#include "../storage/DatabaseManager.h"
#include "../storage/ClassificationRepository.h"
#include "../storage/ClassificationRuleRepository.h"
#include "../storage/ScanItemRepository.h"
#include "FileAnalysisEngine.h"
#include "RelationshipEngine.h"

namespace archive::services {

using ClassifyProgressFn = std::function<void(int items_classified, int total)>;

class Classifier {
public:
    Classifier(storage::DatabaseManager& db,
               storage::ClassificationRepository& classifications,
               storage::ClassificationRuleRepository& rules,
               storage::ScanItemRepository& scan_items);

    std::vector<core::Classification> classify_scan(const std::string& scan_id, int intensity = 50,
                                                     const ClassifyProgressFn& progress = nullptr);
    std::vector<core::Classification> classify_with_analyses(
        const std::string& scan_id,
        const std::vector<FileAnalysis>& analyses,
        int intensity = 50,
        const ClassifyProgressFn& progress = nullptr);
    core::Classification classify_item(const core::ScanItem& item, int intensity = 50);
    void set_rules(const std::vector<core::ClassificationRule>& rules);
    std::vector<std::pair<std::string, int>> get_taxonomy_summary(const std::string& scan_id) const;

private:
    storage::DatabaseManager& db_;
    storage::ClassificationRepository& classifications_;
    storage::ClassificationRuleRepository& rules_repo_;
    storage::ScanItemRepository& scan_items_;
    std::vector<core::ClassificationRule> rules_;
    std::vector<core::ClassificationRule> sorted_rules_;
    bool rules_sorted_ = false;
    FileAnalysisEngine analysis_engine_;
    RelationshipEngine relationship_engine_;

    std::string try_user_rules(const core::ScanItem& item);
    std::string classify_by_extension(const core::ScanItem& item, int intensity);
    std::string classify_by_mime(const core::ScanItem& item, int intensity);
    std::string classify_by_content(const core::ScanItem& item, int intensity);
    std::string classify_by_project(const core::ScanItem& item, int intensity);
    std::string classify_by_filename_pattern(const core::ScanItem& item, int intensity);
    double compute_confidence(const core::ScanItem& item, const std::string& taxonomy_path, int intensity);
    std::string generate_reason(const core::ScanItem& item, const std::string& taxonomy_path);
    std::string build_taxonomy_path(const std::string& category, const std::string& subcategory,
                                     const std::string& detail, int intensity);
    void detect_relationships(std::vector<core::Classification>& classifications,
                              const std::vector<core::ScanItem>& items);
    std::map<std::string, std::vector<std::string>> find_name_groups(const std::vector<core::ScanItem>& items);
    bool matches_pattern(const std::string& filename, const std::string& pattern);
};

} // namespace archive::services
