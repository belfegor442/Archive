#include "test_helpers.h"

#include "storage/DatabaseManager.h"
#include "storage/ArchiveItemRepository.h"
#include "storage/CategoryRepository.h"
#include "storage/TagRepository.h"
#include "storage/VersionRepository.h"
#include "storage/NoteRepository.h"
#include "storage/ActivityRepository.h"
#include "storage/StoredObjectRepository.h"
#include "filesystem/StorageManager.h"
#include "filesystem/FileUtils.h"
#include "services/ImportService.h"
#include "services/VersionService.h"
#include "services/IntegrityService.h"
#include "services/ProjectDetector.h"
#include "services/UpdateService.h"
#include "hashing/FileHasher.h"
#include "core/enums/ItemType.h"
#include "core/enums/ItemStatus.h"
#include "core/enums/IntegrityState.h"
#include "core/utils/Uuid.h"

#include <fstream>
#include <cstdio>
#include <filesystem>

using namespace archive::storage;
using namespace archive::filesystem;
using namespace archive::services;
using namespace archive::hashing;
using namespace archive::core;

static const std::string TEST_BASE = "integ_test";
static const std::string TEST_ITEMS = TEST_BASE + "/items";

static void setup_base() {
    if (std::filesystem::exists(TEST_BASE)) {
        std::filesystem::remove_all(TEST_BASE);
    }
    FileUtils::create_directories(TEST_ITEMS);
}

static void cleanup_base() {
    if (std::filesystem::exists(TEST_BASE)) {
        std::filesystem::remove_all(TEST_BASE);
    }
}

static void create_test_file(const std::string& path, const std::string& content) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

// === Test 1: Import file -> verify -> integrity ===

static void test_import_file_verify_integrity() {
    TEST_BEGIN("INT: Import file, verify integrity");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    std::string src = TEST_BASE + "/src_file.txt";
    create_test_file(src, "Hello integration test");

    auto result = import_svc.import_single(src, std::nullopt);
    ASSERT_EQ(result.success_count(), 1);
    ASSERT_EQ(result.error_count(), 0);

    std::string item_id = result.items[0].id;
    auto item = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item.has_value());
    ASSERT_TRUE(FileUtils::file_exists(item->storage_path));

    auto versions = ver_repo.find_by_item(item_id);
    ASSERT_EQ(versions.size(), 1u);
    ASSERT_EQ(versions[0].version_number, 1);

    auto sos = so_repo.find_by_item(item_id);
    ASSERT_EQ(sos.size(), 1u);

    auto verification = integrity.verify_item(item_id);
    ASSERT_EQ(verification.items.size(), 1u);
    ASSERT_EQ(verification.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

// === Test 2: Import folder -> verify all files ===

static void test_import_folder_verify_all() {
    TEST_BEGIN("INT: Import folder, verify all files");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    std::string src_dir = TEST_BASE + "/src_folder";
    create_test_file(src_dir + "/a.txt", "content A");
    create_test_file(src_dir + "/sub/b.txt", "content B");
    create_test_file(src_dir + "/sub/c.txt", "content C");

    auto result = import_svc.import_folder(src_dir, std::nullopt);
    ASSERT_EQ(result.success_count(), 1);
    ASSERT_EQ(result.error_count(), 0);

    std::string item_id = result.items[0].id;
    auto item = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item.has_value());
    ASSERT_EQ(item->type, ItemType::Folder);
    ASSERT_EQ(item->file_count, 3);

    auto verification = integrity.verify_item(item_id);
    ASSERT_EQ(verification.items.size(), 1u);
    ASSERT_EQ(verification.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

// === Test 3: Import -> modify source -> archive remains VALID ===

static void test_import_source_modified_archive_valid() {
    TEST_BEGIN("INT: Source modified, archive remains valid");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    std::string src = TEST_BASE + "/original.txt";
    create_test_file(src, "original content");

    auto result = import_svc.import_single(src, std::nullopt);
    ASSERT_EQ(result.success_count(), 1);
    std::string item_id = result.items[0].id;

    std::ofstream f(src, std::ios::trunc);
    f << "MODIFIED content after import";
    f.close();

    auto verification = integrity.verify_item(item_id);
    ASSERT_EQ(verification.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

// === Test 4: Import -> modify archived file -> MODIFIED ===

static void test_import_modify_archived_detected() {
    TEST_BEGIN("INT: Modify archived file, integrity detects modification");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    std::string src = TEST_BASE + "/tamper.txt";
    create_test_file(src, "original data");

    auto result = import_svc.import_single(src, std::nullopt);
    std::string item_id = result.items[0].id;
    auto item = item_repo.find_by_id(item_id);

    std::ofstream f(item->storage_path, std::ios::trunc);
    f << "TAMPERED DATA";
    f.close();

    auto verification = integrity.verify_item(item_id);
    ASSERT_EQ(verification.items[0].state, IntegrityState::Modified);

    cleanup_base();
    TEST_PASS();
}

// === Test 5: Import -> verify copy size match ===

static void test_import_copy_size_matches() {
    TEST_BEGIN("INT: Imported copy size matches original");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);

    std::string src = TEST_BASE + "/sized.txt";
    std::string content(4096, 'X');
    create_test_file(src, content);

    auto result = import_svc.import_single(src, std::nullopt);
    if (result.error_count() > 0) {
        std::cerr << "    Import error: " << result.errors[0].error << std::endl;
    }
    ASSERT_EQ(result.success_count(), 1);

    std::string item_id = result.items[0].id;
    auto item = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item.has_value());
    ASSERT_EQ(item->size, 4096u);

    uint64_t stored_size = FileUtils::file_size(item->storage_path);
    ASSERT_EQ(stored_size, 4096u);

    cleanup_base();
    TEST_PASS();
}

// === Test 6: Import -> rollback on invalid source ===

static void test_import_nonexistent_rollback() {
    TEST_BEGIN("INT: Import nonexistent path returns error, no DB ghost");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);

    auto result = import_svc.import_single("/nonexistent/path/file.txt", std::nullopt);
    ASSERT_EQ(result.success_count(), 0);
    ASSERT_EQ(result.error_count(), 1);
    ASSERT_EQ(item_repo.count(), 0);

    cleanup_base();
    TEST_PASS();
}

// === Test 7: Two imports with same filename -> no overwrite ===

static void test_import_same_name_no_overwrite() {
    TEST_BEGIN("INT: Two imports with same filename, no overwrite");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);

    std::string src = TEST_BASE + "/dup.txt";
    create_test_file(src, "first version");

    auto r1 = import_svc.import_single(src, std::nullopt);
    ASSERT_EQ(r1.success_count(), 1);

    create_test_file(src, "second version");
    auto r2 = import_svc.import_single(src, std::nullopt);
    ASSERT_EQ(r2.success_count(), 1);

    ASSERT_EQ(item_repo.count(), 2);

    std::string id1 = r1.items[0].id;
    std::string id2 = r2.items[0].id;
    auto item1 = item_repo.find_by_id(id1);
    auto item2 = item_repo.find_by_id(id2);
    ASSERT_TRUE(item1.has_value());
    ASSERT_TRUE(item2.has_value());
    ASSERT_TRUE(item1->storage_path != item2->storage_path);
    ASSERT_TRUE(FileUtils::file_exists(item1->storage_path));
    ASSERT_TRUE(FileUtils::file_exists(item2->storage_path));

    cleanup_base();
    TEST_PASS();
}

// === Test 8: Folder with nested dirs -> verify complete structure ===

static void test_import_nested_folder_structure() {
    TEST_BEGIN("INT: Nested folder import preserves structure");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);

    std::string src = TEST_BASE + "/deep";
    create_test_file(src + "/root.txt", "root");
    create_test_file(src + "/a/b/c.txt", "nested c");
    create_test_file(src + "/a/d.txt", "nested d");
    create_test_file(src + "/x/y/z/w.txt", "deep w");

    auto result = import_svc.import_folder(src, std::nullopt);
    ASSERT_EQ(result.success_count(), 1);
    std::string item_id = result.items[0].id;

    auto item = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item.has_value());
    ASSERT_EQ(item->file_count, 4);

    std::string dest = item->storage_path;
    ASSERT_TRUE(FileUtils::file_exists(dest + "/root.txt"));
    ASSERT_TRUE(FileUtils::file_exists(dest + "/a/b/c.txt"));
    ASSERT_TRUE(FileUtils::file_exists(dest + "/a/d.txt"));
    ASSERT_TRUE(FileUtils::file_exists(dest + "/x/y/z/w.txt"));

    cleanup_base();
    TEST_PASS();
}

// === Test 9: Create version 2 -> verify v1 and v2 ===

static void test_import_version_create_and_verify() {
    TEST_BEGIN("INT: Create version 2, verify v1 and v2");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);
    VersionService version_svc(db, ver_repo, item_repo, act_repo, so_repo, storage);
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    std::string src = TEST_BASE + "/versioned.txt";
    create_test_file(src, "version 1 content");

    auto result = import_svc.import_single(src, std::nullopt);
    ASSERT_EQ(result.success_count(), 1);
    std::string item_id = result.items[0].id;

    std::string src2 = TEST_BASE + "/versioned_v2.txt";
    create_test_file(src2, "version 2 content - updated");
    auto ver2 = version_svc.create_version(item_id, src2, "second version");

    auto versions = ver_repo.find_by_item(item_id);
    ASSERT_EQ(versions.size(), 2u);

    auto v1_check = integrity.verify_version(versions[1].id);
    ASSERT_EQ(v1_check.items.size(), 1u);
    ASSERT_EQ(v1_check.items[0].state, IntegrityState::Valid);

    auto v2_check = integrity.verify_version(versions[0].id);
    ASSERT_EQ(v2_check.items.size(), 1u);
    ASSERT_EQ(v2_check.items[0].state, IntegrityState::Valid);

    auto item = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item.has_value());
    ASSERT_EQ(item->current_version, 2);

    cleanup_base();
    TEST_PASS();
}

// === Test 10: Import -> trash -> restore -> verify integrity ===

static void test_import_trash_restore_integrity() {
    TEST_BEGIN("INT: Import, trash, restore, verify integrity");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    CategoryRepository cat_repo(db);
    TagRepository tag_repo(db);
    ActivityRepository act_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    ProjectDetector detector;
    ImportService import_svc(db, item_repo, cat_repo, tag_repo, act_repo, ver_repo, so_repo, storage, detector);
    UpdateService update_svc(db, item_repo, act_repo, storage);
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    std::string src = TEST_BASE + "/trash_restore.txt";
    create_test_file(src, "trash and restore test");

    auto result = import_svc.import_single(src, std::nullopt);
    std::string item_id = result.items[0].id;

    auto v1 = integrity.verify_item(item_id);
    ASSERT_EQ(v1.items[0].state, IntegrityState::Valid);

    update_svc.move_to_trash(item_id);
    auto trashed = item_repo.find_by_id(item_id);
    ASSERT_TRUE(trashed.has_value());
    ASSERT_EQ(trashed->status, ItemStatus::Deleted);

    update_svc.restore_from_trash(item_id);
    auto restored = item_repo.find_by_id(item_id);
    ASSERT_TRUE(restored.has_value());
    ASSERT_EQ(restored->status, ItemStatus::Archived);

    auto v2 = integrity.verify_item(item_id);
    ASSERT_EQ(v2.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

void run_integration_tests() {
    std::cout << "=== Integration Tests ===" << std::endl;

    test_import_file_verify_integrity();
    test_import_folder_verify_all();
    test_import_source_modified_archive_valid();
    test_import_modify_archived_detected();
    test_import_copy_size_matches();
    test_import_nonexistent_rollback();
    test_import_same_name_no_overwrite();
    test_import_nested_folder_structure();
    test_import_version_create_and_verify();
    test_import_trash_restore_integrity();
}
