#include "test_helpers.h"

#include "services/ProjectDetector.h"
#include "services/SearchService.h"
#include "services/DashboardService.h"
#include "services/ActivityService.h"
#include "storage/DatabaseManager.h"
#include "storage/ArchiveItemRepository.h"
#include "storage/CategoryRepository.h"
#include "storage/TagRepository.h"
#include "storage/ActivityRepository.h"
#include "filesystem/StorageManager.h"
#include "core/enums/ItemType.h"
#include "core/enums/ItemStatus.h"

using namespace archive::services;
using namespace archive::storage;
using namespace archive::filesystem;
using namespace archive::core;

// --- ProjectDetector tests ---

static void test_detector_default_patterns() {
    TEST_BEGIN("ProjectDetector has default patterns");
    ProjectDetector detector;
    auto patterns = detector.get_all_patterns();
    ASSERT_TRUE(patterns.size() >= 10);
    TEST_PASS();
}

static void test_detector_no_match() {
    TEST_BEGIN("ProjectDetector detects nothing on empty dir");
    ProjectDetector detector;
    auto result = detector.detect("nonexistent_path_xyz");
    ASSERT_TRUE(!result.is_project);
    TEST_PASS();
}

static void test_detector_register_custom() {
    TEST_BEGIN("ProjectDetector custom pattern registration");
    ProjectDetector detector;
    size_t before = detector.get_all_patterns().size();
    detector.register_pattern(ProjectPattern("Custom", "custom", {"custom.toml"}));
    ASSERT_EQ(detector.get_all_patterns().size(), before + 1);
    TEST_PASS();
}

// --- SearchService tests ---

static void test_search_empty_query() {
    TEST_BEGIN("SearchService empty query returns all");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    SearchService search(item_repo);

    ArchiveItem item;
    item.id = "s1"; item.name = "Alpha"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto result = search.search("");
    ASSERT_EQ(result.total, 1);
    ASSERT_EQ(result.items[0].name, "Alpha");
    TEST_PASS();
}

static void test_search_with_query() {
    TEST_BEGIN("SearchService filters by query");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    SearchService search(item_repo);

    ArchiveItem item1;
    item1.id = "s2"; item1.name = "Hello World"; item1.type = ItemType::File;
    item1.status = ItemStatus::Archived; item1.original_path = "/a";
    item1.storage_path = "/b"; item1.size = 10; item1.created_at = "";
    item1.archived_at = ""; item1.last_modified_at = "";
    item_repo.insert(item1);

    ArchiveItem item2;
    item2.id = "s3"; item2.name = "Goodbye"; item2.type = ItemType::File;
    item2.status = ItemStatus::Archived; item2.original_path = "/c";
    item2.storage_path = "/d"; item2.size = 10; item2.created_at = "";
    item2.archived_at = ""; item2.last_modified_at = "";
    item_repo.insert(item2);

    auto result = search.search("Hello");
    ASSERT_EQ(result.total, 1);
    ASSERT_EQ(result.items[0].name, "Hello World");
    TEST_PASS();
}

static void test_search_filter_favorite() {
    TEST_BEGIN("SearchService filter by favorite");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    SearchService search(item_repo);

    ArchiveItem item1;
    item1.id = "f1"; item1.name = "Fav"; item1.type = ItemType::File;
    item1.status = ItemStatus::Archived; item1.original_path = "/a";
    item1.storage_path = "/b"; item1.size = 10; item1.created_at = "";
    item1.archived_at = ""; item1.last_modified_at = "";
    item1.is_favorite = true;
    item_repo.insert(item1);

    ArchiveItem item2;
    item2.id = "f2"; item2.name = "NotFav"; item2.type = ItemType::File;
    item2.status = ItemStatus::Archived; item2.original_path = "/c";
    item2.storage_path = "/d"; item2.size = 10; item2.created_at = "";
    item2.archived_at = ""; item2.last_modified_at = "";
    item_repo.insert(item2);

    SearchFilters filters;
    filters.is_favorite = true;
    auto result = search.search("", filters);
    ASSERT_EQ(result.total, 1);
    ASSERT_EQ(result.items[0].name, "Fav");
    TEST_PASS();
}

// --- DashboardService tests ---

static void test_dashboard_stats() {
    TEST_BEGIN("DashboardService returns stats");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    DashboardService dash(item_repo);

    ArchiveItem item;
    item.id = "d1"; item.name = "Test"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 500; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto stats = dash.get_stats();
    ASSERT_EQ(stats.total_items, 1);
    ASSERT_EQ(stats.archived_items, 1);
    ASSERT_EQ(stats.total_size, 500u);
    TEST_PASS();
}

// --- ActivityService tests ---

static void test_activity_log() {
    TEST_BEGIN("ActivityService logs and retrieves");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    ActivityService act_svc(act_repo);

    ArchiveItem item;
    item.id = "item-act"; item.name = "Test"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    act_svc.log("item-act", ActivityAction::Imported, "from /path");
    auto recent = act_svc.get_recent(10);
    ASSERT_EQ(recent.size(), 1);
    ASSERT_EQ(recent[0].action, ActivityAction::Imported);
    ASSERT_EQ(recent[0].details, "from /path");
    TEST_PASS();
}

void run_service_tests() {
    std::cout << "=== Service Tests ===" << std::endl;

    test_detector_default_patterns();
    test_detector_no_match();
    test_detector_register_custom();
    test_search_empty_query();
    test_search_with_query();
    test_search_filter_favorite();
    test_dashboard_stats();
    test_activity_log();
}
