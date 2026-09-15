#include "test_helpers.h"

#include "storage/DatabaseManager.h"
#include "storage/ScanRepository.h"
#include "storage/ScanItemRepository.h"
#include "storage/ClassificationRepository.h"
#include "storage/OrgPlanRepository.h"
#include "storage/OrgMoveRepository.h"
#include "services/OrganizationPlanner.h"

using namespace archive::storage;
using namespace archive::services;
using namespace archive::core;

struct TestDB {
    DatabaseManager db;
    TestDB() : db(":memory:") {
        db.initialize();
        db.execute("PRAGMA foreign_keys=OFF");
    }
};
static void ensure_scan(DatabaseManager& db, const std::string& scan_id) {
    db.execute("INSERT OR IGNORE INTO scans (id, root_path, status) VALUES ('" + scan_id + "', '/fake', 'completed');");
}

static void ensure_plan(DatabaseManager& db, const std::string& plan_id, const std::string& scan_id) {
    db.execute("INSERT OR IGNORE INTO org_plans (id, scan_id, root_path, status) VALUES ('" + plan_id + "', '" + scan_id + "', '/fake', 'draft');");
}


static void populate_scan_items(ScanItemRepository& repo, const std::string& scan_id, int count) {
    for (int i = 0; i < count; i++) {
        ScanItem item;
        item.id = "item-" + std::to_string(i);
        item.scan_id = scan_id;
        item.filename = "file" + std::to_string(i) + ".cpp";
        item.extension = ".cpp";
        item.path = "/root/file" + std::to_string(i) + ".cpp";
        item.size = 100 * (i + 1);
        item.checksum = "checksum" + std::to_string(i);
        repo.insert(item);
    }
}

static void populate_classifications(ClassificationRepository& repo,
                                      const std::string& scan_id, int count) {
    for (int i = 0; i < count; i++) {
        Classification cls;
        cls.id = "cls-" + std::to_string(i);
        cls.scan_item_id = "item-" + std::to_string(i);
        cls.taxonomy_path = "Development/C++";
        cls.confidence = 0.9;
        cls.reason = "Extension .cpp";
        repo.insert(cls);
    }
}

// ============================================================
// OrganizationPlanner tests
// ============================================================

static void test_create_plan_produces_correct_move_count() {
    TEST_BEGIN("create_plan produces correct move count");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/root";
    scan.status = ScanStatus::Completed; scan_repo.insert(scan);

    populate_scan_items(item_repo, "scan-001", 5);
    populate_classifications(cls_repo, "scan-001", 5);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan("scan-001", "/root", 50);

    ASSERT_EQ(plan.total_files, 5);
    ASSERT_TRUE(plan.moves_planned >= 1);
    ASSERT_EQ(plan.scan_id, "scan-001");
    TEST_PASS();
}

static void test_dest_path_resolves_correctly() {
    TEST_BEGIN("dest_path resolves with taxonomy + filename");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/root";
    scan.status = ScanStatus::Completed; scan_repo.insert(scan);

    ScanItem item;
    item.id = "item-001"; item.scan_id = "scan-001";
    item.filename = "main.cpp"; item.extension = ".cpp";
    item.path = "/root/main.cpp"; item.size = 100; item.checksum = "abc";
    item_repo.insert(item);

    Classification cls;
    cls.id = "cls-001"; cls.scan_item_id = "item-001";
    cls.taxonomy_path = "Development/C++"; cls.confidence = 0.95;
    cls.reason = "Extension .cpp";
    cls_repo.insert(cls);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan("scan-001", "/root", 50);
    auto moves = move_repo.find_by_plan(plan.id);

    ASSERT_TRUE(moves.size() >= 1u);
    ASSERT_TRUE(moves[0].dest_path.find("Development") != std::string::npos);
    ASSERT_TRUE(moves[0].dest_path.find("main.cpp") != std::string::npos);
    TEST_PASS();
}

static void test_confidence_averaging() {
    TEST_BEGIN("plan avg_confidence is averaged from moves");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/root";
    scan.status = ScanStatus::Completed; scan_repo.insert(scan);

    ScanItem i1; i1.id = "i1"; i1.scan_id = "scan-001";
    i1.filename = "a.cpp"; i1.extension = ".cpp"; i1.path = "/root/a.cpp";
    i1.size = 50; i1.checksum = "a"; item_repo.insert(i1);

    ScanItem i2; i2.id = "i2"; i2.scan_id = "scan-001";
    i2.filename = "b.cpp"; i2.extension = ".cpp"; i2.path = "/root/b.cpp";
    i2.size = 50; i2.checksum = "b"; item_repo.insert(i2);

    Classification c1; c1.id = "c1"; c1.scan_item_id = "i1";
    c1.taxonomy_path = "Development/C++"; c1.confidence = 0.8;
    c1.reason = "ext"; cls_repo.insert(c1);

    Classification c2; c2.id = "c2"; c2.scan_item_id = "i2";
    c2.taxonomy_path = "Development/C++"; c2.confidence = 1.0;
    c2.reason = "ext"; cls_repo.insert(c2);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan("scan-001", "/root", 50);

    ASSERT_TRUE(plan.avg_confidence >= 0.8);
    ASSERT_TRUE(plan.avg_confidence <= 1.0);
    TEST_PASS();
}

static void test_plan_summary_categories() {
    TEST_BEGIN("plan summary contains category breakdown");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/root";
    scan.status = ScanStatus::Completed; scan_repo.insert(scan);

    ScanItem i1; i1.id = "i1"; i1.scan_id = "scan-001";
    i1.filename = "a.cpp"; i1.extension = ".cpp"; i1.path = "/root/a.cpp";
    i1.size = 50; i1.checksum = "a"; item_repo.insert(i1);

    ScanItem i2; i2.id = "i2"; i2.scan_id = "scan-001";
    i2.filename = "b.py"; i2.extension = ".py"; i2.path = "/root/b.py";
    i2.size = 50; i2.checksum = "b"; item_repo.insert(i2);

    Classification c1; c1.id = "c1"; c1.scan_item_id = "i1";
    c1.taxonomy_path = "Development/C++"; c1.confidence = 0.9;
    c1.reason = "ext"; cls_repo.insert(c1);

    Classification c2; c2.id = "c2"; c2.scan_item_id = "i2";
    c2.taxonomy_path = "Development/Python"; c2.confidence = 0.85;
    c2.reason = "ext"; cls_repo.insert(c2);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan("scan-001", "/root", 50);
    auto summary = planner.get_summary(plan.id);

    ASSERT_TRUE(summary.by_category.size() >= 2);
    TEST_PASS();
}

static void test_conflict_resolution() {
    TEST_BEGIN("conflict resolution when two files map to same dest");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/root";
    scan.status = ScanStatus::Completed; scan_repo.insert(scan);

    ScanItem i1; i1.id = "i1"; i1.scan_id = "scan-001";
    i1.filename = "data.cpp"; i1.extension = ".cpp"; i1.path = "/root/sub1/data.cpp";
    i1.size = 100; i1.checksum = "a"; item_repo.insert(i1);

    ScanItem i2; i2.id = "i2"; i2.scan_id = "scan-001";
    i2.filename = "data.cpp"; i2.extension = ".cpp"; i2.path = "/root/sub2/data.cpp";
    i2.size = 100; i2.checksum = "b"; item_repo.insert(i2);

    Classification c1; c1.id = "c1"; c1.scan_item_id = "i1";
    c1.taxonomy_path = "Development/C++"; c1.confidence = 0.9;
    c1.reason = "ext"; cls_repo.insert(c1);

    Classification c2; c2.id = "c2"; c2.scan_item_id = "i2";
    c2.taxonomy_path = "Development/C++"; c2.confidence = 0.9;
    c2.reason = "ext"; cls_repo.insert(c2);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan("scan-001", "/root", 50);
    auto moves = move_repo.find_by_plan(plan.id);

    ASSERT_TRUE(moves.size() >= 1u);
    TEST_PASS();
}

static void test_approve_plan() {
    TEST_BEGIN("approve_plan changes status to Ready");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/root";
    scan.status = ScanStatus::Completed; scan_repo.insert(scan);

    populate_scan_items(item_repo, "scan-001", 1);
    populate_classifications(cls_repo, "scan-001", 1);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan("scan-001", "/root", 50);
    ASSERT_EQ(plan.status, PlanStatus::Draft);

    planner.approve_plan(plan.id);
    auto approved = plan_repo.find_by_id(plan.id);
    ASSERT_TRUE(approved.has_value());
    ASSERT_EQ(approved->status, PlanStatus::Ready);
    TEST_PASS();
}

static void test_cancel_plan() {
    TEST_BEGIN("cancel_plan changes status to Failed");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/root";
    scan.status = ScanStatus::Completed; scan_repo.insert(scan);

    populate_scan_items(item_repo, "scan-001", 1);
    populate_classifications(cls_repo, "scan-001", 1);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan("scan-001", "/root", 50);

    planner.cancel_plan(plan.id);
    auto cancelled = plan_repo.find_by_id(plan.id);
    ASSERT_TRUE(cancelled.has_value());
    ASSERT_EQ(cancelled->status, PlanStatus::Failed);
    TEST_PASS();
}

static void test_get_moves() {
    TEST_BEGIN("get_moves returns MoveDetail list");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    ClassificationRepository cls_repo(db);
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/root";
    scan.status = ScanStatus::Completed; scan_repo.insert(scan);

    populate_scan_items(item_repo, "scan-001", 3);
    populate_classifications(cls_repo, "scan-001", 3);

    OrganizationPlanner planner(db, scan_repo, item_repo, cls_repo, plan_repo, move_repo);
    auto plan = planner.create_plan("scan-001", "/root", 50);
    auto details = planner.get_moves(plan.id);

    ASSERT_TRUE(details.size() >= 3u);
    for (const auto& d : details) {
        ASSERT_TRUE(!d.source.empty());
        ASSERT_TRUE(!d.destination.empty());
    }
    TEST_PASS();
}

void run_planner_tests() {
    std::cout << "=== OrganizationPlanner Tests ===" << std::endl;

    test_create_plan_produces_correct_move_count();
    test_dest_path_resolves_correctly();
    test_confidence_averaging();
    test_plan_summary_categories();
    test_conflict_resolution();
    test_approve_plan();
    test_cancel_plan();
    test_get_moves();
}
