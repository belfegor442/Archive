#include "app/AppConfig.h"
#include "storage/DatabaseManager.h"
#include "storage/ScanRepository.h"
#include "storage/ScanItemRepository.h"
#include "storage/ClassificationRepository.h"
#include "storage/ClassificationRuleRepository.h"
#include "storage/TaxonomyRepository.h"
#include "storage/OrgPlanRepository.h"
#include "storage/OrgMoveRepository.h"
#include "storage/UndoRepository.h"
#include "services/Scanner.h"
#include "services/Classifier.h"
#include "services/OrganizationPlanner.h"
#include "services/OrganizationExecutor.h"
#include "core/types/AnalysisResult.h"
#include "core/types/OrgPlanSummary.h"
#include "core/types/MoveDetail.h"
#include "core/models/ClassificationRule.h"
#include "core/models/TaxonomyNode.h"
#include "core/models/OrgPlan.h"
#include "core/models/UndoRecord.h"
#include "core/models/OrgMove.h"
#include "core/enums/PlanStatus.h"
#include "core/enums/MoveStatus.h"
#include "core/enums/UndoStatus.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include <algorithm>
#include <map>
#include <functional>

using namespace archive;

struct CliContext {
    app::AppConfig config;
    std::string command;
    std::vector<std::string> args;
    int intensity = 50;
    bool dry_run = false;
    bool yes = false;
};

static void print_usage() {
    std::cout <<
        "Archive — intelligent file organizer\n"
        "\n"
        "Usage: archive <command> [options]\n"
        "\n"
        "Commands:\n"
        "  analyze <path>              Scan and classify a directory\n"
        "  plan <path>                 Generate an organization plan\n"
        "  preview <plan_id>           Show detailed plan before executing\n"
        "  organize <plan_id>          Execute an approved plan\n"
        "  undo <operation_id>         Undo a previous organization\n"
        "  list-plans                  List all organization plans\n"
        "  list-undos                  List available undo operations\n"
        "  rules                       List classification rules\n"
        "  add-rule <pattern> <target> Add a classification rule\n"
        "  taxonomy                    Show current taxonomy\n"
        "  help                        Show this help message\n"
        "\n"
        "Options:\n"
        "  --data-dir <path>   Override data directory (default: ~/.archive-data)\n"
        "  --intensity <N>     Classification intensity 0-100 (default: 50)\n"
        "  --dry-run           Show what would happen without executing\n"
        "  --yes               Skip confirmation prompts\n"
        "\n"
        "Exit codes:\n"
        "  0   Success\n"
        "  1   General error\n"
        "  2   Invalid arguments\n"
        "  3   Plan not found\n"
        "  4   Execution failed\n"
        "  5   Undo failed\n"
        "  6   Scanner error\n";
}

static void print_version() {
    std::cout << "Archive v0.2.0\n";
}

static bool parse_args(int argc, char* argv[], CliContext& ctx) {
    ctx.config = app::AppConfig::default_config();

    std::vector<std::string> positional;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            ctx.command = "help";
            return true;
        } else if (arg == "--version") {
            ctx.command = "version";
            return true;
        } else if (arg == "--data-dir" && i + 1 < argc) {
            ctx.config.data_dir = argv[++i];
            ctx.config.db_path = ctx.config.data_dir + "/archive.db";
            ctx.config.items_dir = ctx.config.data_dir + "/items";
        } else if (arg == "--intensity" && i + 1 < argc) {
            ctx.intensity = std::atoi(argv[++i]);
            if (ctx.intensity < 0 || ctx.intensity > 100) {
                std::cerr << "Error: intensity must be 0-100\n";
                return false;
            }
        } else if (arg == "--dry-run") {
            ctx.dry_run = true;
        } else if (arg == "--yes" || arg == "-y") {
            ctx.yes = true;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.empty()) {
        ctx.command = "help";
        return true;
    }

    ctx.command = positional[0];
    ctx.args = std::vector<std::string>(positional.begin() + 1, positional.end());
    return true;
}

static void print_analysis(const core::AnalysisResult& result) {
    std::cout << "Analysis for: " << result.root_path << "\n\n";

    std::cout << "  Files:     " << result.total_files << "\n"
              << "  Folders:   " << result.total_folders << "\n"
              << "  Total:     " << result.total_files + result.total_folders << "\n\n";

    if (!result.by_extension.empty()) {
        std::cout << "By extension:\n";
        std::vector<std::pair<std::string, int>> sorted_ext(result.by_extension.begin(),
                                                             result.by_extension.end());
        std::sort(sorted_ext.begin(), sorted_ext.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        for (const auto& [ext, count] : sorted_ext) {
            std::cout << "  " << std::setw(20) << std::left << ext
                      << std::setw(6) << std::right << count << "\n";
        }
        std::cout << "\n";
    }

    if (!result.by_mime.empty()) {
        std::cout << "By MIME type:\n";
        std::vector<std::pair<std::string, int>> sorted_mime(result.by_mime.begin(),
                                                              result.by_mime.end());
        std::sort(sorted_mime.begin(), sorted_mime.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        for (const auto& [mime, count] : sorted_mime) {
            std::cout << "  " << std::setw(30) << std::left << mime
                      << std::setw(6) << std::right << count << "\n";
        }
        std::cout << "\n";
    }

    if (!result.by_project.empty()) {
        std::cout << "By project context:\n";
        for (const auto& [proj, count] : result.by_project) {
            std::cout << "  " << std::setw(20) << std::left << proj
                      << std::setw(6) << std::right << count << "\n";
        }
        std::cout << "\n";
    }

    if (!result.detected_groups.empty()) {
        std::cout << "Detected file groups:\n";
        for (const auto& group : result.detected_groups) {
            std::cout << "  " << group << "\n";
        }
        std::cout << "\n";
    }
}

static void print_plan_summary(const core::OrgPlanSummary& summary) {
    std::cout << "Organization Plan: " << summary.plan_id << "\n\n";

    std::cout << "  Total files:     " << summary.total_files << "\n"
              << "  Moves planned:   " << summary.moves_planned << "\n"
              << "  Unchanged:       " << summary.unchanged << "\n"
              << "  Avg confidence:  " << std::fixed << std::setprecision(1)
              << (summary.avg_confidence * 100.0) << "%\n\n";

    if (!summary.by_category.empty()) {
        std::cout << "By category:\n";
        for (const auto& [cat, count] : summary.by_category) {
            std::cout << "  " << std::setw(30) << std::left << cat
                      << std::setw(6) << std::right << count << "\n";
        }
        std::cout << "\n";
    }

    if (!summary.confidence_distribution.empty()) {
        std::cout << "Confidence distribution:\n";
        for (const auto& [label, pct] : summary.confidence_distribution) {
            std::cout << "  " << std::setw(20) << std::left << label
                      << std::fixed << std::setprecision(1) << (pct * 100.0) << "%\n";
        }
        std::cout << "\n";
    }
}

static void print_moves(const std::vector<core::MoveDetail>& moves) {
    if (moves.empty()) {
        std::cout << "No moves in this plan.\n";
        return;
    }

    std::cout << "Moves (" << moves.size() << "):\n\n";

    size_t max_src = 0, max_dst = 0;
    for (const auto& m : moves) {
        max_src = std::max(max_src, m.source.size());
        max_dst = std::max(max_dst, m.destination.size());
    }
    max_src = std::min(max_src, (size_t)50);
    max_dst = std::min(max_dst, (size_t)50);

    for (const auto& m : moves) {
        std::string src = m.source.size() > 50
            ? "..." + m.source.substr(m.source.size() - 47) : m.source;
        std::string dst = m.destination.size() > 50
            ? "..." + m.destination.substr(m.destination.size() - 47) : m.destination;

        std::cout << "  " << std::setw(max_src) << std::left << src
                  << "  ->  "
                  << std::setw(max_dst) << std::left << dst
                  << "  [" << std::fixed << std::setprecision(0)
                  << (m.confidence * 100.0) << "%]"
                  << "  " << m.reason
                  << "\n";
    }
    std::cout << "\n";
}

static int cmd_analyze(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: analyze requires a path\n";
        std::cerr << "Usage: archive analyze <path>\n";
        return 2;
    }

    ctx.config.ensure_directories();
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ScanRepository scans(db);
    storage::ScanItemRepository scan_items(db);

    services::Scanner scanner(db, scans, scan_items);

    try {
        auto scan = scanner.scan_directory(ctx.args[0]);
        auto result = scanner.analyze(scan.id);
        print_analysis(result);
        std::cout << "Scan ID: " << scan.id << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 6;
    }
}

static int cmd_plan(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: plan requires a path\n";
        std::cerr << "Usage: archive plan <path> [--intensity N]\n";
        return 2;
    }

    ctx.config.ensure_directories();
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ScanRepository scans(db);
    storage::ScanItemRepository scan_items(db);
    storage::ClassificationRepository classifications(db);
    storage::ClassificationRuleRepository rules(db);
    storage::OrgPlanRepository plans(db);
    storage::OrgMoveRepository moves(db);

    services::Scanner scanner(db, scans, scan_items);
    services::Classifier classifier(db, classifications, rules, scan_items);
    services::OrganizationPlanner planner(db, scans, scan_items, classifications, plans, moves);

    try {
        auto scan = scanner.scan_directory(ctx.args[0]);
        classifier.classify_scan(scan.id, ctx.intensity);

        auto plan = planner.create_plan(scan.id, ctx.args[0], ctx.intensity);
        auto summary = planner.get_summary(plan.id);

        planner.approve_plan(plan.id);
        print_plan_summary(summary);

        std::cout << "Plan ID: " << plan.id << "\n";
        std::cout << "Use 'archive preview " << plan.id << "' to see detailed moves.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

static int cmd_preview(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: preview requires a plan ID\n";
        std::cerr << "Usage: archive preview <plan_id>\n";
        return 2;
    }

    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::OrgPlanRepository plans(db);
    storage::OrgMoveRepository moves(db);
    storage::ScanRepository scans(db);
    storage::ScanItemRepository scan_items(db);
    storage::ClassificationRepository classifications(db);

    services::OrganizationPlanner planner(db, scans, scan_items, classifications, plans, moves);

    auto plan = plans.find_by_id(ctx.args[0]);
    if (!plan) {
        std::cerr << "Error: plan not found: " << ctx.args[0] << "\n";
        return 3;
    }

    auto summary = planner.get_summary(plan->id);
    auto move_details = planner.get_moves(plan->id);

    std::string status_str = core::to_string(plan->status);

    std::cout << "Plan: " << plan->id << "\n"
              << "  Root:     " << plan->root_path << "\n"
              << "  Status:   " << status_str << "\n"
              << "  Intensity:" << plan->intensity << "\n"
              << "  Created:  " << plan->created_at << "\n\n";

    print_plan_summary(summary);
    print_moves(move_details);

    return 0;
}

static bool confirm(const std::string& message, bool skip) {
    if (skip) return true;
    std::cout << message << " [y/N] ";
    std::string input;
    std::getline(std::cin, input);
    return input == "y" || input == "Y" || input == "yes" || input == "YES";
}

static int cmd_organize(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: organize requires a plan ID\n";
        std::cerr << "Usage: archive organize <plan_id> [--dry-run] [--yes]\n";
        return 2;
    }

    ctx.config.ensure_directories();
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::OrgPlanRepository plans(db);
    storage::OrgMoveRepository moves(db);
    storage::UndoRepository undo(db);

    services::OrganizationExecutor executor(db, plans, moves, undo);

    auto plan = plans.find_by_id(ctx.args[0]);
    if (!plan) {
        std::cerr << "Error: plan not found: " << ctx.args[0] << "\n";
        return 3;
    }

    auto summary_moves = moves.find_by_plan(plan->id);
    std::cout << "Executing plan: " << plan->id << "\n"
              << "  Moves: " << summary_moves.size() << "\n"
              << "  Root:  " << plan->root_path << "\n\n";

    if (ctx.dry_run) {
        std::cout << "[dry-run] No changes made.\n";
        for (const auto& m : summary_moves) {
            std::cout << "  " << m.source_path << " -> " << m.dest_path << "\n";
        }
        return 0;
    }

    if (!confirm("Execute this plan?", ctx.yes)) {
        std::cout << "Cancelled.\n";
        return 0;
    }

    try {
        auto record = executor.execute(plan->id);
        std::cout << "Plan executed successfully.\n"
                  << "  Moves completed: " << record.moves_count << "\n"
                  << "  Undo ID:         " << record.id << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 4;
    }
}

static int cmd_undo(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: undo requires an operation ID\n";
        std::cerr << "Usage: archive undo <operation_id> [--yes]\n";
        return 2;
    }

    ctx.config.ensure_directories();
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::OrgPlanRepository plans(db);
    storage::OrgMoveRepository moves(db);
    storage::UndoRepository undo_repo(db);

    services::OrganizationExecutor executor(db, plans, moves, undo_repo);

    auto record = undo_repo.find_record_by_id(ctx.args[0]);
    if (!record) {
        std::cerr << "Error: undo record not found: " << ctx.args[0] << "\n";
        return 5;
    }

    if (record->status == core::UndoStatus::Used) {
        std::cerr << "Error: undo already applied for: " << ctx.args[0] << "\n";
        return 5;
    }

    std::cout << "Undo operation: " << record->id << "\n"
              << "  Moves: " << record->moves_count << "\n"
              << "  Root:  " << record->root_path << "\n\n";

    if (!confirm("Undo this operation?", ctx.yes)) {
        std::cout << "Cancelled.\n";
        return 0;
    }

    try {
        executor.undo(record->id);
        std::cout << "Undo completed successfully.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 5;
    }
}

static int cmd_list_plans(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::OrgPlanRepository plans(db);
    auto all = plans.find_all();

    if (all.empty()) {
        std::cout << "No organization plans.\n";
        return 0;
    }

    std::cout << "Organization plans (" << all.size() << "):\n\n";

    size_t max_root = 0;
    for (const auto& p : all) {
        max_root = std::max(max_root, p.root_path.size());
    }
    max_root = std::min(max_root, (size_t)40);

    for (const auto& p : all) {
        std::string status_str = core::to_string(p.status);
        std::string root = p.root_path.size() > 40
            ? "..." + p.root_path.substr(p.root_path.size() - 37) : p.root_path;

        std::cout << "  " << std::setw(12) << std::left << p.id
                  << "  " << std::setw(max_root) << std::left << root
                  << "  " << std::setw(10) << std::left << status_str
                  << "  " << std::setw(4) << std::right << p.moves_planned << " moves"
                  << "  " << p.created_at
                  << "\n";
    }
    std::cout << "\n";

    return 0;
}

static int cmd_list_undos(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::UndoRepository undo_repo(db);
    auto all = undo_repo.find_all();

    if (all.empty()) {
        std::cout << "No undo operations available.\n";
        return 0;
    }

    std::cout << "Undo operations (" << all.size() << "):\n\n";

    for (const auto& r : all) {
        std::string status_str = core::to_string(r.status);

        std::cout << "  " << std::setw(12) << std::left << r.id
                  << "  " << std::setw(10) << std::left << status_str
                  << "  " << std::setw(4) << std::right << r.moves_count << " moves"
                  << "  " << r.created_at
                  << "\n";
    }
    std::cout << "\n";

    return 0;
}

static int cmd_rules(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ClassificationRuleRepository rules(db);
    auto all = rules.find_all();

    if (all.empty()) {
        std::cout << "No classification rules defined.\n";
        return 0;
    }

    std::cout << "Classification rules (" << all.size() << "):\n\n";

    for (const auto& r : all) {
        std::string state = r.enabled ? "enabled" : "disabled";
        std::cout << "  " << std::setw(12) << std::left << r.id
                  << "  " << std::setw(20) << std::left << r.name
                  << "  " << std::setw(20) << std::left << r.pattern
                  << "  -> " << r.target_path
                  << "  [" << state << "]"
                  << "\n";
    }
    std::cout << "\n";

    return 0;
}

static int cmd_add_rule(CliContext& ctx) {
    if (ctx.args.size() < 2) {
        std::cerr << "Error: add-rule requires a pattern and target path\n";
        std::cerr << "Usage: archive add-rule <pattern> <target>\n";
        return 2;
    }

    ctx.config.ensure_directories();
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ClassificationRuleRepository rules(db);

    core::ClassificationRule rule;
    rule.name = "user-rule";
    rule.pattern = ctx.args[0];
    rule.target_path = ctx.args[1];
    rule.priority = 100;
    rule.enabled = true;

    rules.insert(rule);

    std::cout << "Rule added: " << rule.pattern << " -> " << rule.target_path << "\n"
              << "  ID: " << rule.id << "\n";

    return 0;
}

static void print_taxonomy_tree(const std::vector<core::TaxonomyNode>& nodes) {
    std::map<std::string, std::vector<const core::TaxonomyNode*>> children_by_parent;
    std::vector<const core::TaxonomyNode*> roots;

    for (const auto& n : nodes) {
        if (n.parent_id.empty()) {
            roots.push_back(&n);
        } else {
            children_by_parent[n.parent_id].push_back(&n);
        }
    }

    std::sort(roots.begin(), roots.end(),
              [](const auto* a, const auto* b) { return a->name < b->name; });

    auto print_node = [&](const core::TaxonomyNode* node, const std::string& prefix, bool is_last) {
        std::string connector = is_last ? "\u2514\u2500\u2500 " : "\u251C\u2500\u2500 ";
        std::string icon = node->icon.empty() ? "" : node->icon + " ";
        std::string count_str = node->file_count > 0
            ? " (" + std::to_string(node->file_count) + ")" : "";
        std::cout << prefix << connector << icon << node->name << count_str << "\n";
    };

    std::function<void(const core::TaxonomyNode*, const std::string&, bool)> print_children =
        [&](const core::TaxonomyNode* node, const std::string& prefix, bool is_last) {
            auto it = children_by_parent.find(node->id);
            if (it == children_by_parent.end()) return;

            auto children = it->second;
            std::sort(children.begin(), children.end(),
                      [](const auto* a, const auto* b) { return a->name < b->name; });

            for (size_t i = 0; i < children.size(); i++) {
                bool last = (i == children.size() - 1);
                std::string next_prefix = prefix + (is_last ? "    " : "\u2502   ");
                print_node(children[i], next_prefix, last);
                print_children(children[i], next_prefix, last);
            }
        };

    for (size_t i = 0; i < roots.size(); i++) {
        bool last = (i == roots.size() - 1);
        print_node(roots[i], "", last);
        print_children(roots[i], "", last);
    }
}

static int cmd_taxonomy(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::TaxonomyRepository taxonomy(db);
    auto nodes = taxonomy.find_all();

    if (nodes.empty()) {
        std::cout << "Taxonomy is empty.\n";
        return 0;
    }

    std::cout << "Taxonomy (" << nodes.size() << " nodes):\n\n";
    print_taxonomy_tree(nodes);
    std::cout << "\n";

    return 0;
}

int main(int argc, char* argv[]) {
    CliContext ctx;

    if (!parse_args(argc, argv, ctx)) {
        return 2;
    }

    if (ctx.command == "help" || ctx.command == "-h" || ctx.command == "--help") {
        print_usage();
        return 0;
    }

    if (ctx.command == "version" || ctx.command == "--version") {
        print_version();
        return 0;
    }

    if (ctx.command == "analyze")    return cmd_analyze(ctx);
    if (ctx.command == "plan")       return cmd_plan(ctx);
    if (ctx.command == "preview")    return cmd_preview(ctx);
    if (ctx.command == "organize")   return cmd_organize(ctx);
    if (ctx.command == "undo")       return cmd_undo(ctx);
    if (ctx.command == "list-plans") return cmd_list_plans(ctx);
    if (ctx.command == "list-undos") return cmd_list_undos(ctx);
    if (ctx.command == "rules")      return cmd_rules(ctx);
    if (ctx.command == "add-rule")   return cmd_add_rule(ctx);
    if (ctx.command == "taxonomy")   return cmd_taxonomy(ctx);

    std::cerr << "Unknown command: " << ctx.command << "\n";
    std::cerr << "Run 'archive help' for usage.\n";
    return 2;
}
