#include "test_helpers.h"

#include "core/models/ArchiveItem.h"
#include "core/models/Category.h"
#include "core/models/Tag.h"
#include "core/models/Version.h"
#include "core/models/Note.h"
#include "core/models/Activity.h"
#include "core/models/StoredObject.h"

using namespace archive::core;

static void test_archive_item_default() {
    TEST_BEGIN("ArchiveItem default construction");
    ArchiveItem item;
    ASSERT_TRUE(item.id.empty());
    ASSERT_TRUE(item.name.empty());
    ASSERT_EQ(item.type, ItemType::File);
    ASSERT_EQ(item.status, ItemStatus::Archived);
    ASSERT_EQ(item.size, 0u);
    ASSERT_EQ(item.file_count, 0);
    ASSERT_EQ(item.current_version, 1);
    ASSERT_TRUE(item.tags.empty());
    TEST_PASS();
}

static void test_archive_item_with_values() {
    TEST_BEGIN("ArchiveItem construction with values");
    ArchiveItem item;
    item.id = "test-id-123";
    item.name = "My Project";
    item.type = ItemType::Project;
    item.status = ItemStatus::Archived;
    item.description = "A test project";
    item.original_path = "/home/user/projects/test";
    item.storage_path = "/home/user/.archive-data/items/test-id-123/files";
    item.size = 1024 * 50;
    item.file_count = 42;
    item.category_id = "cat-1";
    item.current_version = 3;
    item.is_favorite = true;

    ASSERT_EQ(item.id, "test-id-123");
    ASSERT_EQ(item.name, "My Project");
    ASSERT_EQ(item.type, ItemType::Project);
    ASSERT_EQ(item.status, ItemStatus::Archived);
    ASSERT_EQ(item.description, "A test project");
    ASSERT_EQ(item.size, 51200u);
    ASSERT_EQ(item.file_count, 42);
    ASSERT_TRUE(item.category_id.has_value());
    ASSERT_EQ(item.category_id.value(), "cat-1");
    ASSERT_EQ(item.current_version, 3);
    ASSERT_TRUE(item.is_favorite);
    TEST_PASS();
}

static void test_archive_item_favorite_independent_of_status() {
    TEST_BEGIN("ArchiveItem favorite is independent of status");
    ArchiveItem item;
    item.status = ItemStatus::Archived;
    item.is_favorite = true;
    ASSERT_EQ(item.status, ItemStatus::Archived);
    ASSERT_TRUE(item.is_favorite);

    item.status = ItemStatus::Deleted;
    ASSERT_TRUE(item.is_favorite);
    TEST_PASS();
}

static void test_category_default() {
    TEST_BEGIN("Category default construction");
    Category cat;
    ASSERT_TRUE(cat.id.empty());
    ASSERT_TRUE(cat.name.empty());
    ASSERT_TRUE(cat.parent_id == std::nullopt);
    TEST_PASS();
}

static void test_category_with_parent() {
    TEST_BEGIN("Category with parent_id");
    Category cat("id-1", "C++", "C++ projects", "#00599C", "", "parent-id");
    ASSERT_EQ(cat.id, "id-1");
    ASSERT_EQ(cat.name, "C++");
    ASSERT_EQ(cat.color, "#00599C");
    ASSERT_TRUE(cat.parent_id.has_value());
    ASSERT_EQ(cat.parent_id.value(), "parent-id");
    TEST_PASS();
}

static void test_tag() {
    TEST_BEGIN("Tag construction");
    Tag tag("tag-1", "important", "#ff0000");
    ASSERT_EQ(tag.id, "tag-1");
    ASSERT_EQ(tag.name, "important");
    ASSERT_EQ(tag.color, "#ff0000");
    TEST_PASS();
}

static void test_version() {
    TEST_BEGIN("Version construction");
    Version v("v-1", "item-1", 2, "/storage/path", "abc123", 2048, "Second version");
    ASSERT_EQ(v.id, "v-1");
    ASSERT_EQ(v.item_id, "item-1");
    ASSERT_EQ(v.version_number, 2);
    ASSERT_EQ(v.storage_path, "/storage/path");
    ASSERT_EQ(v.checksum, "abc123");
    ASSERT_EQ(v.size, 2048u);
    ASSERT_EQ(v.notes, "Second version");
    TEST_PASS();
}

static void test_note() {
    TEST_BEGIN("Note construction");
    Note note("n-1", "item-1", "This is a note");
    ASSERT_EQ(note.id, "n-1");
    ASSERT_EQ(note.item_id, "item-1");
    ASSERT_EQ(note.content, "This is a note");
    TEST_PASS();
}

static void test_activity() {
    TEST_BEGIN("Activity construction");
    Activity act("a-1", "item-1", ActivityAction::Imported, "Archived from /path");
    ASSERT_EQ(act.id, "a-1");
    ASSERT_EQ(act.item_id, "item-1");
    ASSERT_EQ(act.action, ActivityAction::Imported);
    ASSERT_EQ(act.details, "Archived from /path");
    TEST_PASS();
}

static void test_stored_object() {
    TEST_BEGIN("StoredObject construction");
    StoredObject obj("so-1", "item-1", "ver-1", "/storage/files", 4096, "checksum", "2025-01-01T00:00:00Z");
    ASSERT_EQ(obj.id, "so-1");
    ASSERT_EQ(obj.item_id, "item-1");
    ASSERT_EQ(obj.version_id, "ver-1");
    ASSERT_EQ(obj.storage_path, "/storage/files");
    ASSERT_EQ(obj.size, 4096u);
    ASSERT_EQ(obj.checksum, "checksum");
    TEST_PASS();
}

void run_model_tests() {
    std::cout << "=== Model Tests ===" << std::endl;

    test_archive_item_default();
    test_archive_item_with_values();
    test_archive_item_favorite_independent_of_status();
    test_category_default();
    test_category_with_parent();
    test_tag();
    test_version();
    test_note();
    test_activity();
    test_stored_object();
}
