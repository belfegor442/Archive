#include "OrganizationPlanner.h"

#include <algorithm>
#include <set>

#include "../core/utils/Uuid.h"
#include "../core/utils/Logger.h"
#include "../filesystem/FileUtils.h"
#include "../storage/Transaction.h"

namespace archive::services {

using namespace archive::core;

OrganizationPlanner::OrganizationPlanner(storage::DatabaseManager& db,
                                         storage::ScanRepository& scans,
                                         storage::ScanItemRepository& scan_items,
                                         storage::ClassificationRepository& classifications,
                                         storage::OrgPlanRepository& plans,
                                         storage::OrgMoveRepository& moves)
    : db_(db)
    , scans_(scans)
    , scan_items_(scan_items)
    , classifications_(classifications)
    , plans_(plans)
    , moves_(moves)
{}

core::OrgPlan OrganizationPlanner::create_plan(const std::string& scan_id,
                                                const std::string& root_path,
                                                int intensity) {
    core::OrgPlan plan;
    plan.id = core::utils::generate_id();
    plan.scan_id = scan_id;
    plan.root_path = root_path;
    plan.status = core::PlanStatus::Draft;
    plan.intensity = intensity;
    plan.created_at = core::utils::now_iso();

    auto items = scan_items_.find_by_scan(scan_id);
    auto classes = classifications_.find_by_scan(scan_id);

    plan.total_files = static_cast<int>(items.size());

    auto org_moves = generate_moves(items, classes, root_path, intensity);

    plan.moves_planned = static_cast<int>(org_moves.size());
    plan.unchanged = plan.total_files - plan.moves_planned;

    double total_conf = 0.0;
    for (const auto& m : org_moves) {
        total_conf += m.confidence;
    }
    plan.avg_confidence = org_moves.empty() ? 0.0 : total_conf / org_moves.size();

    {
        storage::Transaction tx(db_);
        plans_.insert(plan);
        for (auto& m : org_moves) {
            m.plan_id = plan.id;
        }
        if (!org_moves.empty()) {
            moves_.insert_batch(org_moves);
        }
        tx.commit();
    }

    return plan;
}

core::OrgPlanSummary OrganizationPlanner::get_summary(const std::string& plan_id) const {
    core::OrgPlanSummary summary;

    auto plan_opt = plans_.find_by_id(plan_id);
    if (!plan_opt) return summary;

    const auto& plan = *plan_opt;
    summary.plan_id = plan.id;
    summary.total_files = plan.total_files;
    summary.moves_planned = plan.moves_planned;
    summary.unchanged = plan.unchanged;
    summary.avg_confidence = plan.avg_confidence;

    auto plan_moves = moves_.find_by_plan(plan_id);

    std::map<std::string, int> category_counts;
    std::map<double, int> confidence_dist;

    for (const auto& m : plan_moves) {
        std::string category = m.reason;
        auto paren_pos = category.find('(');
        if (paren_pos != std::string::npos) {
            category = category.substr(0, paren_pos);
        }

        size_t last_slash = m.dest_path.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            std::string dest_dir = m.dest_path.substr(0, last_slash);
            size_t second_slash = dest_dir.find_last_of("/\\");
            if (second_slash != std::string::npos) {
                category = dest_dir.substr(second_slash + 1);
            } else {
                category = dest_dir;
            }
        }

        category_counts[category]++;

        double bucket = std::floor(m.confidence * 10) / 10.0;
        confidence_dist[bucket]++;
    }

    summary.by_category = category_counts;

    for (const auto& [conf, count] : confidence_dist) {
        summary.confidence_distribution.push_back({std::to_string(conf), count});
    }

    std::sort(summary.confidence_distribution.begin(), summary.confidence_distribution.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    return summary;
}

std::vector<core::MoveDetail> OrganizationPlanner::get_moves(const std::string& plan_id) const {
    auto plan_moves = moves_.find_by_plan(plan_id);
    std::vector<core::MoveDetail> details;
    details.reserve(plan_moves.size());

    for (const auto& m : plan_moves) {
        core::MoveDetail detail;
        detail.source = m.source_path;
        detail.destination = m.dest_path;
        detail.confidence = m.confidence;
        detail.reason = m.reason;

        std::string category = "Uncategorized";
        size_t last_slash = m.dest_path.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            std::string dest_dir = m.dest_path.substr(0, last_slash);
            size_t second_slash = dest_dir.find_last_of("/\\");
            if (second_slash != std::string::npos) {
                category = dest_dir.substr(second_slash + 1);
            } else {
                category = dest_dir;
            }
        }
        detail.category = category;

        details.push_back(std::move(detail));
    }

    std::sort(details.begin(), details.end(),
              [](const core::MoveDetail& a, const core::MoveDetail& b) {
                  return a.category < b.category;
              });

    return details;
}

void OrganizationPlanner::approve_plan(const std::string& plan_id) {
    auto plan_opt = plans_.find_by_id(plan_id);
    if (!plan_opt) return;

    core::OrgPlan plan = *plan_opt;
    plan.status = core::PlanStatus::Ready;
    plans_.update(plan);
}

void OrganizationPlanner::cancel_plan(const std::string& plan_id) {
    auto plan_opt = plans_.find_by_id(plan_id);
    if (!plan_opt) return;

    core::OrgPlan plan = *plan_opt;
    plan.status = core::PlanStatus::Failed;
    plans_.update(plan);
}

std::vector<core::OrgMove> OrganizationPlanner::generate_moves(const std::vector<core::ScanItem>& items,
                                                                const std::vector<core::Classification>& classifications,
                                                                const std::string& root_path,
                                                                int intensity) {
    std::vector<core::OrgMove> moves;

    std::map<std::string, const core::Classification*> class_map;
    for (const auto& cls : classifications) {
        class_map[cls.scan_item_id] = &cls;
    }

    for (const auto& item : items) {
        auto it = class_map.find(item.id);
        if (it == class_map.end()) continue;

        const core::Classification& cls = *it->second;

        if (cls.taxonomy_path == "Unknown") continue;
        if (cls.confidence < 0.1) continue;

        std::string dest = build_dest_path(cls.taxonomy_path, item.filename, root_path);

        if (dest == item.path) continue;

        core::OrgMove move;
        move.id = core::utils::generate_id();
        move.plan_id = "";
        move.scan_item_id = item.id;
        move.source_path = item.path;
        move.dest_path = dest;
        move.confidence = cls.confidence;
        move.reason = cls.reason;
        move.status = core::MoveStatus::Planned;

        moves.push_back(std::move(move));
    }

    resolve_conflicts(moves);

    return moves;
}

void OrganizationPlanner::resolve_conflicts(std::vector<core::OrgMove>& moves) {
    std::map<std::string, size_t> dest_count;
    std::set<size_t> to_remove;

    for (size_t i = 0; i < moves.size(); ++i) {
        std::string dest = moves[i].dest_path;
        dest_count[dest]++;

        if (dest_count[dest] > 1) {
            size_t best_idx = i;
            for (size_t j = 0; j < i; ++j) {
                if (moves[j].dest_path == dest && to_remove.find(j) == to_remove.end()) {
                    if (moves[j].confidence < moves[i].confidence) {
                        best_idx = j;
                    } else {
                        best_idx = i;
                        break;
                    }
                }
            }

            for (size_t j = 0; j < i; ++j) {
                if (j != best_idx && moves[j].dest_path == dest && to_remove.find(j) == to_remove.end()) {
                    to_remove.insert(j);
                }
            }

            if (best_idx != i) {
                to_remove.insert(i);
            }
        }
    }

    std::vector<core::OrgMove> resolved;
    resolved.reserve(moves.size() - to_remove.size());

    for (size_t i = 0; i < moves.size(); ++i) {
        if (to_remove.find(i) == to_remove.end()) {
            int counter = 1;
            std::string final_dest = moves[i].dest_path;

            std::set<std::string> existing_dests;
            for (const auto& m : resolved) {
                existing_dests.insert(m.dest_path);
            }
            for (size_t j = 0; j < i; ++j) {
                if (to_remove.find(j) == to_remove.end()) {
                    existing_dests.insert(moves[j].dest_path);
                }
            }

            while (existing_dests.find(final_dest) != existing_dests.end()) {
                std::string stem = filesystem::FileUtils::stem(final_dest);
                std::string ext = filesystem::FileUtils::extension(final_dest);
                std::string dir = filesystem::FileUtils::parent_dir(final_dest);
                std::string new_name = stem + "_" + std::to_string(counter) + ext;
                final_dest = dir + "/" + new_name;
                counter++;
            }

            moves[i].dest_path = final_dest;
            resolved.push_back(std::move(moves[i]));
        }
    }

    moves = std::move(resolved);
}

std::string OrganizationPlanner::build_dest_path(const std::string& taxonomy_path,
                                                   const std::string& filename,
                                                   const std::string& root_path) {
    std::string clean_taxonomy = taxonomy_path;
    std::replace(clean_taxonomy.begin(), clean_taxonomy.end(), '\\', '/');

    std::string dest = root_path + "/" + clean_taxonomy + "/" + filename;
    return dest;
}

} // namespace archive::services
