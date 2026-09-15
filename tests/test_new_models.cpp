#include "test_helpers.h"

#include "core/models/Scan.h"
#include "core/models/ScanItem.h"
#include "core/models/Classification.h"
#include "core/models/TaxonomyNode.h"
#include "core/models/OrgPlan.h"
#include "core/models/OrgMove.h"
#include "core/models/ClassificationRule.h"
#include "core/models/UndoRecord.h"
#include "core/models/UndoEntry.h"

using namespace archive::core;

// ============================================================
// Scan tests
// ============================================================

static void test_scan_default_construction() {
    TEST_BEGIN("Scan default construction");
    Scan scan;
    ASSERT_TRUE(scan.id.empty());
    ASSERT_TRUE(scan.root_path.empty());
    ASSERT_EQ(scan.status, ScanStatus::Pending);
    ASSERT_EQ(scan.file_count, 0);
    ASSERT_EQ(scan.folder_count, 0);
    ASSERT_TRUE(scan.started_at.empty());
    ASSERT_TRUE(scan.completed_at.empty());
    TEST_PASS();
}

static void test_scan_construction_with_values() {
    TEST_BEGIN("Scan construction with values");
    Scan scan;
    scan.id = "scan-001";
    scan.root_path = "/home/user/projects";
    scan.status = ScanStatus::Completed;
    scan.file_count = 42;
    scan.folder_count = 7;
    scan.started_at = "2025-09-15T10:00:00Z";
    scan.completed_at = "2025-09-15T10:00:05Z";

    ASSERT_EQ(scan.id, "scan-001");
    ASSERT_EQ(scan.root_path, "/home/user/projects");
    ASSERT_EQ(scan.status, ScanStatus::Completed);
    ASSERT_EQ(scan.file_count, 42);
    ASSERT_EQ(scan.folder_count, 7);
    ASSERT_EQ(scan.started_at, "2025-09-15T10:00:00Z");
    ASSERT_EQ(scan.completed_at, "2025-09-15T10:00:05Z");
    TEST_PASS();
}

static void test_scan_status_can_change() {
    TEST_BEGIN("Scan status can be changed");
    Scan scan;
    ASSERT_EQ(scan.status, ScanStatus::Pending);
    scan.status = ScanStatus::Running;
    ASSERT_EQ(scan.status, ScanStatus::Running);
    scan.status = ScanStatus::Completed;
    ASSERT_EQ(scan.status, ScanStatus::Completed);
    TEST_PASS();
}

// ============================================================
// ScanItem tests
// ============================================================

static void test_scan_item_default_construction() {
    TEST_BEGIN("ScanItem default construction");
    ScanItem item;
    ASSERT_TRUE(item.id.empty());
    ASSERT_TRUE(item.scan_id.empty());
    ASSERT_TRUE(item.path.empty());
    ASSERT_TRUE(item.filename.empty());
    ASSERT_TRUE(item.extension.empty());
    ASSERT_TRUE(item.mime_type.empty());
    ASSERT_EQ(item.size, 0);
    ASSERT_TRUE(item.role.empty());
    ASSERT_TRUE(item.detected_project.empty());
    ASSERT_TRUE(item.content_preview.empty());
    ASSERT_TRUE(item.checksum.empty());
    ASSERT_TRUE(item.created_at.empty());
    ASSERT_TRUE(item.modified_at.empty());
    TEST_PASS();
}

static void test_scan_item_with_values() {
    TEST_BEGIN("ScanItem with values");
    ScanItem item;
    item.id = "item-001";
    item.scan_id = "scan-001";
    item.path = "/home/user/projects/main.cpp";
    item.filename = "main.cpp";
    item.extension = ".cpp";
    item.mime_type = "text/x-c++src";
    item.size = 2048;
    item.role = "Source";
    item.detected_project = "MyProject";
    item.content_preview = "#include <iostream>";
    item.checksum = "abc123";
    item.created_at = "2025-09-15T10:00:00Z";
    item.modified_at = "2025-09-15T10:00:00Z";

    ASSERT_EQ(item.id, "item-001");
    ASSERT_EQ(item.scan_id, "scan-001");
    ASSERT_EQ(item.path, "/home/user/projects/main.cpp");
    ASSERT_EQ(item.filename, "main.cpp");
    ASSERT_EQ(item.extension, ".cpp");
    ASSERT_EQ(item.mime_type, "text/x-c++src");
    ASSERT_EQ(item.size, 2048);
    ASSERT_EQ(item.role, "Source");
    ASSERT_EQ(item.detected_project, "MyProject");
    ASSERT_EQ(item.content_preview, "#include <iostream>");
    ASSERT_EQ(item.checksum, "abc123");
    TEST_PASS();
}

// ============================================================
// Classification tests
// ============================================================

static void test_classification_default_construction() {
    TEST_BEGIN("Classification default construction");
    Classification cls;
    ASSERT_TRUE(cls.id.empty());
    ASSERT_TRUE(cls.scan_item_id.empty());
    ASSERT_TRUE(cls.taxonomy_path.empty());
    ASSERT_EQ(cls.confidence, 0.0);
    ASSERT_TRUE(cls.reason.empty());
    TEST_PASS();
}

static void test_classification_with_values() {
    TEST_BEGIN("Classification with values");
    Classification cls;
    cls.id = "cls-001";
    cls.scan_item_id = "item-001";
    cls.taxonomy_path = "Development/C++";
    cls.confidence = 0.95;
    cls.reason = "Extension .cpp detected";

    ASSERT_EQ(cls.id, "cls-001");
    ASSERT_EQ(cls.scan_item_id, "item-001");
    ASSERT_EQ(cls.taxonomy_path, "Development/C++");
    ASSERT_TRUE(cls.confidence > 0.9);
    ASSERT_EQ(cls.reason, "Extension .cpp detected");
    TEST_PASS();
}

static void test_classification_group_default() {
    TEST_BEGIN("ClassificationGroup default construction");
    ClassificationGroup grp;
    ASSERT_TRUE(grp.id.empty());
    ASSERT_TRUE(grp.classification_id.empty());
    ASSERT_TRUE(grp.group_member_id.empty());
    TEST_PASS();
}

static void test_classification_group_with_values() {
    TEST_BEGIN("ClassificationGroup with values");
    ClassificationGroup grp;
    grp.id = "grp-001";
    grp.classification_id = "cls-001";
    grp.group_member_id = "cls-002";

    ASSERT_EQ(grp.id, "grp-001");
    ASSERT_EQ(grp.classification_id, "cls-001");
    ASSERT_EQ(grp.group_member_id, "cls-002");
    TEST_PASS();
}

// ============================================================
// TaxonomyNode tests
// ============================================================

static void test_taxonomy_node_default_construction() {
    TEST_BEGIN("TaxonomyNode default construction");
    TaxonomyNode node;
    ASSERT_TRUE(node.id.empty());
    ASSERT_TRUE(node.name.empty());
    ASSERT_TRUE(node.parent_id.empty());
    ASSERT_EQ(node.level, 0);
    ASSERT_EQ(node.file_count, 0);
    ASSERT_TRUE(node.icon.empty());
    TEST_PASS();
}

static void test_taxonomy_node_with_values() {
    TEST_BEGIN("TaxonomyNode with values");
    TaxonomyNode node;
    node.id = "tax-001";
    node.name = "Development";
    node.parent_id = "";
    node.level = 0;
    node.file_count = 150;
    node.icon = "code";

    ASSERT_EQ(node.id, "tax-001");
    ASSERT_EQ(node.name, "Development");
    ASSERT_TRUE(node.parent_id.empty());
    ASSERT_EQ(node.level, 0);
    ASSERT_EQ(node.file_count, 150);
    ASSERT_EQ(node.icon, "code");
    TEST_PASS();
}

static void test_taxonomy_node_child() {
    TEST_BEGIN("TaxonomyNode child node");
    TaxonomyNode parent;
    parent.id = "tax-parent";
    parent.name = "Development";

    TaxonomyNode child;
    child.id = "tax-child";
    child.name = "C++";
    child.parent_id = "tax-parent";
    child.level = 1;

    ASSERT_EQ(child.parent_id, parent.id);
    ASSERT_EQ(child.level, 1);
    TEST_PASS();
}

// ============================================================
// OrgPlan tests
// ============================================================

static void test_org_plan_default_construction() {
    TEST_BEGIN("OrgPlan default construction");
    OrgPlan plan;
    ASSERT_TRUE(plan.id.empty());
    ASSERT_TRUE(plan.scan_id.empty());
    ASSERT_TRUE(plan.root_path.empty());
    ASSERT_EQ(plan.status, PlanStatus::Draft);
    ASSERT_EQ(plan.total_files, 0);
    ASSERT_EQ(plan.moves_planned, 0);
    ASSERT_EQ(plan.unchanged, 0);
    ASSERT_EQ(plan.avg_confidence, 0.0);
    ASSERT_EQ(plan.intensity, 50);
    ASSERT_TRUE(plan.created_at.empty());
    ASSERT_TRUE(plan.executed_at.empty());
    TEST_PASS();
}

static void test_org_plan_with_values() {
    TEST_BEGIN("OrgPlan with values");
    OrgPlan plan;
    plan.id = "plan-001";
    plan.scan_id = "scan-001";
    plan.root_path = "/home/user/projects";
    plan.status = PlanStatus::Ready;
    plan.total_files = 42;
    plan.moves_planned = 35;
    plan.unchanged = 7;
    plan.avg_confidence = 0.85;
    plan.intensity = 75;
    plan.created_at = "2025-09-15T10:00:00Z";
    plan.executed_at = "";

    ASSERT_EQ(plan.id, "plan-001");
    ASSERT_EQ(plan.scan_id, "scan-001");
    ASSERT_EQ(plan.root_path, "/home/user/projects");
    ASSERT_EQ(plan.status, PlanStatus::Ready);
    ASSERT_EQ(plan.total_files, 42);
    ASSERT_EQ(plan.moves_planned, 35);
    ASSERT_EQ(plan.unchanged, 7);
    ASSERT_TRUE(plan.avg_confidence > 0.8);
    ASSERT_EQ(plan.intensity, 75);
    TEST_PASS();
}

// ============================================================
// OrgMove tests
// ============================================================

static void test_org_move_default_construction() {
    TEST_BEGIN("OrgMove default construction");
    OrgMove move;
    ASSERT_TRUE(move.id.empty());
    ASSERT_TRUE(move.plan_id.empty());
    ASSERT_TRUE(move.scan_item_id.empty());
    ASSERT_TRUE(move.source_path.empty());
    ASSERT_TRUE(move.dest_path.empty());
    ASSERT_EQ(move.confidence, 0.0);
    ASSERT_TRUE(move.reason.empty());
    ASSERT_EQ(move.status, MoveStatus::Planned);
    TEST_PASS();
}

static void test_org_move_with_values() {
    TEST_BEGIN("OrgMove with values");
    OrgMove move;
    move.id = "move-001";
    move.plan_id = "plan-001";
    move.scan_item_id = "item-001";
    move.source_path = "/home/user/main.cpp";
    move.dest_path = "/home/user/Development/C++/main.cpp";
    move.confidence = 0.92;
    move.reason = "Extension .cpp classified to Development/C++";
    move.status = MoveStatus::Planned;

    ASSERT_EQ(move.id, "move-001");
    ASSERT_EQ(move.plan_id, "plan-001");
    ASSERT_EQ(move.scan_item_id, "item-001");
    ASSERT_EQ(move.source_path, "/home/user/main.cpp");
    ASSERT_EQ(move.dest_path, "/home/user/Development/C++/main.cpp");
    ASSERT_TRUE(move.confidence > 0.9);
    ASSERT_EQ(move.reason, "Extension .cpp classified to Development/C++");
    ASSERT_EQ(move.status, MoveStatus::Planned);
    TEST_PASS();
}

// ============================================================
// ClassificationRule tests
// ============================================================

static void test_classification_rule_default_construction() {
    TEST_BEGIN("ClassificationRule default construction");
    ClassificationRule rule;
    ASSERT_TRUE(rule.id.empty());
    ASSERT_TRUE(rule.name.empty());
    ASSERT_TRUE(rule.pattern.empty());
    ASSERT_TRUE(rule.target_path.empty());
    ASSERT_EQ(rule.priority, 0);
    ASSERT_TRUE(rule.enabled);
    ASSERT_TRUE(rule.created_at.empty());
    TEST_PASS();
}

static void test_classification_rule_with_values() {
    TEST_BEGIN("ClassificationRule with values");
    ClassificationRule rule;
    rule.id = "rule-001";
    rule.name = "C++ sources";
    rule.pattern = "*.cpp";
    rule.target_path = "Development/C++";
    rule.priority = 10;
    rule.enabled = true;
    rule.created_at = "2025-09-15T10:00:00Z";

    ASSERT_EQ(rule.id, "rule-001");
    ASSERT_EQ(rule.name, "C++ sources");
    ASSERT_EQ(rule.pattern, "*.cpp");
    ASSERT_EQ(rule.target_path, "Development/C++");
    ASSERT_EQ(rule.priority, 10);
    ASSERT_TRUE(rule.enabled);
    TEST_PASS();
}

static void test_classification_rule_disabled() {
    TEST_BEGIN("ClassificationRule disabled");
    ClassificationRule rule;
    rule.id = "rule-002";
    rule.enabled = false;
    ASSERT_TRUE(!rule.enabled);
    TEST_PASS();
}

// ============================================================
// UndoRecord tests
// ============================================================

static void test_undo_record_default_construction() {
    TEST_BEGIN("UndoRecord default construction");
    UndoRecord rec;
    ASSERT_TRUE(rec.id.empty());
    ASSERT_TRUE(rec.operation_id.empty());
    ASSERT_EQ(rec.moves_count, 0);
    ASSERT_TRUE(rec.root_path.empty());
    ASSERT_EQ(rec.status, UndoStatus::Available);
    ASSERT_TRUE(rec.created_at.empty());
    TEST_PASS();
}

static void test_undo_record_with_values() {
    TEST_BEGIN("UndoRecord with values");
    UndoRecord rec;
    rec.id = "undo-001";
    rec.operation_id = "plan-001";
    rec.moves_count = 5;
    rec.root_path = "/home/user/projects";
    rec.status = UndoStatus::Available;
    rec.created_at = "2025-09-15T10:05:00Z";

    ASSERT_EQ(rec.id, "undo-001");
    ASSERT_EQ(rec.operation_id, "plan-001");
    ASSERT_EQ(rec.moves_count, 5);
    ASSERT_EQ(rec.root_path, "/home/user/projects");
    ASSERT_EQ(rec.status, UndoStatus::Available);
    ASSERT_EQ(rec.created_at, "2025-09-15T10:05:00Z");
    TEST_PASS();
}

// ============================================================
// UndoEntry tests
// ============================================================

static void test_undo_entry_default_construction() {
    TEST_BEGIN("UndoEntry default construction");
    UndoEntry entry;
    ASSERT_TRUE(entry.undo_id.empty());
    ASSERT_TRUE(entry.source_path.empty());
    ASSERT_TRUE(entry.dest_path.empty());
    ASSERT_EQ(entry.move_index, 0);
    TEST_PASS();
}

static void test_undo_entry_with_values() {
    TEST_BEGIN("UndoEntry with values");
    UndoEntry entry;
    entry.undo_id = "undo-001";
    entry.source_path = "/dest/Development/C++/main.cpp";
    entry.dest_path = "/home/user/main.cpp";
    entry.move_index = 0;

    ASSERT_EQ(entry.undo_id, "undo-001");
    ASSERT_EQ(entry.source_path, "/dest/Development/C++/main.cpp");
    ASSERT_EQ(entry.dest_path, "/home/user/main.cpp");
    ASSERT_EQ(entry.move_index, 0);
    TEST_PASS();
}

void run_new_model_tests() {
    std::cout << "=== New Model Tests ===" << std::endl;

    test_scan_default_construction();
    test_scan_construction_with_values();
    test_scan_status_can_change();

    test_scan_item_default_construction();
    test_scan_item_with_values();

    test_classification_default_construction();
    test_classification_with_values();
    test_classification_group_default();
    test_classification_group_with_values();

    test_taxonomy_node_default_construction();
    test_taxonomy_node_with_values();
    test_taxonomy_node_child();

    test_org_plan_default_construction();
    test_org_plan_with_values();

    test_org_move_default_construction();
    test_org_move_with_values();

    test_classification_rule_default_construction();
    test_classification_rule_with_values();
    test_classification_rule_disabled();

    test_undo_record_default_construction();
    test_undo_record_with_values();

    test_undo_entry_default_construction();
    test_undo_entry_with_values();
}
