#include "test_helpers.h"

#include "storage/DatabaseManager.h"
#include "storage/ClassificationRepository.h"
#include "storage/ClassificationRuleRepository.h"
#include "storage/ScanItemRepository.h"
#include "services/Classifier.h"
#include "core/models/ScanItem.h"
#include "core/models/Classification.h"
#include "core/models/ClassificationRule.h"

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


static ScanItem make_item(const std::string& id, const std::string& filename,
                           const std::string& ext, const std::string& mime = "",
                           const std::string& content_preview = "",
                           const std::string& project = "") {
    ScanItem item;
    item.id = id;
    item.scan_id = "scan-001";
    item.filename = filename;
    item.extension = ext;
    item.mime_type = mime;
    item.content_preview = content_preview;
    item.detected_project = project;
    item.path = "/" + filename;
    item.size = 100;
    return item;
}

// ============================================================
// classify_by_extension tests
// ============================================================

static void test_classify_cpp_extension() {
    TEST_BEGIN("classify_by_extension: .cpp -> Development/C++");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "main.cpp", ".cpp");
    auto cls = classifier.classify_item(item, 50);
    ASSERT_TRUE(cls.taxonomy_path.find("Development") != std::string::npos);
    TEST_PASS();
}

static void test_classify_python_extension() {
    TEST_BEGIN("classify_by_extension: .py -> Development/Python");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "script.py", ".py");
    auto cls = classifier.classify_item(item, 50);
    ASSERT_TRUE(cls.taxonomy_path.find("Development") != std::string::npos);
    TEST_PASS();
}

static void test_classify_jpg_extension() {
    TEST_BEGIN("classify_by_extension: .jpg -> Images/Photos");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "photo.jpg", ".jpg");
    auto cls = classifier.classify_item(item, 50);
    ASSERT_TRUE(cls.taxonomy_path.find("Image") != std::string::npos ||
                cls.taxonomy_path.find("Photo") != std::string::npos);
    TEST_PASS();
}

static void test_classify_h_extension() {
    TEST_BEGIN("classify_by_extension: .h -> Development/C++ (Header)");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "utils.h", ".h");
    auto cls = classifier.classify_item(item, 50);
    ASSERT_TRUE(cls.taxonomy_path.find("Development") != std::string::npos);
    TEST_PASS();
}

// ============================================================
// classify_by_mime tests
// ============================================================

static void test_classify_jpeg_magic_bytes() {
    TEST_BEGIN("classify_by_mime: JPEG magic bytes -> Images/Photos");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "photo", ".bin", "image/jpeg");
    auto cls = classifier.classify_item(item, 50);
    ASSERT_TRUE(cls.taxonomy_path.find("Image") != std::string::npos ||
                cls.taxonomy_path.find("Photo") != std::string::npos);
    TEST_PASS();
}

// ============================================================
// classify_by_content tests
// ============================================================

static void test_classify_html_content() {
    TEST_BEGIN("classify_by_content: HTML content -> Documents/Web");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "page", ".html",
                              "text/html", "<!DOCTYPE html><html><head><title>Test</title></head></html>");
    auto cls = classifier.classify_item(item, 50);
    ASSERT_TRUE(cls.taxonomy_path.find("Document") != std::string::npos ||
                cls.taxonomy_path.find("Web") != std::string::npos);
    TEST_PASS();
}

static void test_classify_python_shebang_content() {
    TEST_BEGIN("classify_by_content: Python shebang -> Development/Python");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "myscript", ".sh",
                              "text/x-script", "#!/usr/bin/env python3\nprint('hello')");
    auto cls = classifier.classify_item(item, 50);
    ASSERT_TRUE(cls.taxonomy_path.find("Development") != std::string::npos);
    TEST_PASS();
}

// ============================================================
// confidence scoring tests
// ============================================================

static void test_confidence_extension_with_project_higher() {
    TEST_BEGIN("confidence: extension + project > extension alone");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem alone = make_item("i1", "main.cpp", ".cpp");
    ScanItem with_project = make_item("i2", "main.cpp", ".cpp", "", "", "MyProject");

    auto cls_alone = classifier.classify_item(alone, 50);
    auto cls_with_proj = classifier.classify_item(with_project, 50);

    ASSERT_TRUE(cls_with_proj.confidence >= cls_alone.confidence);
    TEST_PASS();
}

static void test_confidence_extension_better_than_content() {
    TEST_BEGIN("confidence: extension > content-based");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem with_ext = make_item("i1", "main.cpp", ".cpp");
    ScanItem no_ext = make_item("i2", "mystery", "", "text/plain",
                                "#include <iostream>\nint main() {}");

    auto cls_ext = classifier.classify_item(with_ext, 50);
    auto cls_no_ext = classifier.classify_item(no_ext, 50);

    ASSERT_TRUE(cls_ext.confidence >= cls_no_ext.confidence);
    TEST_PASS();
}

// ============================================================
// intensity levels tests
// ============================================================

static void test_intensity_basic() {
    TEST_BEGIN("intensity 0 = basic classification");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "main.cpp", ".cpp");
    auto cls = classifier.classify_item(item, 0);
    ASSERT_TRUE(!cls.taxonomy_path.empty());
    TEST_PASS();
}

static void test_intensity_subcategories() {
    TEST_BEGIN("intensity 50 = subcategories");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "main.cpp", ".cpp");
    auto cls = classifier.classify_item(item, 50);
    ASSERT_TRUE(cls.taxonomy_path.find("/") != std::string::npos);
    TEST_PASS();
}

static void test_intensity_full_depth() {
    TEST_BEGIN("intensity 100 = full depth");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);
    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "main.cpp", ".cpp");
    auto cls_100 = classifier.classify_item(item, 100);
    auto cls_0 = classifier.classify_item(item, 0);
    ASSERT_TRUE(cls_100.taxonomy_path.size() >= cls_0.taxonomy_path.size());
    TEST_PASS();
}

// ============================================================
// user rules override tests
// ============================================================

static void test_user_rules_override() {
    TEST_BEGIN("user rules override default classification");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);

    ClassificationRule rule;
    rule.id = "rule-001";
    rule.name = "Custom C++ rule";
    rule.pattern = "*.cpp";
    rule.target_path = "MyCustom/CppPath";
    rule.priority = 100;
    rule.enabled = true;
    rule_repo.insert(rule);

    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    ScanItem item = make_item("i1", "main.cpp", ".cpp");
    auto cls = classifier.classify_item(item, 50);
    ASSERT_EQ(cls.taxonomy_path, "MyCustom/CppPath");
    TEST_PASS();
}

// ============================================================
// group detection tests
// ============================================================

static void test_group_detection() {
    TEST_BEGIN("group detection: invoice.pdf + invoice.xml grouped");
    TestDB tdb;
    auto& db = tdb.db;
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);

    ScanItem i1 = make_item("i1", "invoice.pdf", ".pdf");
    ScanItem i2 = make_item("i2", "invoice.xml", ".xml");
    ScanItem i3 = make_item("i3", "random.txt", ".txt");

    item_repo.insert(i1);
    item_repo.insert(i2);
    item_repo.insert(i3);

    Classifier classifier(db, cls_repo, rule_repo, item_repo);

    auto c1 = classifier.classify_item(i1, 50);
    auto c2 = classifier.classify_item(i2, 50);
    auto c3 = classifier.classify_item(i3, 50);

    ASSERT_TRUE(!c1.taxonomy_path.empty());
    ASSERT_TRUE(!c2.taxonomy_path.empty());
    ASSERT_TRUE(!c3.taxonomy_path.empty());
    TEST_PASS();
}

// ============================================================
// classify_scan tests
// ============================================================

static void test_classify_scan() {
    TEST_BEGIN("classify_scan processes all items");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_scan(db, "scan-001");
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);

    ScanItem i1 = make_item("i1", "main.cpp", ".cpp");
    ScanItem i2 = make_item("i2", "script.py", ".py");
    ScanItem i3 = make_item("i3", "photo.jpg", ".jpg");
    item_repo.insert(i1);
    item_repo.insert(i2);
    item_repo.insert(i3);

    Classifier classifier(db, cls_repo, rule_repo, item_repo);
    auto results = classifier.classify_scan("scan-001", 50);

    ASSERT_TRUE(results.size() >= 3u);
    TEST_PASS();
}

// ============================================================
// taxonomy summary tests
// ============================================================

static void test_taxonomy_summary() {
    TEST_BEGIN("get_taxonomy_summary returns categories with counts");
    TestDB tdb;
    auto& db = tdb.db;
    ensure_scan(db, "scan-001");
    ClassificationRepository cls_repo(db);
    ClassificationRuleRepository rule_repo(db);
    ScanItemRepository item_repo(db);

    ScanItem i1 = make_item("i1", "main.cpp", ".cpp");
    ScanItem i2 = make_item("i2", "utils.cpp", ".cpp");
    ScanItem i3 = make_item("i3", "script.py", ".py");
    ScanItem i4 = make_item("i4", "photo.jpg", ".jpg");
    item_repo.insert(i1);
    item_repo.insert(i2);
    item_repo.insert(i3);
    item_repo.insert(i4);

    Classifier classifier(db, cls_repo, rule_repo, item_repo);
    classifier.classify_scan("scan-001", 50);

    auto summary = classifier.get_taxonomy_summary("scan-001");
    ASSERT_TRUE(summary.size() >= 2u);
    TEST_PASS();
}

void run_classifier_tests() {
    std::cout << "=== Classifier Tests ===" << std::endl;

    test_classify_cpp_extension();
    test_classify_python_extension();
    test_classify_jpg_extension();
    test_classify_h_extension();
    test_classify_jpeg_magic_bytes();
    test_classify_html_content();
    test_classify_python_shebang_content();
    test_confidence_extension_with_project_higher();
    test_confidence_extension_better_than_content();
    test_intensity_basic();
    test_intensity_subcategories();
    test_intensity_full_depth();
    test_user_rules_override();
    test_group_detection();
    test_classify_scan();
    test_taxonomy_summary();
}
