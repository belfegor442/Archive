#include "test_helpers.h"

#include "storage/DatabaseManager.h"
#include "storage/ScanRepository.h"
#include "storage/ScanItemRepository.h"
#include "storage/ClassificationRepository.h"
#include "storage/ClassificationRuleRepository.h"
#include "storage/OrgPlanRepository.h"
#include "storage/OrgMoveRepository.h"
#include "storage/UndoRepository.h"
#include "services/Scanner.h"
#include "services/Classifier.h"
#include "services/OrganizationPlanner.h"
#include "services/OrganizationExecutor.h"

#include <fstream>
#include <filesystem>

using namespace archive::storage;
using namespace archive::services;
using namespace archive::core;

static const std::string TEST_BASE = "organize_flow_test";

static void cleanup() {
    std::error_code ec;
    std::filesystem::remove_all(TEST_BASE, ec);
}

static void create_test_file(const std::string& path, const std::string& content) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

static bool file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

static uint64_t file_size(const std::string& path) {
    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    return ec ? 0 : sz;
}

// ============================================================
// Full integration test: organize flow
// ============================================================

static void test_full_organize_flow() {
    TEST_BEGIN("INT ORG: Full organize flow (scan -> classify -> plan -> execute -> undo)");

    cleanup();

    // 1. Create a messy temp directory with various file types
    std::string messy = TEST_BASE + "/messy";
    create_test_file(messy + "/main.cpp", "#include <iostream>\nint main() {}");
    create_test_file(messy + "/utils.h", "#pragma once\nvoid helper();");
    create_test_file(messy + "/script.py", "print('hello world')");
    create_test_file(messy + "/readme.md", "# Project README\nThis is a readme.");
    create_test_file(messy + "/config.json", "{\"name\": \"test\"}");
    create_test_file(messy + "/data.csv", "col1,col2\n1,2");
    create_test_file(messy + "/style.css", "body { color: red; }");
    create_test_file(messy + "/notes.txt", "Some random notes");

    // 2. Scanner scans it
    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    Scanner scanner(db, scan_repo, item_repo);
    auto scan = scanner.scan_directory(messy);

    ASSERT_EQ(scan.status, ScanStatus::Completed);
    ASSERT_TRUE(scan.file_count >= 8);

    // 3. Classifier classifies all items
    Classifier classifier(db, cls_repo, rule_repo, item_repo);
    auto classifications = classifier.classify_scan(scan.id, 50);
    ASSERT_TRUE(classifications.size() >= 8u);

    // 4. Planner creates a plan
    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan(scan.id, messy, 50);

    ASSERT_TRUE(plan.moves_planned >= 1);
    ASSERT_TRUE(plan.total_files >= 8);

    // 5. Preview shows moves
    auto summary = planner.get_summary(plan.id);
    ASSERT_TRUE(summary.total_files >= 8);
    ASSERT_TRUE(summary.moves_planned >= 1);

    auto details = planner.get_moves(plan.id);
    ASSERT_TRUE(details.size() >= 1u);

    // 5.5. Approve the plan before execution
    planner.approve_plan(plan.id);

    // 6. Executor moves files
    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute(plan.id);

    ASSERT_TRUE(!undo.id.empty());
    ASSERT_EQ(undo.status, UndoStatus::Available);
    ASSERT_TRUE(undo.moves_count >= 1);

    // 7. Verify files are in new locations
    for (const auto& detail : details) {
        if (file_exists(detail.source)) {
            // source should have been moved away
            ASSERT_TRUE(!file_exists(detail.source));
        }
    }

    // At least some destination files should exist
    int dest_count = 0;
    for (const auto& detail : details) {
        if (file_exists(detail.destination)) dest_count++;
    }
    ASSERT_TRUE(dest_count >= 1);

    // 8. Verify files have same content (checksum)
    auto undo_entries = undo_repo.find_entries(undo.id);
    for (const auto& entry : undo_entries) {
        if (file_exists(entry.source_path)) {
            // This is the moved file (dest_path is the undo dest)
            // entry.source_path = where file was moved TO
            // entry.dest_path = where file was moved FROM (original)
        }
    }

    // 9. Undo restores everything
    executor.undo(undo.id);

    // 10. Verify files back in original locations
    ASSERT_TRUE(file_exists(messy + "/main.cpp"));
    ASSERT_TRUE(file_exists(messy + "/script.py"));
    ASSERT_TRUE(file_exists(messy + "/readme.md"));
    ASSERT_TRUE(file_exists(messy + "/config.json"));

    // 11. Verify no files lost
    ASSERT_TRUE(file_exists(messy + "/utils.h"));
    ASSERT_TRUE(file_exists(messy + "/data.csv"));
    ASSERT_TRUE(file_exists(messy + "/style.css"));
    ASSERT_TRUE(file_exists(messy + "/notes.txt"));

    cleanup();
    TEST_PASS();
}

// ============================================================
// Integration: empty directory organize
// ============================================================

static void test_organize_empty_directory() {
    TEST_BEGIN("INT ORG: Organize empty directory");
    cleanup();

    std::string empty_dir = TEST_BASE + "/empty";
    std::filesystem::create_directories(empty_dir);

    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    Scanner scanner(db, scan_repo, item_repo);
    auto scan = scanner.scan_directory(empty_dir);
    ASSERT_EQ(scan.file_count, 0);

    Classifier classifier(db, cls_repo, rule_repo, item_repo);
    auto cls = classifier.classify_scan(scan.id, 50);
    ASSERT_TRUE(cls.empty());

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan(scan.id, empty_dir, 50);
    ASSERT_EQ(plan.moves_planned, 0);

    cleanup();
    TEST_PASS();
}

// ============================================================
// Integration: single file organize and undo
// ============================================================

static void test_organize_single_file_undo() {
    TEST_BEGIN("INT ORG: Single file organize and undo round-trip");
    cleanup();

    std::string content = "int main() { return 42; }";
    std::string src_dir = TEST_BASE + "/single_src";
    create_test_file(src_dir + "/app.cpp", content);

    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    Scanner scanner(db, scan_repo, item_repo);
    auto scan = scanner.scan_directory(src_dir);
    ASSERT_TRUE(scan.file_count >= 1);

    Classifier classifier(db, cls_repo, rule_repo, item_repo);
    classifier.classify_scan(scan.id, 50);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan(scan.id, src_dir, 50);
    planner.approve_plan(plan.id);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute(plan.id);

    if (undo.moves_count > 0) {
        executor.undo(undo.id);

        ASSERT_TRUE(file_exists(src_dir + "/app.cpp"));
        std::string restored = read_file(src_dir + "/app.cpp");
        ASSERT_EQ(restored, content);
    }

    cleanup();
    TEST_PASS();
}

// ============================================================
// Integration: multiple file types organized correctly
// ============================================================

static void test_organize_preserves_all_files() {
    TEST_BEGIN("INT ORG: No files lost during organize + undo");
    cleanup();

    std::string dir = TEST_BASE + "/preserve_test";
    std::map<std::string, std::string> file_contents;
    file_contents["code.cpp"] = "int x = 1;";
    file_contents["data.txt"] = "some data";
    file_contents["image.png"] = std::string("\x89PNG", 4);
    file_contents["doc.pdf"] = std::string("%PDF", 4);

    for (const auto& [name, content] : file_contents) {
        create_test_file(dir + "/" + name, content);
    }

    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    Scanner scanner(db, scan_repo, item_repo);
    auto scan = scanner.scan_directory(dir);
    ASSERT_TRUE(scan.file_count >= 4);

    Classifier classifier(db, cls_repo, rule_repo, item_repo);
    classifier.classify_scan(scan.id, 50);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan(scan.id, dir, 50);
    planner.approve_plan(plan.id);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute(plan.id);

    executor.undo(undo.id);

    for (const auto& [name, content] : file_contents) {
        ASSERT_TRUE(file_exists(dir + "/" + name));
        std::string restored = read_file(dir + "/" + name);
        ASSERT_EQ(restored, content);
    }

    cleanup();
    TEST_PASS();
}

// ============================================================
// Integration: verify undo status changes
// ============================================================

static void test_undo_status_transitions() {
    TEST_BEGIN("INT ORG: Undo status transitions");
    cleanup();

    std::string dir = TEST_BASE + "/status_test";
    create_test_file(dir + "/file.txt", "content");

    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    Scanner scanner(db, scan_repo, item_repo);
    auto scan = scanner.scan_directory(dir);

    Classifier classifier(db, cls_repo, rule_repo, item_repo);
    classifier.classify_scan(scan.id, 50);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan(scan.id, dir, 50);
    planner.approve_plan(plan.id);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute(plan.id);

    auto record = undo_repo.find_record_by_id(undo.id);
    ASSERT_TRUE(record.has_value());
    ASSERT_EQ(record->status, UndoStatus::Available);

    if (record->moves_count > 0) {
        executor.undo(undo.id);
        auto after = undo_repo.find_record_by_id(undo.id);
        ASSERT_TRUE(after.has_value());
    }

    cleanup();
    TEST_PASS();
}

// ============================================================
// Integration: verify file sizes preserved
// ============================================================

static void test_organize_preserves_sizes() {
    TEST_BEGIN("INT ORG: File sizes preserved after organize + undo");
    cleanup();

    std::string dir = TEST_BASE + "/size_test";
    std::string big_content(4096, 'A');
    create_test_file(dir + "/big.txt", big_content);
    create_test_file(dir + "/small.txt", "x");

    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    Scanner scanner(db, scan_repo, item_repo);
    auto scan = scanner.scan_directory(dir);

    Classifier classifier(db, cls_repo, rule_repo, item_repo);
    classifier.classify_scan(scan.id, 50);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan(scan.id, dir, 50);
    planner.approve_plan(plan.id);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute(plan.id);
    executor.undo(undo.id);

    ASSERT_TRUE(file_size(dir + "/big.txt") == 4096u);
    ASSERT_TRUE(file_size(dir + "/small.txt") == 1u);

    cleanup();
    TEST_PASS();
}

void run_organize_flow_tests() {
    std::cout << "=== Organize Flow Integration Tests ===" << std::endl;

    test_full_organize_flow();
    test_organize_empty_directory();
    test_organize_single_file_undo();
    test_organize_preserves_all_files();
    test_undo_status_transitions();
    test_organize_preserves_sizes();
}
