#include "test_helpers.h"

#include "storage/DatabaseManager.h"
#include "storage/ScanRepository.h"
#include "storage/ScanItemRepository.h"
#include "storage/ClassificationRepository.h"
#include "storage/TaxonomyRepository.h"
#include "storage/ClassificationRuleRepository.h"
#include "storage/OrgPlanRepository.h"
#include "storage/OrgMoveRepository.h"
#include "storage/UndoRepository.h"

using namespace archive::storage;
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

static void ensure_item(DatabaseManager& db, const std::string& scan_id, const std::string& item_id) {
    ensure_scan(db, scan_id);
    db.execute("INSERT OR IGNORE INTO scan_items (id, scan_id, path, filename) VALUES ('" + item_id + "', '" + scan_id + "', '/fake/" + item_id + "', '" + item_id + "');");
}

static void ensure_plan(DatabaseManager& db, const std::string& plan_id, const std::string& scan_id) {
    ensure_scan(db, scan_id);
    db.execute("INSERT OR IGNORE INTO org_plans (id, scan_id, root_path, status) VALUES ('" + plan_id + "', '" + scan_id + "', '/fake', 'draft');");
}


// ============================================================
// ScanRepository tests
// ============================================================

static void test_scan_insert_and_find() {
    TEST_BEGIN("ScanRepository insert and find_by_id");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository repo(db);

    Scan scan;
    scan.id = "scan-001";
    scan.root_path = "/home/user/projects";
    scan.status = ScanStatus::Pending;
    scan.file_count = 0;
    scan.folder_count = 0;
    scan.started_at = "";
    scan.completed_at = "";
    repo.insert(scan);

    auto found = repo.find_by_id("scan-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->id, "scan-001");
    ASSERT_EQ(found->root_path, "/home/user/projects");
    ASSERT_EQ(found->status, ScanStatus::Pending);
    TEST_PASS();
}

static void test_scan_find_all() {
    TEST_BEGIN("ScanRepository find_all");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository repo(db);

    Scan s1; s1.id = "scan-1"; s1.root_path = "/a"; repo.insert(s1);
    Scan s2; s2.id = "scan-2"; s2.root_path = "/b"; repo.insert(s2);

    auto all = repo.find_all();
    ASSERT_EQ(all.size(), 2u);
    TEST_PASS();
}

static void test_scan_update_status() {
    TEST_BEGIN("ScanRepository update_status");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/a"; repo.insert(scan);
    repo.update_status("scan-001", ScanStatus::Running);

    auto found = repo.find_by_id("scan-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->status, ScanStatus::Running);
    TEST_PASS();
}

static void test_scan_set_completed() {
    TEST_BEGIN("ScanRepository set_completed");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository repo(db);

    Scan scan; scan.id = "scan-001"; scan.root_path = "/a"; repo.insert(scan);
    repo.set_completed("scan-001", 42, 7);

    auto found = repo.find_by_id("scan-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->status, ScanStatus::Completed);
    ASSERT_EQ(found->file_count, 42);
    ASSERT_EQ(found->folder_count, 7);
    TEST_PASS();
}

static void test_scan_find_by_id_not_found() {
    TEST_BEGIN("ScanRepository find_by_id returns nullopt for missing");
    TestDB tdb;
    auto& db = tdb.db;
    ScanRepository repo(db);

    auto found = repo.find_by_id("nonexistent");
    ASSERT_TRUE(!found.has_value());
    TEST_PASS();
}

// ============================================================
// ScanItemRepository tests
// ============================================================

static void test_scan_item_insert_and_find() {
    TEST_BEGIN("ScanItemRepository insert and find_by_id");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_scan(db, "scan-001");
    ScanItemRepository repo(db);

    ScanItem item;
    item.id = "item-001";
    item.scan_id = "scan-001";
    item.path = "/home/user/main.cpp";
    item.filename = "main.cpp";
    item.extension = ".cpp";
    item.mime_type = "text/x-c++src";
    item.size = 1024;
    item.role = "Source";
    repo.insert(item);

    auto found = repo.find_by_id("item-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->id, "item-001");
    ASSERT_EQ(found->scan_id, "scan-001");
    ASSERT_EQ(found->filename, "main.cpp");
    ASSERT_EQ(found->extension, ".cpp");
    ASSERT_EQ(found->size, 1024);
    TEST_PASS();
}

static void test_scan_item_find_by_scan() {
    TEST_BEGIN("ScanItemRepository find_by_scan");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_scan(db, "scan-001");
    ensure_scan(db, "scan-002");
    ScanItemRepository repo(db);

    ScanItem i1; i1.id = "i1"; i1.scan_id = "scan-001"; i1.filename = "a.cpp"; repo.insert(i1);
    ScanItem i2; i2.id = "i2"; i2.scan_id = "scan-001"; i2.filename = "b.py"; repo.insert(i2);
    ScanItem i3; i3.id = "i3"; i3.scan_id = "scan-002"; i3.filename = "c.txt"; repo.insert(i3);

    auto items = repo.find_by_scan("scan-001");
    ASSERT_EQ(items.size(), 2u);
    TEST_PASS();
}

static void test_scan_item_count_by_scan() {
    TEST_BEGIN("ScanItemRepository count_by_scan");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_scan(db, "scan-001");
    ensure_scan(db, "scan-999");
    ScanItemRepository repo(db);

    ScanItem i1; i1.id = "i1"; i1.scan_id = "scan-001"; repo.insert(i1);
    ScanItem i2; i2.id = "i2"; i2.scan_id = "scan-001"; repo.insert(i2);
    ScanItem i3; i3.id = "i3"; i3.scan_id = "scan-001"; repo.insert(i3);

    ASSERT_EQ(repo.count_by_scan("scan-001"), 3);
    ASSERT_EQ(repo.count_by_scan("scan-999"), 0);
    TEST_PASS();
}

static void test_scan_item_clear_scan() {
    TEST_BEGIN("ScanItemRepository clear_scan");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_scan(db, "scan-001");
    ensure_scan(db, "scan-002");
    ScanItemRepository repo(db);

    ScanItem i1; i1.id = "i1"; i1.scan_id = "scan-001"; repo.insert(i1);
    ScanItem i2; i2.id = "i2"; i2.scan_id = "scan-001"; repo.insert(i2);
    ScanItem i3; i3.id = "i3"; i3.scan_id = "scan-002"; repo.insert(i3);

    repo.clear_scan("scan-001");
    ASSERT_EQ(repo.count_by_scan("scan-001"), 0);
    ASSERT_EQ(repo.count_by_scan("scan-002"), 1);
    TEST_PASS();
}

static void test_scan_item_insert_batch() {
    TEST_BEGIN("ScanItemRepository insert_batch");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_scan(db, "scan-batch");
    ScanItemRepository repo(db);

    std::vector<ScanItem> items;
    for (int i = 0; i < 5; i++) {
        ScanItem item;
        item.id = "batch-" + std::to_string(i);
        item.scan_id = "scan-batch";
        item.filename = "file" + std::to_string(i) + ".txt";
        items.push_back(item);
    }
    repo.insert_batch(items);

    ASSERT_EQ(repo.count_by_scan("scan-batch"), 5);
    TEST_PASS();
}

// ============================================================
// ClassificationRepository tests
// ============================================================

static void test_classification_insert_and_find() {
    TEST_BEGIN("ClassificationRepository insert and find_by_scan_item");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_item(db, "scan-001", "item-001");
    ClassificationRepository repo(db);

    Classification cls;
    cls.id = "cls-001";
    cls.scan_item_id = "item-001";
    cls.taxonomy_path = "Development/C++";
    cls.confidence = 0.95;
    cls.reason = "Extension .cpp";
    repo.insert(cls);

    auto found = repo.find_by_scan_item("item-001");
    ASSERT_EQ(found.size(), 1u);
    ASSERT_EQ(found[0].taxonomy_path, "Development/C++");
    TEST_PASS();
}

static void test_classification_find_best() {
    TEST_BEGIN("ClassificationRepository find_best returns highest confidence");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_item(db, "scan-001", "item-001");
    ClassificationRepository repo(db);

    Classification c1;
    c1.id = "cls-low"; c1.scan_item_id = "item-001";
    c1.taxonomy_path = "Documents"; c1.confidence = 0.3;
    repo.insert(c1);

    Classification c2;
    c2.id = "cls-high"; c2.scan_item_id = "item-001";
    c2.taxonomy_path = "Development/C++"; c2.confidence = 0.95;
    repo.insert(c2);

    auto best = repo.find_best("item-001");
    ASSERT_TRUE(best.has_value());
    ASSERT_EQ(best->id, "cls-high");
    ASSERT_TRUE(best->confidence > 0.9);
    TEST_PASS();
}

static void test_classification_clear_scan() {
    TEST_BEGIN("ClassificationRepository clear_scan");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_item(db, "scan-001", "item-001");
    ensure_item(db, "scan-001", "item-002");
    ClassificationRepository repo(db);

    Classification c1; c1.id = "c1"; c1.scan_item_id = "item-001";
    c1.taxonomy_path = "A"; repo.insert(c1);

    Classification c2; c2.id = "c2"; c2.scan_item_id = "item-002";
    c2.taxonomy_path = "B"; repo.insert(c2);

    repo.clear_scan("scan-001");
    auto remaining = repo.find_by_scan("scan-001");
    ASSERT_TRUE(remaining.empty());
    TEST_PASS();
}

static void test_classification_insert_batch() {
    TEST_BEGIN("ClassificationRepository insert_batch");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_item(db, "scan-001", "item-0");
    ensure_item(db, "scan-001", "item-1");
    ensure_item(db, "scan-001", "item-2");
    ClassificationRepository repo(db);

    std::vector<Classification> batch;
    for (int i = 0; i < 3; i++) {
        Classification cls;
        cls.id = "cls-batch-" + std::to_string(i);
        cls.scan_item_id = "item-" + std::to_string(i);
        cls.taxonomy_path = "Category";
        cls.confidence = 0.8;
        batch.push_back(cls);
    }
    repo.insert_batch(batch);

    auto all = repo.find_by_scan("scan-001");
    ASSERT_TRUE(all.size() >= 3u);
    TEST_PASS();
}

// ============================================================
// TaxonomyRepository tests
// ============================================================

static void test_taxonomy_insert_and_find() {
    TEST_BEGIN("TaxonomyRepository insert and find_by_id");
    TestDB tdb;
    auto& db = tdb.db;
    TaxonomyRepository repo(db);

    TaxonomyNode node;
    node.id = "tax-001";
    node.name = "Development";
    node.parent_id = "";
    node.level = 0;
    node.file_count = 0;
    node.icon = "code";
    repo.insert(node);

    auto found = repo.find_by_id("tax-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->name, "Development");
    ASSERT_EQ(found->level, 0);
    TEST_PASS();
}

static void test_taxonomy_find_children() {
    TEST_BEGIN("TaxonomyRepository find_children");
    TestDB tdb;
    auto& db = tdb.db;
    TaxonomyRepository repo(db);

    TaxonomyNode parent; parent.id = "parent"; parent.name = "Development";
    parent.level = 0; repo.insert(parent);

    TaxonomyNode c1; c1.id = "child1"; c1.name = "C++";
    c1.parent_id = "parent"; c1.level = 1; repo.insert(c1);

    TaxonomyNode c2; c2.id = "child2"; c2.name = "Python";
    c2.parent_id = "parent"; c2.level = 1; repo.insert(c2);

    TaxonomyNode c3; c3.id = "other"; c3.name = "Images";
    c3.level = 0; repo.insert(c3);

    auto children = repo.find_children("parent");
    ASSERT_EQ(children.size(), 2u);
    TEST_PASS();
}

static void test_taxonomy_find_root_nodes() {
    TEST_BEGIN("TaxonomyRepository find_root_nodes");
    TestDB tdb;
    auto& db = tdb.db;
    TaxonomyRepository repo(db);

    TaxonomyNode r1; r1.id = "r1"; r1.name = "Development"; r1.level = 0; repo.insert(r1);
    TaxonomyNode r2; r2.id = "r2"; r2.name = "Documents"; r2.level = 0; repo.insert(r2);

    TaxonomyNode child; child.id = "c1"; child.name = "C++";
    child.parent_id = "r1"; child.level = 1; repo.insert(child);

    auto roots = repo.find_root_nodes();
    ASSERT_EQ(roots.size(), 2u);
    TEST_PASS();
}

static void test_taxonomy_clear() {
    TEST_BEGIN("TaxonomyRepository clear");
    TestDB tdb;
    auto& db = tdb.db;
    TaxonomyRepository repo(db);

    TaxonomyNode n1; n1.id = "n1"; n1.name = "A"; repo.insert(n1);
    TaxonomyNode n2; n2.id = "n2"; n2.name = "B"; repo.insert(n2);

    repo.clear();
    auto all = repo.find_all();
    ASSERT_TRUE(all.empty());
    TEST_PASS();
}

static void test_taxonomy_increment_count() {
    TEST_BEGIN("TaxonomyRepository increment_count");
    TestDB tdb;
    auto& db = tdb.db;
    TaxonomyRepository repo(db);

    TaxonomyNode node; node.id = "tax-1"; node.name = "Dev";
    node.file_count = 5; repo.insert(node);

    repo.increment_count("tax-1");
    auto found = repo.find_by_id("tax-1");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->file_count, 6);
    TEST_PASS();
}

// ============================================================
// ClassificationRuleRepository tests
// ============================================================

static void test_rule_insert_and_find() {
    TEST_BEGIN("ClassificationRuleRepository insert and find_by_id");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRuleRepository repo(db);

    ClassificationRule rule;
    rule.id = "rule-001";
    rule.name = "C++ sources";
    rule.pattern = "*.cpp";
    rule.target_path = "Development/C++";
    rule.priority = 10;
    rule.enabled = true;
    repo.insert(rule);

    auto found = repo.find_by_id("rule-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->name, "C++ sources");
    ASSERT_EQ(found->pattern, "*.cpp");
    ASSERT_TRUE(found->enabled);
    TEST_PASS();
}

static void test_rule_find_all() {
    TEST_BEGIN("ClassificationRuleRepository find_all");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRuleRepository repo(db);

    ClassificationRule r1; r1.id = "r1"; r1.name = "A"; repo.insert(r1);
    ClassificationRule r2; r2.id = "r2"; r2.name = "B"; repo.insert(r2);

    auto all = repo.find_all();
    ASSERT_EQ(all.size(), 2u);
    TEST_PASS();
}

static void test_rule_find_enabled() {
    TEST_BEGIN("ClassificationRuleRepository find_enabled");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRuleRepository repo(db);

    ClassificationRule r1; r1.id = "r1"; r1.name = "Enabled"; r1.enabled = true; repo.insert(r1);
    ClassificationRule r2; r2.id = "r2"; r2.name = "Disabled"; r2.enabled = false; repo.insert(r2);

    auto enabled = repo.find_enabled();
    ASSERT_EQ(enabled.size(), 1u);
    ASSERT_EQ(enabled[0].name, "Enabled");
    TEST_PASS();
}

static void test_rule_remove() {
    TEST_BEGIN("ClassificationRuleRepository remove");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRuleRepository repo(db);

    ClassificationRule rule; rule.id = "rule-del"; rule.name = "DeleteMe"; repo.insert(rule);
    ASSERT_TRUE(repo.find_by_id("rule-del").has_value());

    repo.remove("rule-del");
    ASSERT_TRUE(!repo.find_by_id("rule-del").has_value());
    TEST_PASS();
}

// ============================================================
// OrgPlanRepository tests
// ============================================================

static void test_org_plan_insert_and_find() {
    TEST_BEGIN("OrgPlanRepository insert and find_by_id");
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository repo(db);

    OrgPlan plan;
    plan.id = "plan-001";
    plan.scan_id = "scan-001";
    plan.root_path = "/home/user";
    plan.status = PlanStatus::Draft;
    plan.total_files = 10;
    plan.moves_planned = 8;
    repo.insert(plan);

    auto found = repo.find_by_id("plan-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->id, "plan-001");
    ASSERT_EQ(found->total_files, 10);
    TEST_PASS();
}

static void test_org_plan_find_latest_by_root() {
    TEST_BEGIN("OrgPlanRepository find_latest_by_root");
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository repo(db);

    OrgPlan p1; p1.id = "plan-1"; p1.root_path = "/home/user";
    p1.created_at = "2025-01-01T00:00:00Z"; repo.insert(p1);

    OrgPlan p2; p2.id = "plan-2"; p2.root_path = "/home/user";
    p2.created_at = "2025-09-15T00:00:00Z"; repo.insert(p2);

    auto latest = repo.find_latest_by_root("/home/user");
    ASSERT_TRUE(latest.has_value());
    ASSERT_EQ(latest->id, "plan-2");
    TEST_PASS();
}

static void test_org_plan_find_all() {
    TEST_BEGIN("OrgPlanRepository find_all");
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository repo(db);

    OrgPlan p1; p1.id = "p1"; repo.insert(p1);
    OrgPlan p2; p2.id = "p2"; repo.insert(p2);
    OrgPlan p3; p3.id = "p3"; repo.insert(p3);

    auto all = repo.find_all();
    ASSERT_EQ(all.size(), 3u);
    TEST_PASS();
}

// ============================================================
// OrgMoveRepository tests
// ============================================================

static void test_org_move_insert_and_find() {
    TEST_BEGIN("OrgMoveRepository insert and find_by_id");
    TestDB tdb;
    auto& db = tdb.db;
    OrgMoveRepository repo(db);

    OrgMove move;
    move.id = "move-001";
    move.plan_id = "plan-001";
    move.scan_item_id = "item-001";
    move.source_path = "/src/main.cpp";
    move.dest_path = "/dest/Development/C++/main.cpp";
    move.confidence = 0.9;
    move.status = MoveStatus::Planned;
    repo.insert(move);

    auto found = repo.find_by_id("move-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->source_path, "/src/main.cpp");
    ASSERT_EQ(found->status, MoveStatus::Planned);
    TEST_PASS();
}

static void test_org_move_find_by_plan() {
    TEST_BEGIN("OrgMoveRepository find_by_plan");
    TestDB tdb;
    auto& db = tdb.db;
    OrgMoveRepository repo(db);

    OrgMove m1; m1.id = "m1"; m1.plan_id = "plan-1"; repo.insert(m1);
    OrgMove m2; m2.id = "m2"; m2.plan_id = "plan-1"; repo.insert(m2);
    OrgMove m3; m3.id = "m3"; m3.plan_id = "plan-2"; repo.insert(m3);

    auto moves = repo.find_by_plan("plan-1");
    ASSERT_EQ(moves.size(), 2u);
    TEST_PASS();
}

static void test_org_move_find_by_plan_and_status() {
    TEST_BEGIN("OrgMoveRepository find_by_plan_and_status");
    TestDB tdb;
    auto& db = tdb.db;
    OrgMoveRepository repo(db);

    OrgMove m1; m1.id = "m1"; m1.plan_id = "p1"; m1.status = MoveStatus::Planned; repo.insert(m1);
    OrgMove m2; m2.id = "m2"; m2.plan_id = "p1"; m2.status = MoveStatus::Completed; repo.insert(m2);
    OrgMove m3; m3.id = "m3"; m3.plan_id = "p1"; m3.status = MoveStatus::Planned; repo.insert(m3);

    auto planned = repo.find_by_plan_and_status("p1", MoveStatus::Planned);
    ASSERT_EQ(planned.size(), 2u);

    auto completed = repo.find_by_plan_and_status("p1", MoveStatus::Completed);
    ASSERT_EQ(completed.size(), 1u);
    TEST_PASS();
}

static void test_org_move_count_by_plan() {
    TEST_BEGIN("OrgMoveRepository count_by_plan");
    TestDB tdb;
    auto& db = tdb.db;
    OrgMoveRepository repo(db);

    OrgMove m1; m1.id = "m1"; m1.plan_id = "p1"; repo.insert(m1);
    OrgMove m2; m2.id = "m2"; m2.plan_id = "p1"; repo.insert(m2);
    OrgMove m3; m3.id = "m3"; m3.plan_id = "p2"; repo.insert(m3);

    ASSERT_EQ(repo.count_by_plan("p1"), 2);
    ASSERT_EQ(repo.count_by_plan("p2"), 1);
    TEST_PASS();
}

static void test_org_move_insert_batch() {
    TEST_BEGIN("OrgMoveRepository insert_batch");
    TestDB tdb;
    auto& db = tdb.db;
    OrgMoveRepository repo(db);

    std::vector<OrgMove> moves;
    for (int i = 0; i < 4; i++) {
        OrgMove m;
        m.id = "batch-" + std::to_string(i);
        m.plan_id = "plan-batch";
        m.status = MoveStatus::Planned;
        moves.push_back(m);
    }
    repo.insert_batch(moves);

    ASSERT_EQ(repo.count_by_plan("plan-batch"), 4);
    TEST_PASS();
}

static void test_org_move_update_status() {
    TEST_BEGIN("OrgMoveRepository update_status");
    TestDB tdb;
    auto& db = tdb.db;
    OrgMoveRepository repo(db);

    OrgMove m; m.id = "m1"; m.plan_id = "p1"; m.status = MoveStatus::Planned; repo.insert(m);
    repo.update_status("m1", MoveStatus::Completed);

    auto found = repo.find_by_id("m1");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->status, MoveStatus::Completed);
    TEST_PASS();
}

// ============================================================
// UndoRepository tests
// ============================================================

static void test_undo_insert_record_and_find() {
    TEST_BEGIN("UndoRepository insert_record and find_record_by_id");
    TestDB tdb;
    auto& db = tdb.db;
    UndoRepository repo(db);

    UndoRecord rec;
    rec.id = "undo-001";
    rec.operation_id = "plan-001";
    rec.moves_count = 3;
    rec.root_path = "/home/user";
    rec.status = UndoStatus::Available;
    repo.insert_record(rec);

    auto found = repo.find_record_by_id("undo-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->id, "undo-001");
    ASSERT_EQ(found->moves_count, 3);
    ASSERT_EQ(found->status, UndoStatus::Available);
    TEST_PASS();
}

static void test_undo_insert_entries_and_find() {
    TEST_BEGIN("UndoRepository insert_entries and find_entries");
    TestDB tdb;
    auto& db = tdb.db;
    UndoRepository repo(db);

    UndoRecord rec; rec.id = "undo-001"; rec.moves_count = 2;
    rec.status = UndoStatus::Available; repo.insert_record(rec);

    std::vector<UndoEntry> entries;
    UndoEntry e1; e1.undo_id = "undo-001";
    e1.source_path = "/dest/a.txt"; e1.dest_path = "/src/a.txt";
    e1.move_index = 0; entries.push_back(e1);

    UndoEntry e2; e2.undo_id = "undo-001";
    e2.source_path = "/dest/b.txt"; e2.dest_path = "/src/b.txt";
    e2.move_index = 1; entries.push_back(e2);

    repo.insert_entries(entries);

    auto found = repo.find_entries("undo-001");
    ASSERT_EQ(found.size(), 2u);
    ASSERT_EQ(found[0].dest_path, "/src/a.txt");
    ASSERT_EQ(found[1].dest_path, "/src/b.txt");
    TEST_PASS();
}

static void test_undo_find_available() {
    TEST_BEGIN("UndoRepository find_available");
    TestDB tdb;
    auto& db = tdb.db;
    UndoRepository repo(db);

    UndoRecord r1; r1.id = "u1"; r1.status = UndoStatus::Available;
    r1.moves_count = 1; repo.insert_record(r1);

    UndoRecord r2; r2.id = "u2"; r2.status = UndoStatus::Used;
    r2.moves_count = 1; repo.insert_record(r2);

    UndoRecord r3; r3.id = "u3"; r3.status = UndoStatus::Available;
    r3.moves_count = 1; repo.insert_record(r3);

    auto available = repo.find_available();
    ASSERT_EQ(available.size(), 2u);
    TEST_PASS();
}

void run_new_storage_tests() {
    std::cout << "=== New Storage Tests ===" << std::endl;

    test_scan_insert_and_find();
    test_scan_find_all();
    test_scan_update_status();
    test_scan_set_completed();
    test_scan_find_by_id_not_found();

    test_scan_item_insert_and_find();
    test_scan_item_find_by_scan();
    test_scan_item_count_by_scan();
    test_scan_item_clear_scan();
    test_scan_item_insert_batch();

    test_classification_insert_and_find();
    test_classification_find_best();
    test_classification_clear_scan();
    test_classification_insert_batch();

    test_taxonomy_insert_and_find();
    test_taxonomy_find_children();
    test_taxonomy_find_root_nodes();
    test_taxonomy_clear();
    test_taxonomy_increment_count();

    test_rule_insert_and_find();
    test_rule_find_all();
    test_rule_find_enabled();
    test_rule_remove();

    test_org_plan_insert_and_find();
    test_org_plan_find_latest_by_root();
    test_org_plan_find_all();

    test_org_move_insert_and_find();
    test_org_move_find_by_plan();
    test_org_move_find_by_plan_and_status();
    test_org_move_count_by_plan();
    test_org_move_insert_batch();
    test_org_move_update_status();

    test_undo_insert_record_and_find();
    test_undo_insert_entries_and_find();
    test_undo_find_available();
}
