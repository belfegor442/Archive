#include "test_helpers.h"

#include "storage/DatabaseManager.h"
#include "storage/ArchiveItemRepository.h"
#include "storage/CategoryRepository.h"
#include "storage/TagRepository.h"
#include "storage/VersionRepository.h"
#include "storage/NoteRepository.h"
#include "storage/ActivityRepository.h"
#include "core/enums/ItemType.h"
#include "core/enums/ItemStatus.h"

#include <cstdio>

using namespace archive::storage;
using namespace archive::core;

static const std::string TEST_DB = ":memory:";

static void cleanup() {
}

// --- DatabaseManager tests ---

static void test_db_open() {
    TEST_BEGIN("DatabaseManager opens successfully");
    cleanup();
    DatabaseManager db(TEST_DB);
    ASSERT_NO_THROW(db.initialize());
    db.close();
    TEST_PASS();
}

static void test_db_schema_created() {
    TEST_BEGIN("DatabaseManager creates schema");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();

    auto stmt = db.prepare("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
    int table_count = 0;
    while (stmt.step()) table_count++;
    ASSERT_TRUE(table_count >= 7);
    db.close();
    TEST_PASS();
}

static void test_db_transaction() {
    TEST_BEGIN("DatabaseManager transaction commit/rollback");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();

    db.execute("CREATE TABLE test_tbl (id INTEGER PRIMARY KEY, val TEXT)");
    db.begin_transaction();
    db.execute("INSERT INTO test_tbl (id, val) VALUES (1, 'hello')");
    db.commit();

    auto stmt = db.prepare("SELECT COUNT(*) FROM test_tbl");
    ASSERT_TRUE(stmt.step());
    ASSERT_EQ(stmt.column_int(0), 1);

    db.begin_transaction();
    db.execute("INSERT INTO test_tbl (id, val) VALUES (2, 'world')");
    db.rollback();

    auto stmt2 = db.prepare("SELECT COUNT(*) FROM test_tbl");
    ASSERT_TRUE(stmt2.step());
    ASSERT_EQ(stmt2.column_int(0), 1);

    db.close();
    TEST_PASS();
}

// --- ArchiveItemRepository tests ---

static void test_item_insert_and_find() {
    TEST_BEGIN("ArchiveItemRepository insert and find_by_id");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();
    ArchiveItemRepository repo(db);

    ArchiveItem item;
    item.id = "item-001";
    item.name = "My Project";
    item.type = ItemType::Project;
    item.status = ItemStatus::Archived;
    item.original_path = "/home/user/projects/my-project";
    item.storage_path = "/data/items/item-001/files";
    item.size = 1024 * 100;
    item.file_count = 15;
    item.description = "A test project";
    item.created_at = "2025-01-01T00:00:00Z";
    item.archived_at = "2025-01-01T00:00:00Z";
    item.last_modified_at = "2025-01-01T00:00:00Z";
    item.checksum = "abc123";
    item.current_version = 1;

    repo.insert(item);

    auto found = repo.find_by_id("item-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->id, "item-001");
    ASSERT_EQ(found->name, "My Project");
    ASSERT_EQ(found->type, ItemType::Project);
    ASSERT_EQ(found->status, ItemStatus::Archived);
    ASSERT_EQ(found->size, 102400u);
    ASSERT_EQ(found->file_count, 15);
    ASSERT_TRUE(found->is_favorite == false);

    db.close();
    TEST_PASS();
}

static void test_item_update() {
    TEST_BEGIN("ArchiveItemRepository update");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();
    ArchiveItemRepository repo(db);

    ArchiveItem item;
    item.id = "item-002";
    item.name = "Original Name";
    item.type = ItemType::File;
    item.status = ItemStatus::Archived;
    item.original_path = "/tmp/test.txt";
    item.storage_path = "/data/items/item-002/files/test.txt";
    item.size = 100;
    item.created_at = "2025-01-01T00:00:00Z";
    item.archived_at = "2025-01-01T00:00:00Z";
    item.last_modified_at = "2025-01-01T00:00:00Z";
    repo.insert(item);

    item.name = "Updated Name";
    item.size = 200;
    repo.update(item);

    auto found = repo.find_by_id("item-002");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->name, "Updated Name");
    ASSERT_EQ(found->size, 200u);

    db.close();
    TEST_PASS();
}

static void test_item_remove() {
    TEST_BEGIN("ArchiveItemRepository remove");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();
    ArchiveItemRepository repo(db);

    ArchiveItem item;
    item.id = "item-003";
    item.name = "To Delete";
    item.type = ItemType::File;
    item.status = ItemStatus::Archived;
    item.original_path = "/tmp/del.txt";
    item.storage_path = "/data/items/item-003/files/del.txt";
    item.size = 10;
    item.created_at = "2025-01-01T00:00:00Z";
    item.archived_at = "2025-01-01T00:00:00Z";
    item.last_modified_at = "2025-01-01T00:00:00Z";
    repo.insert(item);

    repo.remove("item-003");
    auto found = repo.find_by_id("item-003");
    ASSERT_TRUE(!found.has_value());

    db.close();
    TEST_PASS();
}

static void test_item_favorite() {
    TEST_BEGIN("ArchiveItemRepository set_favorite");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();
    ArchiveItemRepository repo(db);

    ArchiveItem item;
    item.id = "item-004";
    item.name = "Favorite Item";
    item.type = ItemType::File;
    item.status = ItemStatus::Archived;
    item.original_path = "/tmp/fav.txt";
    item.storage_path = "/data/items/item-004/files/fav.txt";
    item.size = 10;
    item.created_at = "2025-01-01T00:00:00Z";
    item.archived_at = "2025-01-01T00:00:00Z";
    item.last_modified_at = "2025-01-01T00:00:00Z";
    repo.insert(item);

    repo.set_favorite("item-004", true);
    auto found = repo.find_by_id("item-004");
    ASSERT_TRUE(found->is_favorite);

    repo.set_favorite("item-004", false);
    found = repo.find_by_id("item-004");
    ASSERT_TRUE(!found->is_favorite);

    db.close();
    TEST_PASS();
}

static void test_item_search() {
    TEST_BEGIN("ArchiveItemRepository search");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();
    ArchiveItemRepository repo(db);

    ArchiveItem item1;
    item1.id = "item-010";
    item1.name = "C++ Project";
    item1.type = ItemType::Project;
    item1.status = ItemStatus::Archived;
    item1.original_path = "/projects/cpp";
    item1.storage_path = "/data/items/item-010/files";
    item1.size = 1000;
    item1.created_at = "2025-01-01T00:00:00Z";
    item1.archived_at = "2025-01-01T00:00:00Z";
    item1.last_modified_at = "2025-01-01T00:00:00Z";
    repo.insert(item1);

    ArchiveItem item2;
    item2.id = "item-011";
    item2.name = "Python Script";
    item2.type = ItemType::File;
    item2.status = ItemStatus::Archived;
    item2.original_path = "/scripts/python.py";
    item2.storage_path = "/data/items/item-011/files/python.py";
    item2.size = 500;
    item2.created_at = "2025-01-01T00:00:00Z";
    item2.archived_at = "2025-01-01T00:00:00Z";
    item2.last_modified_at = "2025-01-01T00:00:00Z";
    repo.insert(item2);

    auto results = repo.search("C++");
    ASSERT_TRUE(results.size() == 1);
    ASSERT_EQ(results[0].id, "item-010");

    auto all = repo.search("");
    ASSERT_TRUE(all.size() >= 2);

    db.close();
    TEST_PASS();
}

// --- CategoryRepository tests ---

static void test_category_crud() {
    TEST_BEGIN("CategoryRepository CRUD");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();
    CategoryRepository repo(db);

    Category cat;
    cat.id = "cat-001";
    cat.name = "Documents";
    cat.description = "Important documents";
    cat.color = "#ff0000";
    cat.icon = "document";
    cat.created_at = "2025-01-01T00:00:00Z";
    repo.insert(cat);

    auto found = repo.find_by_id("cat-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->name, "Documents");
    ASSERT_EQ(found->color, "#ff0000");

    cat.name = "Updated Docs";
    repo.update(cat);
    found = repo.find_by_id("cat-001");
    ASSERT_EQ(found->name, "Updated Docs");

    auto by_name = repo.find_by_name("Updated Docs");
    ASSERT_TRUE(by_name.has_value());

    auto all = repo.find_all();
    ASSERT_TRUE(all.size() >= 1);

    repo.remove("cat-001");
    found = repo.find_by_id("cat-001");
    ASSERT_TRUE(!found.has_value());

    db.close();
    TEST_PASS();
}

// --- TagRepository tests ---

static void test_tag_crud() {
    TEST_BEGIN("TagRepository CRUD");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();
    TagRepository repo(db);

    Tag tag;
    tag.id = "tag-001";
    tag.name = "important";
    tag.color = "#ff0000";
    repo.insert(tag);

    auto found = repo.find_by_id("tag-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->name, "important");

    auto by_name = repo.find_by_name("important");
    ASSERT_TRUE(by_name.has_value());

    auto all = repo.find_all();
    ASSERT_TRUE(all.size() >= 1);

    repo.remove("tag-001");
    found = repo.find_by_id("tag-001");
    ASSERT_TRUE(!found.has_value());

    db.close();
    TEST_PASS();
}

// --- VersionRepository tests ---

static void test_version_crud() {
    TEST_BEGIN("VersionRepository CRUD");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();

    ArchiveItemRepository item_repo(db);
    ArchiveItem item;
    item.id = "item-v1";
    item.name = "Versioned";
    item.type = ItemType::File;
    item.status = ItemStatus::Archived;
    item.original_path = "/tmp/v.txt";
    item.storage_path = "/data/items/item-v1/files/v.txt";
    item.size = 100;
    item.created_at = "2025-01-01T00:00:00Z";
    item.archived_at = "2025-01-01T00:00:00Z";
    item.last_modified_at = "2025-01-01T00:00:00Z";
    item_repo.insert(item);

    VersionRepository ver_repo(db);
    Version v1;
    v1.id = "ver-001";
    v1.item_id = "item-v1";
    v1.version_number = 1;
    v1.storage_path = "/data/items/item-v1/versions/v1.txt";
    v1.checksum = "abc";
    v1.size = 100;
    v1.created_at = "2025-01-01T00:00:00Z";
    ver_repo.insert(v1);

    Version v2;
    v2.id = "ver-002";
    v2.item_id = "item-v1";
    v2.version_number = 2;
    v2.storage_path = "/data/items/item-v1/versions/v2.txt";
    v2.checksum = "def";
    v2.size = 150;
    v2.created_at = "2025-01-02T00:00:00Z";
    ver_repo.insert(v2);

    auto versions = ver_repo.find_by_item("item-v1");
    ASSERT_TRUE(versions.size() == 2);

    auto latest = ver_repo.find_latest("item-v1");
    ASSERT_TRUE(latest.has_value());
    ASSERT_EQ(latest->version_number, 2);

    db.close();
    TEST_PASS();
}

// --- NoteRepository tests ---

static void test_note_crud() {
    TEST_BEGIN("NoteRepository CRUD");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();

    ArchiveItemRepository item_repo(db);
    ArchiveItem item;
    item.id = "item-n1";
    item.name = "Noted";
    item.type = ItemType::File;
    item.status = ItemStatus::Archived;
    item.original_path = "/tmp/n.txt";
    item.storage_path = "/data/items/item-n1/files/n.txt";
    item.size = 10;
    item.created_at = "2025-01-01T00:00:00Z";
    item.archived_at = "2025-01-01T00:00:00Z";
    item.last_modified_at = "2025-01-01T00:00:00Z";
    item_repo.insert(item);

    NoteRepository note_repo(db);
    Note note;
    note.id = "note-001";
    note.item_id = "item-n1";
    note.content = "This is a test note";
    note.created_at = "2025-01-01T00:00:00Z";
    note.updated_at = "2025-01-01T00:00:00Z";
    note_repo.insert(note);

    auto notes = note_repo.find_by_item("item-n1");
    ASSERT_TRUE(notes.size() == 1);
    ASSERT_EQ(notes[0].content, "This is a test note");

    notes[0].content = "Updated note";
    notes[0].updated_at = "2025-01-02T00:00:00Z";
    note_repo.update(notes[0]);

    notes = note_repo.find_by_item("item-n1");
    ASSERT_EQ(notes[0].content, "Updated note");

    note_repo.remove("note-001");
    notes = note_repo.find_by_item("item-n1");
    ASSERT_TRUE(notes.empty());

    db.close();
    TEST_PASS();
}

// --- ActivityRepository tests ---

static void test_activity_log() {
    TEST_BEGIN("ActivityRepository insert and query");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();

    ArchiveItemRepository item_repo(db);
    ArchiveItem item;
    item.id = "item-a1";
    item.name = "Active";
    item.type = ItemType::File;
    item.status = ItemStatus::Archived;
    item.original_path = "/tmp/a.txt";
    item.storage_path = "/data/items/item-a1/files/a.txt";
    item.size = 10;
    item.created_at = "2025-01-01T00:00:00Z";
    item.archived_at = "2025-01-01T00:00:00Z";
    item.last_modified_at = "2025-01-01T00:00:00Z";
    item_repo.insert(item);

    ActivityRepository act_repo(db);
    Activity act;
    act.id = "act-001";
    act.item_id = "item-a1";
    act.action = ActivityAction::Imported;
    act.details = "Imported from /home/user";
    act.created_at = "2025-01-01T00:00:00Z";
    act_repo.insert(act);

    auto activities = act_repo.find_by_item("item-a1");
    ASSERT_TRUE(activities.size() == 1);
    ASSERT_EQ(activities[0].action, ActivityAction::Imported);
    ASSERT_EQ(activities[0].details, "Imported from /home/user");

    auto recent = act_repo.find_recent(10);
    ASSERT_TRUE(recent.size() >= 1);

    db.close();
    TEST_PASS();
}

// --- Integration: item with category and tags ---

static void test_item_with_category_and_tags() {
    TEST_BEGIN("Integration: item + category + tags");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();

    CategoryRepository cat_repo(db);
    Category cat;
    cat.id = "cat-int";
    cat.name = "C++ Projects";
    cat.color = "#00599C";
    cat.created_at = "2025-01-01T00:00:00Z";
    cat_repo.insert(cat);

    TagRepository tag_repo(db);
    Tag tag1;
    tag1.id = "tag-int-1";
    tag1.name = "cpp";
    tag1.color = "#f34b7d";
    tag_repo.insert(tag1);

    Tag tag2;
    tag2.id = "tag-int-2";
    tag2.name = "archive";
    tag2.color = "#6366f1";
    tag_repo.insert(tag2);

    ArchiveItemRepository item_repo(db);
    ArchiveItem item;
    item.id = "item-int";
    item.name = "Archiver";
    item.type = ItemType::Project;
    item.status = ItemStatus::Archived;
    item.original_path = "/projects/archiver";
    item.storage_path = "/data/items/item-int/files";
    item.size = 5000;
    item.category_id = "cat-int";
    item.is_favorite = true;
    item.created_at = "2025-01-01T00:00:00Z";
    item.archived_at = "2025-01-01T00:00:00Z";
    item.last_modified_at = "2025-01-01T00:00:00Z";
    item_repo.insert(item);

    item_repo.add_tag("item-int", "tag-int-1");
    item_repo.add_tag("item-int", "tag-int-2");

    auto found = item_repo.find_by_id("item-int");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->category_id.value(), "cat-int");
    ASSERT_TRUE(found->is_favorite);
    ASSERT_EQ(found->tags.size(), 2);
    ASSERT_EQ(found->tags[0].name, "archive");
    ASSERT_EQ(found->tags[1].name, "cpp");

    db.close();
    TEST_PASS();
}

// --- Stats ---

static void test_dashboard_stats() {
    TEST_BEGIN("DashboardStats aggregation");
    cleanup();
    DatabaseManager db(TEST_DB);
    db.initialize();
    ArchiveItemRepository repo(db);

    for (int i = 0; i < 5; i++) {
        ArchiveItem item;
        item.id = "stat-" + std::to_string(i);
        item.name = "Item " + std::to_string(i);
        item.type = ItemType::File;
        item.status = ItemStatus::Archived;
        item.original_path = "/tmp/stat" + std::to_string(i);
        item.storage_path = "/data/items/stat-" + std::to_string(i);
        item.size = 100 * (i + 1);
        item.is_favorite = (i < 2);
        item.created_at = "2025-01-01T00:00:00Z";
        item.archived_at = "2025-01-01T00:00:00Z";
        item.last_modified_at = "2025-01-01T00:00:00Z";
        repo.insert(item);
    }

    auto stats = repo.get_stats();
    ASSERT_EQ(stats.total_items, 5);
    ASSERT_EQ(stats.archived_items, 5);
    ASSERT_EQ(stats.favorite_items, 2);
    ASSERT_EQ(stats.total_size, 1500u);

    db.close();
    TEST_PASS();
}

void run_storage_tests() {
    std::cout << "=== Storage Tests ===" << std::endl;

    test_db_open();
    test_db_schema_created();
    test_db_transaction();
    test_item_insert_and_find();
    test_item_update();
    test_item_remove();
    test_item_favorite();
    test_item_search();
    test_category_crud();
    test_tag_crud();
    test_version_crud();
    test_note_crud();
    test_activity_log();
    test_item_with_category_and_tags();
    test_dashboard_stats();
}
