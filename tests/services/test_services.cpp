#include "test_helpers.h"

#include "services/ProjectDetector.h"
#include "services/SearchService.h"
#include "services/DashboardService.h"
#include "services/ActivityService.h"
#include "services/IntegrityService.h"
#include "services/CategoryService.h"
#include "services/TagService.h"
#include "services/UpdateService.h"
#include "services/VersionService.h"
#include "services/NoteService.h"
#include "storage/DatabaseManager.h"
#include "storage/ArchiveItemRepository.h"
#include "storage/CategoryRepository.h"
#include "storage/TagRepository.h"
#include "storage/VersionRepository.h"
#include "storage/NoteRepository.h"
#include "storage/ActivityRepository.h"
#include "filesystem/StorageManager.h"
#include "hashing/FileHasher.h"
#include "core/enums/ItemType.h"
#include "core/enums/ItemStatus.h"
#include "core/utils/Uuid.h"

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

// --- IntegrityService tests ---

static void test_integrity_verify_item_missing() {
    TEST_BEGIN("IntegrityService verify_item with missing file");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    IntegrityService integrity(item_repo);

    ArchiveItem item;
    item.id = "int-missing"; item.name = "Missing"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/nonexistent/file.bin"; item.size = 100;
    item.checksum = "abc123"; item.created_at = ""; item.archived_at = "";
    item.last_modified_at = "";
    item_repo.insert(item);

    auto result = integrity.verify_item("int-missing");
    ASSERT_EQ(result.items.size(), 1u);
    ASSERT_EQ(result.missing_count, 1);
    ASSERT_EQ(result.items[0].state, IntegrityState::Missing);
    TEST_PASS();
}

static void test_integrity_verify_item_no_checksum() {
    TEST_BEGIN("IntegrityService verify_item with no checksum");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    IntegrityService integrity(item_repo);

    ArchiveItem item;
    item.id = "int-nocs"; item.name = "NoCS"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 0;
    item.created_at = ""; item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto result = integrity.verify_item("int-nocs");
    ASSERT_EQ(result.items.size(), 1u);
    ASSERT_EQ(result.corrupted_count, 1);
    ASSERT_EQ(result.items[0].state, IntegrityState::Unknown);
    TEST_PASS();
}

static void test_integrity_verify_all_empty() {
    TEST_BEGIN("IntegrityService verify_all on empty DB");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    IntegrityService integrity(item_repo);

    auto result = integrity.verify_all();
    ASSERT_EQ(result.items.size(), 0u);
    ASSERT_EQ(result.valid_count, 0);
    TEST_PASS();
}

// --- CategoryService tests ---

static void test_category_create() {
    TEST_BEGIN("CategoryService create and get");
    DatabaseManager db(":memory:");
    db.initialize();
    CategoryRepository cat_repo(db);
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    CategoryService svc(cat_repo, item_repo, act_repo);

    auto cat = svc.create("Documents", "#ff0000", "Important docs");
    ASSERT_TRUE(cat.id.size() > 0);
    ASSERT_EQ(cat.name, "Documents");
    ASSERT_EQ(cat.color, "#ff0000");

    auto fetched = svc.get_by_id(cat.id);
    ASSERT_TRUE(fetched.has_value());
    ASSERT_EQ(fetched->name, "Documents");
    TEST_PASS();
}

static void test_category_update() {
    TEST_BEGIN("CategoryService update");
    DatabaseManager db(":memory:");
    db.initialize();
    CategoryRepository cat_repo(db);
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    CategoryService svc(cat_repo, item_repo, act_repo);

    auto cat = svc.create("Old Name", "#000000");
    svc.update(cat.id, "New Name", "#ffffff");

    auto fetched = svc.get_by_id(cat.id);
    ASSERT_TRUE(fetched.has_value());
    ASSERT_EQ(fetched->name, "New Name");
    ASSERT_EQ(fetched->color, "#ffffff");
    TEST_PASS();
}

static void test_category_remove() {
    TEST_BEGIN("CategoryService remove");
    DatabaseManager db(":memory:");
    db.initialize();
    CategoryRepository cat_repo(db);
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    CategoryService svc(cat_repo, item_repo, act_repo);

    auto cat = svc.create("Temp");
    svc.remove(cat.id);
    auto fetched = svc.get_by_id(cat.id);
    ASSERT_TRUE(!fetched.has_value());
    TEST_PASS();
}

static void test_category_get_all() {
    TEST_BEGIN("CategoryService get_all");
    DatabaseManager db(":memory:");
    db.initialize();
    CategoryRepository cat_repo(db);
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    CategoryService svc(cat_repo, item_repo, act_repo);

    svc.create("A");
    svc.create("B");
    svc.create("C");
    auto all = svc.get_all();
    ASSERT_EQ(all.size(), 3u);
    TEST_PASS();
}

// --- UpdateService tests ---

static void test_update_move_to_trash() {
    TEST_BEGIN("UpdateService move_to_trash");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    StorageManager storage(":", "test-items");
    UpdateService svc(item_repo, act_repo, storage);

    ArchiveItem item;
    item.id = "upd-1"; item.name = "TrashMe"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    svc.move_to_trash("upd-1");
    auto fetched = item_repo.find_by_id("upd-1");
    ASSERT_TRUE(fetched.has_value());
    ASSERT_EQ(fetched->status, ItemStatus::Deleted);
    TEST_PASS();
}

static void test_update_restore() {
    TEST_BEGIN("UpdateService restore_from_trash");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    StorageManager storage(":", "test-items");
    UpdateService svc(item_repo, act_repo, storage);

    ArchiveItem item;
    item.id = "upd-2"; item.name = "RestoreMe"; item.type = ItemType::File;
    item.status = ItemStatus::Deleted; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    svc.restore_from_trash("upd-2");
    auto fetched = item_repo.find_by_id("upd-2");
    ASSERT_TRUE(fetched.has_value());
    ASSERT_EQ(fetched->status, ItemStatus::Archived);
    TEST_PASS();
}

static void test_update_toggle_favorite() {
    TEST_BEGIN("UpdateService toggle_favorite");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    StorageManager storage(":", "test-items");
    UpdateService svc(item_repo, act_repo, storage);

    ArchiveItem item;
    item.id = "upd-3"; item.name = "FavMe"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    svc.toggle_favorite("upd-3");
    auto fetched = item_repo.find_by_id("upd-3");
    ASSERT_TRUE(fetched.has_value());
    ASSERT_TRUE(fetched->is_favorite);

    svc.toggle_favorite("upd-3");
    fetched = item_repo.find_by_id("upd-3");
    ASSERT_TRUE(!fetched->is_favorite);
    TEST_PASS();
}

static void test_update_metadata() {
    TEST_BEGIN("UpdateService update_metadata");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    StorageManager storage(":", "test-items");
    UpdateService svc(item_repo, act_repo, storage);

    ArchiveItem item;
    item.id = "upd-4"; item.name = "OldName"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    svc.update_metadata("upd-4", "NewName", "New description");
    auto fetched = item_repo.find_by_id("upd-4");
    ASSERT_TRUE(fetched.has_value());
    ASSERT_EQ(fetched->name, "NewName");
    ASSERT_EQ(fetched->description, "New description");
    TEST_PASS();
}

// --- TagService tests ---

static void test_tag_create() {
    TEST_BEGIN("TagService create and get");
    DatabaseManager db(":memory:");
    db.initialize();
    TagRepository tag_repo(db);
    ArchiveItemRepository item_repo(db);
    TagService svc(tag_repo, item_repo);

    auto tag = svc.create("important", "#ff0000");
    ASSERT_TRUE(tag.id.size() > 0);
    ASSERT_EQ(tag.name, "important");
    ASSERT_EQ(tag.color, "#ff0000");

    auto fetched = svc.get_by_id(tag.id);
    ASSERT_TRUE(fetched.has_value());
    ASSERT_EQ(fetched->name, "important");
    TEST_PASS();
}

static void test_tag_update() {
    TEST_BEGIN("TagService update");
    DatabaseManager db(":memory:");
    db.initialize();
    TagRepository tag_repo(db);
    ArchiveItemRepository item_repo(db);
    TagService svc(tag_repo, item_repo);

    auto tag = svc.create("old");
    svc.update(tag.id, "new", "#00ff00");

    auto fetched = svc.get_by_id(tag.id);
    ASSERT_TRUE(fetched.has_value());
    ASSERT_EQ(fetched->name, "new");
    ASSERT_EQ(fetched->color, "#00ff00");
    TEST_PASS();
}

static void test_tag_remove() {
    TEST_BEGIN("TagService remove");
    DatabaseManager db(":memory:");
    db.initialize();
    TagRepository tag_repo(db);
    ArchiveItemRepository item_repo(db);
    TagService svc(tag_repo, item_repo);

    auto tag = svc.create("temp");
    svc.remove(tag.id);
    ASSERT_TRUE(!svc.get_by_id(tag.id).has_value());
    TEST_PASS();
}

static void test_tag_add_to_item() {
    TEST_BEGIN("TagService add_to_item");
    DatabaseManager db(":memory:");
    db.initialize();
    TagRepository tag_repo(db);
    ArchiveItemRepository item_repo(db);
    TagService svc(tag_repo, item_repo);

    ArchiveItem item;
    item.id = "item-t1"; item.name = "Tagged"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto tag = svc.create("cpp");
    svc.add_to_item("item-t1", tag.id);

    auto tags = svc.get_tags_for_item("item-t1");
    ASSERT_EQ(tags.size(), 1u);
    ASSERT_EQ(tags[0].name, "cpp");
    TEST_PASS();
}

static void test_tag_remove_from_item() {
    TEST_BEGIN("TagService remove_from_item");
    DatabaseManager db(":memory:");
    db.initialize();
    TagRepository tag_repo(db);
    ArchiveItemRepository item_repo(db);
    TagService svc(tag_repo, item_repo);

    ArchiveItem item;
    item.id = "item-t2"; item.name = "Untag"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto tag = svc.create("rust");
    svc.add_to_item("item-t2", tag.id);
    svc.remove_from_item("item-t2", tag.id);

    auto tags = svc.get_tags_for_item("item-t2");
    ASSERT_TRUE(tags.empty());
    TEST_PASS();
}

// --- NoteService tests ---

static void test_note_add() {
    TEST_BEGIN("NoteService add");
    DatabaseManager db(":memory:");
    db.initialize();
    NoteRepository note_repo(db);
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    NoteService svc(note_repo, item_repo, act_repo);

    ArchiveItem item;
    item.id = "item-n1"; item.name = "Noted"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto note = svc.add("item-n1", "Test note content");
    ASSERT_TRUE(note.id.size() > 0);
    ASSERT_EQ(note.content, "Test note content");
    ASSERT_EQ(note.item_id, "item-n1");

    auto notes = svc.get_notes("item-n1");
    ASSERT_EQ(notes.size(), 1u);
    ASSERT_EQ(notes[0].content, "Test note content");
    TEST_PASS();
}

static void test_note_update() {
    TEST_BEGIN("NoteService update");
    DatabaseManager db(":memory:");
    db.initialize();
    NoteRepository note_repo(db);
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    NoteService svc(note_repo, item_repo, act_repo);

    ArchiveItem item;
    item.id = "item-n2"; item.name = "UpdNote"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto note = svc.add("item-n2", "Original");
    svc.update(note.id, "Updated content");

    auto notes = svc.get_notes("item-n2");
    ASSERT_EQ(notes.size(), 1u);
    ASSERT_EQ(notes[0].content, "Updated content");
    TEST_PASS();
}

static void test_note_remove() {
    TEST_BEGIN("NoteService remove");
    DatabaseManager db(":memory:");
    db.initialize();
    NoteRepository note_repo(db);
    ArchiveItemRepository item_repo(db);
    ActivityRepository act_repo(db);
    NoteService svc(note_repo, item_repo, act_repo);

    ArchiveItem item;
    item.id = "item-n3"; item.name = "DelNote"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto note = svc.add("item-n3", "Delete me");
    svc.remove(note.id, "item-n3");

    auto notes = svc.get_notes("item-n3");
    ASSERT_TRUE(notes.empty());
    TEST_PASS();
}

// --- VersionService tests ---

static void test_version_create_and_get() {
    TEST_BEGIN("VersionService create_version and get_versions");
    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    VersionRepository ver_repo(db);
    ActivityRepository act_repo(db);
    StorageManager storage(":", "test-items");
    VersionService svc(ver_repo, item_repo, act_repo, storage);

    ArchiveItem item;
    item.id = "item-v1"; item.name = "Versioned"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 100; item.created_at = "";
    item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto versions = svc.get_versions("item-v1");
    ASSERT_TRUE(versions.empty());

    auto latest = svc.get_latest("item-v1");
    ASSERT_TRUE(!latest.has_value());
    TEST_PASS();
}

// --- Uuid utility tests ---

static void test_uuid_generate() {
    TEST_BEGIN("UUID generate_id produces unique IDs");
    auto id1 = archive::core::utils::generate_id();
    auto id2 = archive::core::utils::generate_id();
    ASSERT_TRUE(id1.size() == 32);
    ASSERT_TRUE(id2.size() == 32);
    ASSERT_TRUE(id1 != id2);
    TEST_PASS();
}

static void test_uuid_now_iso() {
    TEST_BEGIN("UUID now_iso produces valid timestamp");
    auto ts = archive::core::utils::now_iso();
    ASSERT_TRUE(ts.size() == 20);
    ASSERT_TRUE(ts[4] == '-');
    ASSERT_TRUE(ts[7] == '-');
    ASSERT_TRUE(ts[10] == 'T');
    ASSERT_TRUE(ts[19] == 'Z');
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
    test_integrity_verify_item_missing();
    test_integrity_verify_item_no_checksum();
    test_integrity_verify_all_empty();
    test_category_create();
    test_category_update();
    test_category_remove();
    test_category_get_all();
    test_update_move_to_trash();
    test_update_restore();
    test_update_toggle_favorite();
    test_update_metadata();
    test_tag_create();
    test_tag_update();
    test_tag_remove();
    test_tag_add_to_item();
    test_tag_remove_from_item();
    test_note_add();
    test_note_update();
    test_note_remove();
    test_version_create_and_get();
    test_uuid_generate();
    test_uuid_now_iso();
}
