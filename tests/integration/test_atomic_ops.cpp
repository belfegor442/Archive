#include "test_helpers.h"

#include "storage/DatabaseManager.h"
#include "storage/ArchiveItemRepository.h"
#include "storage/CategoryRepository.h"
#include "storage/TagRepository.h"
#include "storage/VersionRepository.h"
#include "storage/NoteRepository.h"
#include "storage/ActivityRepository.h"
#include "storage/StoredObjectRepository.h"
#include "storage/Transaction.h"
#include "filesystem/StorageManager.h"
#include "filesystem/FileUtils.h"
#include "filesystem/FilesystemTracker.h"
#include "filesystem/StagingManager.h"
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

static const std::string TEST_BASE = "atomic_test";
static const std::string TEST_ITEMS = TEST_BASE + "/items";

static void setup_base() {
    if (std::filesystem::exists(TEST_BASE)) {
        std::error_code ec;
        std::filesystem::remove_all(TEST_BASE, ec);
    }
    FileUtils::create_directories(TEST_ITEMS);
}

static void cleanup_base() {
    if (std::filesystem::exists(TEST_BASE)) {
        std::error_code ec;
        std::filesystem::remove_all(TEST_BASE, ec);
    }
}

static void create_test_file(const std::string& path, const std::string& content) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

// === Transaction RAII Tests ===

static void test_transaction_commit() {
    TEST_BEGIN("Transaction RAII commit");
    DatabaseManager db(":memory:");
    db.initialize();

    {
        Transaction tx(db);
        db.execute("CREATE TABLE test_t (id INTEGER PRIMARY KEY)");
        db.execute("INSERT INTO test_t (id) VALUES (1)");
        tx.commit();
    }

    auto stmt = db.prepare("SELECT COUNT(*) FROM test_t");
    ASSERT_TRUE(stmt.step());
    ASSERT_EQ(stmt.column_int(0), 1);
    TEST_PASS();
}

static void test_transaction_rollback() {
    TEST_BEGIN("Transaction RAII rollback on scope exit");
    DatabaseManager db(":memory:");
    db.initialize();
    db.execute("CREATE TABLE test_t (id INTEGER PRIMARY KEY)");

    {
        Transaction tx(db);
        db.execute("INSERT INTO test_t (id) VALUES (1)");
    }

    auto stmt = db.prepare("SELECT COUNT(*) FROM test_t");
    ASSERT_TRUE(stmt.step());
    ASSERT_EQ(stmt.column_int(0), 0);
    TEST_PASS();
}

static void test_transaction_explicit_rollback() {
    TEST_BEGIN("Transaction explicit rollback");
    DatabaseManager db(":memory:");
    db.initialize();
    db.execute("CREATE TABLE test_t (id INTEGER PRIMARY KEY)");

    {
        Transaction tx(db);
        db.execute("INSERT INTO test_t (id) VALUES (1)");
        tx.rollback();
    }

    auto stmt = db.prepare("SELECT COUNT(*) FROM test_t");
    ASSERT_TRUE(stmt.step());
    ASSERT_EQ(stmt.column_int(0), 0);
    TEST_PASS();
}

// === FilesystemTracker Tests ===

static void test_tracker_compensate() {
    TEST_BEGIN("FilesystemTracker compensate removes files");
    std::string dir = TEST_BASE + "/tracker_test";
    FileUtils::create_directories(dir);

    std::string file1 = dir + "/file1.txt";
    std::string file2 = dir + "/file2.txt";
    create_test_file(file1, "data1");
    create_test_file(file2, "data2");

    FilesystemTracker tracker;
    tracker.track_copied_file(file1);
    tracker.track_copied_file(file2);

    ASSERT_TRUE(FileUtils::file_exists(file1));
    ASSERT_TRUE(FileUtils::file_exists(file2));

    tracker.compensate();

    ASSERT_TRUE(!FileUtils::file_exists(file1));
    ASSERT_TRUE(!FileUtils::file_exists(file2));

    std::filesystem::remove_all(dir);
    TEST_PASS();
}

static void test_tracker_dirs_and_files() {
    TEST_BEGIN("FilesystemTracker tracks dirs and files");
    FilesystemTracker tracker;
    ASSERT_TRUE(!tracker.has_operations());

    tracker.track_created_dir(TEST_BASE + "/newdir");
    tracker.track_copied_file(TEST_BASE + "/newfile.txt");
    ASSERT_TRUE(tracker.has_operations());

    tracker.clear();
    ASSERT_TRUE(!tracker.has_operations());
    TEST_PASS();
}

// === StagingManager Tests ===

static void test_staging_create_and_detect() {
    TEST_BEGIN("StagingManager create and detect abandoned");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_op");

    ASSERT_TRUE(staging.staging_dir_exists(op_id));

    auto abandoned = staging.detect_abandoned_staging();
    ASSERT_EQ(abandoned.size(), 1u);
    ASSERT_EQ(abandoned[0].operation_id, op_id);

    staging.cleanup_staging(op_id);
    ASSERT_TRUE(!staging.staging_dir_exists(op_id));

    cleanup_base();
    TEST_PASS();
}

static void test_staging_finalize() {
    TEST_BEGIN("StagingManager finalize moves files to dest");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_finalize");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "test content");
    std::string staged = staging.stage_file(op_id, src);
    ASSERT_TRUE(FileUtils::file_exists(staged));

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    staging.finalize_staging(op_id, dest_dir);

    std::string dest_file = dest_dir + "/" + FileUtils::file_name(staged);
    ASSERT_TRUE(FileUtils::file_exists(dest_file));

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Import Rollback Tests ===

static void test_import_rollback_no_ghost_item() {
    TEST_BEGIN("Import rollback: no ghost item in DB after failure");
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

    auto result = import_svc.import_single("/nonexistent/file.txt", std::nullopt);
    ASSERT_EQ(result.error_count(), 1);
    ASSERT_EQ(item_repo.count(), 0);

    auto versions = ver_repo.find_by_item("nonexistent");
    ASSERT_TRUE(versions.empty());

    cleanup_base();
    TEST_PASS();
}

static void test_import_rollback_no_stored_object() {
    TEST_BEGIN("Import rollback: no orphan StoredObject after copy failure");
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

    std::string src = TEST_BASE + "/test_file.txt";
    create_test_file(src, "test content");

    auto result = import_svc.import_single(src, std::nullopt);
    ASSERT_EQ(result.success_count(), 1);

    std::string item_id = result.items[0].id;
    auto item = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item.has_value());

    auto versions = ver_repo.find_by_item(item_id);
    ASSERT_EQ(versions.size(), 1u);

    auto sos = so_repo.find_by_item(item_id);
    ASSERT_EQ(sos.size(), 1u);

    ASSERT_TRUE(FileUtils::file_exists(item->storage_path));
    ASSERT_TRUE(FileUtils::file_exists(versions[0].storage_path));
    ASSERT_TRUE(FileUtils::file_exists(sos[0].storage_path));

    cleanup_base();
    TEST_PASS();
}

// === Version Rollback Tests ===

static void test_version_create_rollback() {
    TEST_BEGIN("Version create: rollback on failure leaves consistent state");
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

    std::string src = TEST_BASE + "/versioned.txt";
    create_test_file(src, "original content");

    auto result = import_svc.import_single(src, std::nullopt);
    ASSERT_EQ(result.success_count(), 1);
    std::string item_id = result.items[0].id;

    auto item_before = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item_before.has_value());
    ASSERT_EQ(item_before->current_version, 1);

    ASSERT_THROW(version_svc.create_version(item_id, "/nonexistent/file.txt"));

    auto item_after = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item_after.has_value());
    ASSERT_EQ(item_after->current_version, 1);

    auto versions = ver_repo.find_by_item(item_id);
    ASSERT_EQ(versions.size(), 1u);

    cleanup_base();
    TEST_PASS();
}

static void test_version_create_rollback_no_orphan() {
    TEST_BEGIN("Version create failure: no orphan version or stored object");
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

    std::string src = TEST_BASE + "/versioned2.txt";
    create_test_file(src, "original content");

    auto result = import_svc.import_single(src, std::nullopt);
    std::string item_id = result.items[0].id;

    ASSERT_THROW(version_svc.create_version(item_id, "/nonexistent/file.txt"));

    auto versions = ver_repo.find_by_item(item_id);
    ASSERT_EQ(versions.size(), 1u);

    auto sos = so_repo.find_by_item(item_id);
    ASSERT_EQ(sos.size(), 1u);

    cleanup_base();
    TEST_PASS();
}

// === Version Integrity Tests ===

static void test_version_v1_v2_integrity() {
    TEST_BEGIN("Version v1 and v2 both maintain integrity");
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

    std::string src1 = TEST_BASE + "/ver_test.txt";
    create_test_file(src1, "version 1");
    auto r1 = import_svc.import_single(src1, std::nullopt);
    std::string item_id = r1.items[0].id;

    std::string src2 = TEST_BASE + "/ver_test_v2.txt";
    create_test_file(src2, "version 2 updated");
    version_svc.create_version(item_id, src2, "second version");

    auto versions = ver_repo.find_by_item(item_id);
    ASSERT_EQ(versions.size(), 2u);

    auto v1_check = integrity.verify_version(versions[1].id);
    ASSERT_EQ(v1_check.items[0].state, IntegrityState::Valid);

    auto v2_check = integrity.verify_version(versions[0].id);
    ASSERT_EQ(v2_check.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

// === Restore Tests ===

static void test_restore_v1_then_v2() {
    TEST_BEGIN("Restore v1 then v2, both valid");
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

    std::string src1 = TEST_BASE + "/restore_test.txt";
    create_test_file(src1, "version 1 content");
    auto r1 = import_svc.import_single(src1, std::nullopt);
    std::string item_id = r1.items[0].id;

    std::string src2 = TEST_BASE + "/restore_test_v2.txt";
    create_test_file(src2, "version 2 content updated");
    version_svc.create_version(item_id, src2, "v2");

    auto versions = ver_repo.find_by_item(item_id);
    ASSERT_EQ(versions.size(), 2u);
    std::string v1_id = versions[1].id;
    std::string v2_id = versions[0].id;

    version_svc.restore(item_id, v1_id);
    auto item1 = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item1.has_value());
    ASSERT_EQ(item1->current_version, 1);
    auto check1 = integrity.verify_item(item_id);
    ASSERT_EQ(check1.items[0].state, IntegrityState::Valid);

    version_svc.restore(item_id, v2_id);
    auto item2 = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item2.has_value());
    ASSERT_EQ(item2->current_version, 2);
    auto check2 = integrity.verify_item(item_id);
    ASSERT_EQ(check2.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

static void test_restore_rollback() {
    TEST_BEGIN("Restore failure leaves consistent state");
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

    std::string src = TEST_BASE + "/restore_rollback.txt";
    create_test_file(src, "content");
    auto r = import_svc.import_single(src, std::nullopt);
    std::string item_id = r.items[0].id;

    auto item_before = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item_before.has_value());
    std::string orig_path = item_before->storage_path;
    int orig_version = item_before->current_version;

    ASSERT_THROW(version_svc.restore(item_id, "nonexistent_version_id"));

    auto item_after = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item_after.has_value());
    ASSERT_EQ(item_after->current_version, orig_version);
    ASSERT_EQ(item_after->storage_path, orig_path);

    cleanup_base();
    TEST_PASS();
}

// === Integrity Consistency Tests ===

static void test_consistency_clean() {
    TEST_BEGIN("ConsistencyReport: clean archive");
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

    std::string src = TEST_BASE + "/consistency.txt";
    create_test_file(src, "consistent content");
    import_svc.import_single(src, std::nullopt);

    auto report = integrity.check_consistency();
    ASSERT_TRUE(report.is_clean());

    cleanup_base();
    TEST_PASS();
}

static void test_consistency_item_without_version() {
    TEST_BEGIN("ConsistencyReport: item without version detected");
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
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    ArchiveItem item;
    item.id = "no-ver-item"; item.name = "NoVersion"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10;
    item.created_at = ""; item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    auto report = integrity.check_consistency();
    ASSERT_TRUE(!report.is_clean());
    ASSERT_TRUE(report.broken_relations > 0);

    cleanup_base();
    TEST_PASS();
}

static void test_consistency_stored_object_without_file() {
    TEST_BEGIN("ConsistencyReport: missing file detected");
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
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    ArchiveItem item;
    item.id = "missing-file"; item.name = "MissingFile"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/nonexistent/path.txt"; item.size = 10;
    item.checksum = "abc"; item.created_at = ""; item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    Version ver;
    ver.id = "ver-missing"; ver.item_id = "missing-file";
    ver.version_number = 1; ver.storage_path = "/nonexistent/path.txt";
    ver.checksum = "abc"; ver.size = 10; ver.created_at = "";
    ver_repo.insert(ver);

    StoredObject so;
    so.id = "so-missing"; so.item_id = "missing-file";
    so.version_id = "ver-missing"; so.storage_path = "/nonexistent/path.txt";
    so.size = 10; so.checksum = "abc"; so.created_at = "";
    so_repo.insert(so);

    auto report = integrity.check_consistency();
    ASSERT_TRUE(!report.is_clean());
    ASSERT_TRUE(report.missing_objects > 0);

    cleanup_base();
    TEST_PASS();
}

// === Global Consistency After Operations ===

static void test_consistency_after_full_lifecycle() {
    TEST_BEGIN("Consistency after import+version+restore+trash");
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
    UpdateService update_svc(item_repo, act_repo, storage);
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    std::string src1 = TEST_BASE + "/lifecycle.txt";
    create_test_file(src1, "lifecycle v1");
    auto r = import_svc.import_single(src1, std::nullopt);
    std::string item_id = r.items[0].id;

    std::string src2 = TEST_BASE + "/lifecycle_v2.txt";
    create_test_file(src2, "lifecycle v2");
    version_svc.create_version(item_id, src2, "v2");

    update_svc.move_to_trash(item_id);
    update_svc.restore_from_trash(item_id);

    auto versions = ver_repo.find_by_item(item_id);
    ASSERT_TRUE(versions.size() >= 2);

    auto report = integrity.check_consistency();
    ASSERT_TRUE(report.is_clean());

    auto verification = integrity.verify_item(item_id);
    ASSERT_EQ(verification.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

// === Collision Tests ===

static void test_import_same_name_different_content() {
    TEST_BEGIN("Import same name: different files stored separately");
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

    std::string src1 = TEST_BASE + "/collision1.txt";
    std::string src2 = TEST_BASE + "/collision2.txt";
    create_test_file(src1, "first content");
    auto r1 = import_svc.import_single(src1, std::nullopt);
    ASSERT_EQ(r1.success_count(), 1);

    create_test_file(src2, "second content");
    auto r2 = import_svc.import_single(src2, std::nullopt);
    ASSERT_EQ(r2.success_count(), 1);

    std::string id1 = r1.items[0].id;
    std::string id2 = r2.items[0].id;
    auto item1 = item_repo.find_by_id(id1);
    auto item2 = item_repo.find_by_id(id2);

    ASSERT_TRUE(item1->storage_path != item2->storage_path);
    ASSERT_TRUE(FileUtils::file_exists(item1->storage_path));
    ASSERT_TRUE(FileUtils::file_exists(item2->storage_path));

    cleanup_base();
    TEST_PASS();
}

// === Empty File Import ===

static void test_import_empty_file() {
    TEST_BEGIN("Import empty file succeeds");
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

    std::string src = TEST_BASE + "/empty.txt";
    create_test_file(src, "");

    auto result = import_svc.import_single(src, std::nullopt);
    ASSERT_EQ(result.success_count(), 1);

    auto verification = integrity.verify_item(result.items[0].id);
    ASSERT_EQ(verification.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

// === Folder Import with Nested Dirs ===

static void test_folder_import_deep_nesting() {
    TEST_BEGIN("Folder import: deep nesting preserved");
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

    std::string src = TEST_BASE + "/deep_src";
    create_test_file(src + "/a/b/c/d/e.txt", "deep content");
    create_test_file(src + "/x/y.txt", "shallow");

    auto result = import_svc.import_folder(src, std::nullopt);
    ASSERT_EQ(result.success_count(), 1);

    auto verification = integrity.verify_item(result.items[0].id);
    ASSERT_EQ(verification.items[0].state, IntegrityState::Valid);

    auto item = item_repo.find_by_id(result.items[0].id);
    ASSERT_TRUE(item.has_value());
    ASSERT_TRUE(FileUtils::file_exists(item->storage_path + "/a/b/c/d/e.txt"));
    ASSERT_TRUE(FileUtils::file_exists(item->storage_path + "/x/y.txt"));

    cleanup_base();
    TEST_PASS();
}

// === Database Constraint Tests ===

static void test_version_unique_constraint() {
    TEST_BEGIN("DB constraint: UNIQUE(item_id, version_number) enforced");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    VersionRepository ver_repo(db);

    ArchiveItem item;
    item.id = "constraint-item"; item.name = "Test"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10;
    item.created_at = ""; item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    Version v1;
    v1.id = "v1"; v1.item_id = "constraint-item";
    v1.version_number = 1; v1.storage_path = "/v1";
    v1.size = 10; v1.created_at = "";
    ver_repo.insert(v1);

    ASSERT_THROW({
        Version v2;
        v2.id = "v2"; v2.item_id = "constraint-item";
        v2.version_number = 1; v2.storage_path = "/v2";
        v2.size = 10; v2.created_at = "";
        ver_repo.insert(v2);
    });

    cleanup_base();
    TEST_PASS();
}

static void test_stored_object_unique_version_constraint() {
    TEST_BEGIN("DB constraint: UNIQUE(version_id) on stored_objects enforced");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    ArchiveItemRepository item_repo(db);
    VersionRepository ver_repo(db);
    StoredObjectRepository so_repo(db);

    ArchiveItem item;
    item.id = "item1"; item.name = "Test"; item.type = ItemType::File;
    item.status = ItemStatus::Archived; item.original_path = "/a";
    item.storage_path = "/b"; item.size = 10;
    item.created_at = ""; item.archived_at = ""; item.last_modified_at = "";
    item_repo.insert(item);

    Version v1;
    v1.id = "ver1"; v1.item_id = "item1";
    v1.version_number = 1; v1.storage_path = "/v1";
    v1.size = 10; v1.created_at = "";
    ver_repo.insert(v1);

    StoredObject so1;
    so1.id = "so1"; so1.item_id = "item1";
    so1.version_id = "ver1"; so1.storage_path = "/p1";
    so1.size = 10; so1.created_at = "";
    so_repo.insert(so1);

    ASSERT_THROW({
        StoredObject so2;
        so2.id = "so2"; so2.item_id = "item1";
        so2.version_id = "ver1"; so2.storage_path = "/p2";
        so2.size = 20; so2.created_at = "";
        so_repo.insert(so2);
    });

    cleanup_base();
    TEST_PASS();
}

// === Multiple Imports Concurrent-ish ===

static void test_multiple_sequential_imports() {
    TEST_BEGIN("Multiple sequential imports: all consistent");
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

    for (int i = 0; i < 5; i++) {
        std::string src = TEST_BASE + "/multi_" + std::to_string(i) + ".txt";
        create_test_file(src, "content " + std::to_string(i));
        auto result = import_svc.import_single(src, std::nullopt);
        ASSERT_EQ(result.success_count(), 1);
    }

    ASSERT_EQ(item_repo.count(), 5);

    auto report = integrity.check_consistency();
    ASSERT_TRUE(report.is_clean());

    auto verification = integrity.verify_all();
    ASSERT_TRUE(verification.all_valid());

    cleanup_base();
    TEST_PASS();
}

// === Permanent Delete Atomicity ===

static void test_permanent_delete_consistency() {
    TEST_BEGIN("Permanent delete: DB and FS consistent");
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
    UpdateService update_svc(item_repo, act_repo, storage);

    std::string src = TEST_BASE + "/delete_me.txt";
    create_test_file(src, "delete test");
    auto result = import_svc.import_single(src, std::nullopt);
    std::string item_id = result.items[0].id;
    auto item = item_repo.find_by_id(item_id);
    std::string storage_path = item->storage_path;

    ASSERT_TRUE(FileUtils::file_exists(storage_path));
    ASSERT_TRUE(item_repo.find_by_id(item_id).has_value());

    update_svc.permanent_delete(item_id);

    ASSERT_TRUE(!FileUtils::file_exists(storage_path));
    ASSERT_TRUE(!item_repo.find_by_id(item_id).has_value());

    cleanup_base();
    TEST_PASS();
}

// === FilesystemTracker Backup/Restore Tests ===

static void test_tracker_backup_restore_replaced_file() {
    TEST_BEGIN("FilesystemTracker: backup and restore replaced file on compensate");
    std::string dir = TEST_BASE + "/tracker_backup_test";
    std::string backup = TEST_BASE + "/tracker_backup_dir";
    FileUtils::create_directories(dir);

    std::string file1 = dir + "/replaced.txt";
    create_test_file(file1, "original content");

    FilesystemTracker tracker(backup);
    std::string dest = dir + "/replaced.txt";
    tracker.track_replaced_file(dest, file1);

    create_test_file(dest, "new content that overwrites");
    ASSERT_TRUE(FileUtils::file_exists(dest));

    tracker.compensate();

    {
        std::ifstream f(dest);
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "original content");
    }

    std::filesystem::remove_all(dir);
    std::filesystem::remove_all(backup);
    TEST_PASS();
}

static void test_tracker_mark_success_cleans_backup() {
    TEST_BEGIN("FilesystemTracker: mark_success cleans backup dir");
    std::string dir = TEST_BASE + "/tracker_success_test";
    std::string backup = TEST_BASE + "/tracker_success_backup";
    FileUtils::create_directories(dir);

    std::string file1 = dir + "/file.txt";
    create_test_file(file1, "data");

    FilesystemTracker tracker(backup);
    tracker.track_copied_file(file1);

    tracker.mark_success();

    ASSERT_TRUE(!std::filesystem::exists(backup));

    std::filesystem::remove_all(dir);
    TEST_PASS();
}

static void test_tracker_compensate_restores_multiple_replaced() {
    TEST_BEGIN("FilesystemTracker: compensate restores multiple replaced files");
    std::string dir = TEST_BASE + "/tracker_multi_replace";
    std::string backup = TEST_BASE + "/tracker_multi_backup";
    FileUtils::create_directories(dir);

    std::string f1 = dir + "/a.txt";
    std::string f2 = dir + "/b.txt";
    std::string f3 = dir + "/c.txt";
    create_test_file(f1, "original_a");
    create_test_file(f2, "original_b");
    create_test_file(f3, "original_c");

    FilesystemTracker tracker(backup);
    tracker.track_replaced_file(f1, f1);
    tracker.track_replaced_file(f2, f2);
    tracker.track_replaced_file(f3, f3);

    create_test_file(f1, "new_a");
    create_test_file(f2, "new_b");
    create_test_file(f3, "new_c");

    tracker.compensate();

    auto read = [](const std::string& path) {
        std::ifstream f(path);
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        return content;
    };

    ASSERT_EQ(read(f1), "original_a");
    ASSERT_EQ(read(f2), "original_b");
    ASSERT_EQ(read(f3), "original_c");

    std::filesystem::remove_all(dir);
    std::filesystem::remove_all(backup);
    TEST_PASS();
}

// === StagingManager State Machine Tests ===

static void test_staging_state_transitions() {
    TEST_BEGIN("StagingManager: state transitions tracked in metadata");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_state", "item123");

    StagingOperation op = staging.detect_abandoned_staging()[0];
    ASSERT_EQ(op.state, "preparing");
    ASSERT_EQ(op.item_id, "item123");

    staging.mark_staged(op_id, "ver456", "abc123");
    op = staging.detect_abandoned_staging()[0];
    ASSERT_EQ(op.state, "staged");
    ASSERT_EQ(op.version_id, "ver456");
    ASSERT_EQ(op.expected_checksum, "abc123");

    staging.mark_finalizing(op_id);
    op = staging.detect_abandoned_staging()[0];
    ASSERT_EQ(op.state, "finalizing");

    staging.mark_committed(op_id);
    auto abandoned = staging.detect_abandoned_staging();
    bool found = false;
    for (const auto& a : abandoned) {
        if (a.operation_id == op_id) found = true;
    }
    ASSERT_TRUE(!found);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_staging_validate_reject_collision() {
    TEST_BEGIN("StagingManager: validate rejects collision with Reject policy");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_validate");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "staged content");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    create_test_file(dest_dir + "/" + FileUtils::file_name(src), "existing content");

    bool valid = staging.validate_staging(op_id, dest_dir, CollisionPolicy::Reject);
    ASSERT_TRUE(!valid);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_staging_validate_skip_identical() {
    TEST_BEGIN("StagingManager: validate accepts identical with SkipIfIdentical");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_skip_identical");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "same content");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    create_test_file(dest_dir + "/" + FileUtils::file_name(src), "same content");

    bool valid = staging.validate_staging(op_id, dest_dir, CollisionPolicy::SkipIfIdentical);
    ASSERT_TRUE(valid);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_staging_validate_reject_different_skip() {
    TEST_BEGIN("StagingManager: validate rejects different content with SkipIfIdentical");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_reject_diff");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "new content");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    create_test_file(dest_dir + "/" + FileUtils::file_name(src), "old content");

    bool valid = staging.validate_staging(op_id, dest_dir, CollisionPolicy::SkipIfIdentical);
    ASSERT_TRUE(!valid);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_staging_finalize_backup_and_replace() {
    TEST_BEGIN("StagingManager: finalize with BackupAndReplace replaces and cleans backup");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_backup_replace");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "new version");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/" + FileUtils::file_name(src);
    create_test_file(dest_file, "old version");

    staging.finalize_staging(op_id, dest_dir, CollisionPolicy::BackupAndReplace);

    std::string content;
    {
        std::ifstream f(dest_file);
        content = std::string((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(content, "new version");

    std::string backup_file = dest_file + ".backup";
    ASSERT_TRUE(!FileUtils::file_exists(backup_file));

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_staging_rollback_cleans_up() {
    TEST_BEGIN("StagingManager: rollback marks rolled_back and cleans");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_rollback");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "content");
    staging.stage_file(op_id, src);

    ASSERT_TRUE(staging.staging_dir_exists(op_id));

    staging.rollback_staging(op_id);

    ASSERT_TRUE(!staging.staging_dir_exists(op_id));

    cleanup_base();
    TEST_PASS();
}

static void test_staging_detect_corrupted() {
    TEST_BEGIN("StagingManager: detect corrupted staging in finalizing state");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_corrupted");

    staging.mark_finalizing(op_id);

    auto corrupted = staging.detect_corrupted_staging();
    ASSERT_EQ(corrupted.size(), 1u);
    ASSERT_EQ(corrupted[0].operation_id, op_id);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Import Atomicity Protocol Tests ===

static void test_import_rollback_restores_replaced_files() {
    TEST_BEGIN("Import rollback: restores replaced files on DB failure");
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

    std::string src = TEST_BASE + "/replace_test.txt";
    create_test_file(src, "original data");

    auto r1 = import_svc.import_single(src, std::nullopt);
    ASSERT_EQ(r1.success_count(), 1);

    std::string item_id = r1.items[0].id;
    auto item = item_repo.find_by_id(item_id);
    std::string storage_path = item->storage_path;

    std::string src2 = TEST_BASE + "/replace_test_v2.txt";
    create_test_file(src2, "v2 content");
    auto r2 = import_svc.import_single(src2, std::nullopt);
    ASSERT_EQ(r2.success_count(), 1);

    ASSERT_TRUE(FileUtils::file_exists(storage_path));

    cleanup_base();
    TEST_PASS();
}

static void test_version_restore_atomicity() {
    TEST_BEGIN("Version restore: FS and DB consistent after restore");
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

    std::string src1 = TEST_BASE + "/atomic_restore.txt";
    create_test_file(src1, "version 1 content");
    auto r1 = import_svc.import_single(src1, std::nullopt);
    std::string item_id = r1.items[0].id;

    std::string src2 = TEST_BASE + "/atomic_restore_v2.txt";
    create_test_file(src2, "version 2 content");
    version_svc.create_version(item_id, src2, "v2");

    auto versions = ver_repo.find_by_item(item_id);
    ASSERT_EQ(versions.size(), 2u);
    std::string v1_id = versions[1].id;

    version_svc.restore(item_id, v1_id);

    auto item = item_repo.find_by_id(item_id);
    ASSERT_TRUE(item.has_value());
    ASSERT_EQ(item->current_version, 1);
    ASSERT_TRUE(FileUtils::file_exists(item->storage_path));

    auto check = integrity.verify_item(item_id);
    ASSERT_EQ(check.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

// === Symlink Protection Tests ===

static void test_symlink_not_traversed() {
    TEST_BEGIN("StorageManager: symlinks skipped during folder store");
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

    std::string src = TEST_BASE + "/symlink_src";
    FileUtils::create_directories(src);
    create_test_file(src + "/real.txt", "real content");

    std::string link_target = TEST_BASE + "/symlink_target.txt";
    create_test_file(link_target, "should not be copied");

    std::error_code ec;
    std::filesystem::create_symlink(link_target, src + "/link.txt", ec);

    if (!ec) {
        auto item_id = "test_symlink_item";
        storage.create_item_dir(item_id);
        auto result_path = storage.store_folder(item_id, src);

        ASSERT_TRUE(FileUtils::file_exists(result_path + "/real.txt"));
        ASSERT_TRUE(!FileUtils::file_exists(result_path + "/link.txt"));
    }

    std::filesystem::remove_all(src);
    std::filesystem::remove_all(TEST_BASE + "/items/test_symlink_item");
    cleanup_base();
    TEST_PASS();
}

// === Consistency Report Enhanced Tests ===

static void test_consistency_corrupted_staging_detected() {
    TEST_BEGIN("ConsistencyReport: corrupted staging detected");
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
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    auto op_id = storage.staging().create_staging_dir("test_corrupted_consistency");
    storage.staging().mark_finalizing(op_id);

    auto report = integrity.check_consistency();
    ASSERT_TRUE(!report.issues.empty());
    ASSERT_TRUE(report.warning_count > 0);

    storage.staging().cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_consistency_after_version_create_failure() {
    TEST_BEGIN("ConsistencyReport: clean after version create failure");
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

    std::string src = TEST_BASE + "/consistency_version.txt";
    create_test_file(src, "content");
    auto r = import_svc.import_single(src, std::nullopt);
    std::string item_id = r.items[0].id;

    ASSERT_THROW(version_svc.create_version(item_id, "/nonexistent.txt"));

    auto report = integrity.check_consistency();
    ASSERT_TRUE(report.is_clean());

    cleanup_base();
    TEST_PASS();
}

static void test_consistency_full_lifecycle_with_staging() {
    TEST_BEGIN("ConsistencyReport: clean after full lifecycle with staging");
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
    UpdateService update_svc(item_repo, act_repo, storage);
    IntegrityService integrity(item_repo, ver_repo, so_repo, act_repo, storage);

    std::string src1 = TEST_BASE + "/full_staging.txt";
    create_test_file(src1, "content v1");
    auto r1 = import_svc.import_single(src1, std::nullopt);
    std::string item_id = r1.items[0].id;

    std::string src2 = TEST_BASE + "/full_staging_v2.txt";
    create_test_file(src2, "content v2");
    version_svc.create_version(item_id, src2, "v2");

    update_svc.move_to_trash(item_id);
    update_svc.restore_from_trash(item_id);

    auto report = integrity.check_consistency();
    ASSERT_TRUE(report.is_clean());

    auto verification = integrity.verify_item(item_id);
    ASSERT_EQ(verification.items[0].state, IntegrityState::Valid);

    cleanup_base();
    TEST_PASS();
}

// === Path Traversal Protection Tests ===

static void test_path_traversal_rejected_in_staging() {
    TEST_BEGIN("Path traversal: .. rejected in stage_file_in_dir");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_traversal");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "test content");

    bool threw = false;
    try {
        staging.stage_file_in_dir(op_id, src, "../../etc/passwd");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_path_traversal_rejected_in_storage() {
    TEST_BEGIN("Path traversal: .. rejected in store_file_in_dir");
    setup_base();

    DatabaseManager db(":memory:");
    db.initialize();
    StorageManager storage(TEST_BASE, TEST_ITEMS);
    storage.create_item_dir("test_item");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "test content");

    bool threw = false;
    try {
        storage.store_file_in_dir("test_item", src, "../../evil.txt");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    cleanup_base();
    TEST_PASS();
}

static void test_valid_relative_path_accepted() {
    TEST_BEGIN("Valid relative path: sub/dir/file.txt accepted");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_valid_path");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "test content");

    std::string result = staging.stage_file_in_dir(op_id, src, "sub/dir/file.txt");
    ASSERT_TRUE(FileUtils::file_exists(result));

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Rollback Restores Backups Tests ===

static void test_rollback_restores_backups() {
    TEST_BEGIN("Rollback: restores backed up files");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_rollback_backup");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "new content");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/" + FileUtils::file_name(src);
    create_test_file(dest_file, "original content");

    staging.finalize_staging(op_id, dest_dir, CollisionPolicy::BackupAndReplace);

    std::string verify_content;
    {
        std::ifstream f(dest_file);
        verify_content = std::string((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(verify_content, "new content");

    auto op_id2 = staging.create_staging_dir("test_rollback_backup2");
    std::string src2 = TEST_BASE + "/src2.txt";
    create_test_file(src2, "another new");
    staging.stage_file(op_id2, src2);
    staging.finalize_staging(op_id2, dest_dir, CollisionPolicy::BackupAndReplace);

    staging.rollback_staging(op_id2);

    cleanup_base();
    TEST_PASS();
}

// === Post-Copy Verification Tests ===

static void test_finalize_verifies_copy() {
    TEST_BEGIN("Finalize: verifies file size and checksum after copy");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_verify");

    std::string src = TEST_BASE + "/src.txt";
    std::string content(8192, 'A');
    create_test_file(src, content);
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);

    staging.finalize_staging(op_id, dest_dir);

    std::string dest_file = dest_dir + "/" + FileUtils::file_name(src);
    ASSERT_TRUE(FileUtils::file_exists(dest_file));
    ASSERT_EQ(FileUtils::file_size(dest_file), 8192u);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_backup_cleanup_after_success() {
    TEST_BEGIN("Finalize: backup files cleaned after successful finalize");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_backup_cleanup");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "new");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/" + FileUtils::file_name(src);
    create_test_file(dest_file, "old");

    staging.finalize_staging(op_id, dest_dir, CollisionPolicy::BackupAndReplace);

    std::string backup_file = dest_file + ".backup";
    ASSERT_TRUE(!FileUtils::file_exists(backup_file));

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Recovery After Restart Test (#8) ===

static void test_recovery_after_restart_new_instance() {
    TEST_BEGIN("Recovery: new StagingManager instance restores from journal");
    setup_base();

    std::string op_id;
    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/target.txt";
    create_test_file(dest_file, "original");

    {
        StagingManager staging(TEST_BASE);
        op_id = staging.create_staging_dir("test_recovery");

        std::string src = TEST_BASE + "/src.txt";
        create_test_file(src, "replaced");
        staging.stage_file(op_id, src);

        std::string backup_path = staging.get_staging_path(op_id) + "/.meta/backups/target.txt";
        staging.backup_file_strict(dest_file, backup_path);

        std::ofstream f(dest_file, std::ios::trunc);
        f << "modified";
        f.close();

        std::vector<BackupEntry> entries;
        BackupEntry be;
        be.destination = dest_file;
        be.backup_path = backup_path;
        be.state = "created";
        entries.push_back(be);
        staging.write_backup_journal(op_id, entries);
    }

    StagingManager staging2(TEST_BASE);
    auto journal = staging2.read_backup_journal(op_id);
    ASSERT_EQ(journal.size(), 1u);
    ASSERT_EQ(journal[0].destination, dest_file);
    ASSERT_EQ(journal[0].state, "created");

    staging2.restore_backups(op_id);

    {
        std::ifstream f(dest_file);
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "original");
    }

    staging2.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_recovery_journal_persists_across_instances() {
    TEST_BEGIN("Recovery: journal persists across StagingManager instances");
    setup_base();

    std::string op_id;
    {
        StagingManager staging(TEST_BASE);
        op_id = staging.create_staging_dir("test_journal_persist");

        std::string dest_dir = TEST_BASE + "/dest";
        FileUtils::create_directories(dest_dir);
        std::string dest_file = dest_dir + "/file.txt";
        create_test_file(dest_file, "v1");

        std::string src = TEST_BASE + "/src.txt";
        create_test_file(src, "v2");
        staging.stage_file(op_id, src);

        std::string backup_path = staging.get_staging_path(op_id) + "/.meta/backups/file.txt";
        staging.backup_file_strict(dest_file, backup_path);

        std::vector<BackupEntry> entries;
        BackupEntry be;
        be.destination = dest_file;
        be.backup_path = backup_path;
        be.state = "created";
        entries.push_back(be);
        staging.write_backup_journal(op_id, entries);
    }

    StagingManager staging2(TEST_BASE);
    auto journal = staging2.read_backup_journal(op_id);
    ASSERT_EQ(journal.size(), 1u);
    ASSERT_EQ(journal[0].state, "created");

    staging2.restore_backups(op_id);

    staging2.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_recovery_rollback_after_restart() {
    TEST_BEGIN("Recovery: rollback works after restart from new instance");
    setup_base();

    std::string op_id;
    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/target.txt";
    create_test_file(dest_file, "original");

    {
        StagingManager staging(TEST_BASE);
        op_id = staging.create_staging_dir("test_recovery_rollback");

        std::string src = TEST_BASE + "/src.txt";
        create_test_file(src, "staged");
        staging.stage_file(op_id, src);

        std::string backup_path = staging.get_staging_path(op_id) + "/.meta/backups/target.txt";
        staging.backup_file_strict(dest_file, backup_path);

        std::ofstream f(dest_file, std::ios::trunc);
        f << "modified";
        f.close();

        staging.mark_finalizing(op_id);

        std::vector<BackupEntry> entries;
        BackupEntry be;
        be.destination = dest_file;
        be.backup_path = backup_path;
        be.state = "created";
        entries.push_back(be);
        staging.write_backup_journal(op_id, entries);
    }

    StagingManager staging2(TEST_BASE);
    staging2.rollback_staging(op_id);

    {
        std::ifstream f(dest_file);
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "original");
    }

    ASSERT_TRUE(!staging2.staging_dir_exists(op_id));

    cleanup_base();
    TEST_PASS();
}

// === Backup Safety Tests (#4) ===

static void test_backup_failure_destination_unchanged() {
    TEST_BEGIN("Backup failure: destination unchanged, operation aborted");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_backup_fail");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "new content");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/" + FileUtils::file_name(src);
    create_test_file(dest_file, "original content");

    std::string file_as_dir = TEST_BASE + "/file_not_dir";
    create_test_file(file_as_dir, "I am a file, not a dir");
    std::string bad_backup = file_as_dir + "/backup.txt";

    bool threw = false;
    try {
        staging.backup_file_strict(dest_file, bad_backup);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    {
        std::ifstream f(dest_file);
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "original content");
    }

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_backup_verification_fails_destination_unchanged() {
    TEST_BEGIN("Backup verification fails: destination unchanged");
    setup_base();

    StagingManager staging(TEST_BASE);

    std::string existing = TEST_BASE + "/existing.txt";
    create_test_file(existing, "data");

    std::string nonexistent = TEST_BASE + "/nonexistent_dir/file.txt";

    bool threw = false;
    try {
        staging.backup_file_strict(nonexistent, existing);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    ASSERT_TRUE(FileUtils::file_exists(existing));
    {
        std::ifstream f(existing);
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "data");
    }

    cleanup_base();
    TEST_PASS();
}

// === SkipIfIdentical Exact Semantics (#5) ===

static void test_skip_identical_collision_accepted() {
    TEST_BEGIN("SkipIfIdentical: identical content, no overwrite");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_skip_identical_ok");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "same content");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    create_test_file(dest_dir + "/" + FileUtils::file_name(src), "same content");

    bool valid = staging.validate_staging(op_id, dest_dir, CollisionPolicy::SkipIfIdentical);
    ASSERT_TRUE(valid);

    staging.finalize_staging(op_id, dest_dir, CollisionPolicy::SkipIfIdentical);

    std::string content;
    {
        std::ifstream f(dest_dir + "/" + FileUtils::file_name(src));
        content = std::string((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(content, "same content");

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_skip_identical_different_rejected_validate() {
    TEST_BEGIN("SkipIfIdentical: different content rejected at validate");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_skip_diff_validate");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "new content");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    create_test_file(dest_dir + "/" + FileUtils::file_name(src), "old content");

    bool valid = staging.validate_staging(op_id, dest_dir, CollisionPolicy::SkipIfIdentical);
    ASSERT_TRUE(!valid);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_skip_identical_different_rejected_finalize() {
    TEST_BEGIN("SkipIfIdentical: different content rejected at finalize");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_skip_diff_finalize");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "new content");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    create_test_file(dest_dir + "/" + FileUtils::file_name(src), "old content");

    bool threw = false;
    try {
        staging.finalize_staging(op_id, dest_dir, CollisionPolicy::SkipIfIdentical);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    std::string content;
    {
        std::ifstream f(dest_dir + "/" + FileUtils::file_name(src));
        content = std::string((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(content, "old content");

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Finalize Failure Tests (#6, #7) ===

static void test_finalize_verify_failure_restores_backup() {
    TEST_BEGIN("Finalize: verify failure restores backup");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_verify_fail_real");

    std::string src = TEST_BASE + "/src.txt";
    std::string staged_content(8192, 'A');
    create_test_file(src, staged_content);
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/" + FileUtils::file_name(src);
    create_test_file(dest_file, "original");

    staging.finalize_staging(op_id, dest_dir, CollisionPolicy::BackupAndReplace);

    std::string content;
    {
        std::ifstream f(dest_file);
        content = std::string((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(content, staged_content);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_verify_finalized_detects_mismatch() {
    TEST_BEGIN("Verify: verify_finalized detects size mismatch");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_verify_mismatch");

    std::string src = TEST_BASE + "/src.txt";
    std::string staged_content(8192, 'A');
    create_test_file(src, staged_content);
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/" + FileUtils::file_name(src);
    create_test_file(dest_file, "short");

    bool threw = false;
    try {
        staging.verify_finalized(op_id, dest_dir);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_finalize_copy_failure_restores_backup() {
    TEST_BEGIN("Finalize: mid-finalize failure preserves backup journal");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_copy_fail");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "new content");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/" + FileUtils::file_name(src);
    create_test_file(dest_file, "original content");

    bool threw = false;
    try {
        staging.finalize_staging(op_id, dest_dir, CollisionPolicy::Reject);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    auto journal = staging.read_backup_journal(op_id);
    ASSERT_TRUE(journal.empty());

    std::string content;
    {
        std::ifstream f(dest_file);
        content = std::string((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(content, "original content");

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Path Containment Tests (#10) ===

static void test_sibling_prefix_rejected() {
    TEST_BEGIN("Path containment: sibling directory not contained");
    setup_base();

    std::string base = TEST_BASE + "/archive_base";
    std::string sibling = TEST_BASE + "/archive_base_other";
    std::string file = sibling + "/file.txt";

    FileUtils::create_directories(base);
    FileUtils::create_directories(sibling);
    create_test_file(file, "data");

    bool within = FileUtils::is_path_within(file, base);
    ASSERT_TRUE(!within);

    cleanup_base();
    TEST_PASS();
}

static void test_absolute_path_rejected() {
    TEST_BEGIN("Path containment: absolute path rejected in relative context");
    setup_base();

    bool threw = false;
    try {
        FileUtils::sanitize_relative_path("/etc/passwd");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    cleanup_base();
    TEST_PASS();
}

static void test_nested_valid_path_accepted() {
    TEST_BEGIN("Path containment: nested valid path accepted");
    setup_base();

    std::string base = TEST_BASE + "/archive_base";
    std::string nested = base + "/sub/file.txt";

    FileUtils::create_directories(base + "/sub");
    create_test_file(nested, "data");

    bool within = FileUtils::is_path_within(nested, base);
    ASSERT_TRUE(within);

    cleanup_base();
    TEST_PASS();
}

// === Stage Folder Security Tests (#11) ===

static void test_stage_folder_traversal_rejected() {
    TEST_BEGIN("stage_folder: symlink skipped and files within bounds staged");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_folder_traversal");

    std::string src = TEST_BASE + "/src_folder";
    FileUtils::create_directories(src);
    create_test_file(src + "/normal.txt", "safe content");
    create_test_file(src + "/sub/nested.txt", "nested content");

    std::string dest = staging.stage_folder(op_id, src);
    ASSERT_TRUE(FileUtils::file_exists(dest + "/normal.txt"));
    ASSERT_TRUE(FileUtils::file_exists(dest + "/sub/nested.txt"));

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_stage_folder_symlink_skipped() {
    TEST_BEGIN("stage_folder: symlinks not copied");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_folder_symlink");

    std::string src = TEST_BASE + "/src_folder_symlink";
    FileUtils::create_directories(src);
    create_test_file(src + "/real.txt", "real");

    std::string target = TEST_BASE + "/symlink_target.txt";
    create_test_file(target, "target content");

    std::error_code ec;
    std::filesystem::create_symlink(target, src + "/link.txt", ec);

    std::string dest = staging.stage_folder(op_id, src);
    ASSERT_TRUE(FileUtils::file_exists(dest + "/real.txt"));

    if (!ec) {
        ASSERT_TRUE(!FileUtils::file_exists(dest + "/link.txt"));
    }

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Cleanup State Tests (#9) ===

static void test_cleanup_rolled_back_staging() {
    TEST_BEGIN("Cleanup: RolledBack staging listed in abandoned for cleanup");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_cleanup_rolled_back");

    staging.mark_rolled_back(op_id);
    ASSERT_TRUE(staging.staging_dir_exists(op_id));

    auto abandoned = staging.detect_abandoned_staging();
    bool found = false;
    for (const auto& a : abandoned) {
        if (a.operation_id == op_id) found = true;
    }
    ASSERT_TRUE(found);

    staging.cleanup_abandoned();
    ASSERT_TRUE(!staging.staging_dir_exists(op_id));

    cleanup_base();
    TEST_PASS();
}

static void test_cleanup_committed_not_in_abandoned() {
    TEST_BEGIN("Cleanup: Committed not listed as abandoned");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_cleanup_committed");

    staging.mark_committed(op_id);

    auto abandoned = staging.detect_abandoned_staging();
    bool found = false;
    for (const auto& a : abandoned) {
        if (a.operation_id == op_id) found = true;
    }
    ASSERT_TRUE(!found);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_cleanup_preparing_in_abandoned() {
    TEST_BEGIN("Cleanup: Preparing state listed as abandoned");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_cleanup_preparing");

    auto abandoned = staging.detect_abandoned_staging();
    bool found = false;
    for (const auto& a : abandoned) {
        if (a.operation_id == op_id) found = true;
    }
    ASSERT_TRUE(found);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_cleanup_abandoned_only_rolled_back() {
    TEST_BEGIN("Cleanup: cleanup_abandoned only removes RolledBack");
    setup_base();

    StagingManager staging(TEST_BASE);

    auto op1 = staging.create_staging_dir("test_clean_1");
    staging.mark_rolled_back(op1);

    auto op2 = staging.create_staging_dir("test_clean_2");
    staging.mark_finalizing(op2);

    staging.cleanup_abandoned();

    ASSERT_TRUE(!staging.staging_dir_exists(op1));
    ASSERT_TRUE(staging.staging_dir_exists(op2));

    staging.cleanup_staging(op2);
    cleanup_base();
    TEST_PASS();
}

static void test_cleanup_staged_in_abandoned() {
    TEST_BEGIN("Cleanup: Staged state survives cleanup_abandoned");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_cleanup_staged");
    staging.mark_staged(op_id);

    staging.cleanup_abandoned();
    ASSERT_TRUE(staging.staging_dir_exists(op_id));

    auto abandoned = staging.detect_abandoned_staging();
    bool found = false;
    for (const auto& a : abandoned) {
        if (a.operation_id == op_id) found = true;
    }
    ASSERT_TRUE(found);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_cleanup_finalizing_in_abandoned() {
    TEST_BEGIN("Cleanup: Finalizing state survives cleanup_abandoned");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_cleanup_finalizing");
    staging.mark_finalizing(op_id);

    staging.cleanup_abandoned();
    ASSERT_TRUE(staging.staging_dir_exists(op_id));

    auto abandoned = staging.detect_abandoned_staging();
    bool found = false;
    for (const auto& a : abandoned) {
        if (a.operation_id == op_id) found = true;
    }
    ASSERT_TRUE(found);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_cleanup_abandoned_state_survives() {
    TEST_BEGIN("Cleanup: Abandoned state survives cleanup_abandoned");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_cleanup_abandoned_state");
    staging.mark_abandoned(op_id);

    staging.cleanup_abandoned();
    ASSERT_TRUE(staging.staging_dir_exists(op_id));

    auto abandoned = staging.detect_abandoned_staging();
    bool found = false;
    for (const auto& a : abandoned) {
        if (a.operation_id == op_id) found = true;
    }
    ASSERT_TRUE(found);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_cleanup_corrupted_survives() {
    TEST_BEGIN("Cleanup: Corrupted state survives cleanup_abandoned");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_cleanup_corrupted");
    staging.mark_finalizing(op_id);

    auto corrupted = staging.detect_corrupted_staging();
    ASSERT_TRUE(!corrupted.empty());

    staging.cleanup_abandoned();
    ASSERT_TRUE(staging.staging_dir_exists(op_id));

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_cleanup_committed_staging_removed() {
    TEST_BEGIN("Cleanup: cleanup_committed removes committed staging dirs");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_cleanup_committed_rm");
    staging.mark_committed(op_id);
    ASSERT_TRUE(staging.staging_dir_exists(op_id));

    staging.cleanup_committed();
    ASSERT_TRUE(!staging.staging_dir_exists(op_id));

    cleanup_base();
    TEST_PASS();
}

// === BackupAndReplace Collision Test ===

static void test_backup_and_replace_collision_accepted() {
    TEST_BEGIN("BackupAndReplace: collision accepted, original backed up");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_backup_replace_ok");

    std::string src = TEST_BASE + "/src.txt";
    create_test_file(src, "new");
    staging.stage_file(op_id, src);

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);
    std::string dest_file = dest_dir + "/" + FileUtils::file_name(src);
    create_test_file(dest_file, "old");

    staging.finalize_staging(op_id, dest_dir, CollisionPolicy::BackupAndReplace);

    std::string content;
    {
        std::ifstream f(dest_file);
        content = std::string((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(content, "new");

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Backup Journal Roundtrip Tests ===

static void test_backup_journal_roundtrip() {
    TEST_BEGIN("Backup journal roundtrip: all fields preserved");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_roundtrip");

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);

    std::string dest_a = dest_dir + "/file_a.txt";
    std::string dest_b = dest_dir + "/sub/file_b.dat";
    std::filesystem::create_directories(dest_dir + "/sub");
    create_test_file(dest_a, "original_a");
    create_test_file(dest_b, "original_b");

    std::string backup_a = staging.get_staging_path(op_id) + "/.meta/backups/file_a.txt";
    std::string backup_b = staging.get_staging_path(op_id) + "/.meta/backups/sub_file_b.dat";

    staging.backup_file_strict(dest_a, backup_a);
    staging.backup_file_strict(dest_b, backup_b);

    std::vector<BackupEntry> entries;
    entries.push_back({dest_a, backup_a, "created"});
    entries.push_back({dest_b, backup_b, "created"});
    staging.write_backup_journal(op_id, entries);

    std::vector<BackupEntry> read_back = staging.read_backup_journal(op_id);
    ASSERT_EQ(read_back.size(), 2u);
    ASSERT_EQ(read_back[0].destination, dest_a);
    ASSERT_EQ(read_back[0].backup_path, backup_a);
    ASSERT_EQ(read_back[0].state, "created");
    ASSERT_EQ(read_back[1].destination, dest_b);
    ASSERT_EQ(read_back[1].backup_path, backup_b);
    ASSERT_EQ(read_back[1].state, "created");

    std::ofstream f_a(dest_a, std::ios::trunc);
    f_a << "modified_a";
    f_a.close();
    std::ofstream f_b(dest_b, std::ios::trunc);
    f_b << "modified_b";
    f_b.close();

    staging.restore_backups(op_id);

    std::string content_a;
    {
        std::ifstream f(dest_a);
        content_a = std::string((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(content_a, "original_a");

    std::string content_b;
    {
        std::ifstream f(dest_b);
        content_b = std::string((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(content_b, "original_b");

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_backup_journal_roundtrip_special_chars() {
    TEST_BEGIN("Backup journal roundtrip: paths with spaces and special chars");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_roundtrip_special");

    std::string dest_dir = TEST_BASE + "/dest";
    FileUtils::create_directories(dest_dir);

    std::string dest_file = dest_dir + "/my file (v2) - copy.txt";
    std::string backup_file = staging.get_staging_path(op_id) + "/.meta/backups/my file (v2) - copy.txt";
    create_test_file(dest_file, "original special");

    staging.backup_file_strict(dest_file, backup_file);

    std::vector<BackupEntry> entries;
    entries.push_back({dest_file, backup_file, "created"});
    staging.write_backup_journal(op_id, entries);

    std::vector<BackupEntry> read_back = staging.read_backup_journal(op_id);
    ASSERT_EQ(read_back.size(), 1u);
    ASSERT_EQ(read_back[0].destination, dest_file);
    ASSERT_EQ(read_back[0].backup_path, backup_file);
    ASSERT_EQ(read_back[0].state, "created");

    std::ofstream f(dest_file, std::ios::trunc);
    f << "modified special";
    f.close();

    staging.restore_backups(op_id);

    std::string content;
    {
        std::ifstream f(dest_file);
        content = std::string((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(content, "original special");

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Journal Escape Tests ===

static void test_journal_escape_quotes_and_backslashes() {
    TEST_BEGIN("Journal: quotes and backslashes roundtrip correctly");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_escape_qb");

    std::string dest_with_quote = TEST_BASE + "/dest/file \"final\".txt";
    std::string backup_with_bs = TEST_BASE + "/dest/sub\\file.txt";
    FileUtils::create_directories(TEST_BASE + "/dest/sub");

    std::vector<BackupEntry> entries;
    entries.push_back({dest_with_quote, backup_with_bs, "created"});
    staging.write_backup_journal(op_id, entries);

    std::vector<BackupEntry> read_back = staging.read_backup_journal(op_id);
    ASSERT_EQ(read_back.size(), 1u);
    ASSERT_EQ(read_back[0].destination, dest_with_quote);
    ASSERT_EQ(read_back[0].backup_path, backup_with_bs);
    ASSERT_EQ(read_back[0].state, "created");

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_escape_braces_in_string() {
    TEST_BEGIN("Journal: braces inside string values roundtrip");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_escape_braces");

    std::string dest = TEST_BASE + "/folder/{test}/file.txt";
    std::string backup = TEST_BASE + "/backup/{test}/file.txt";
    FileUtils::create_directories(TEST_BASE + "/folder/{test}");
    FileUtils::create_directories(TEST_BASE + "/backup/{test}");

    std::vector<BackupEntry> entries;
    entries.push_back({dest, backup, "created"});
    staging.write_backup_journal(op_id, entries);

    std::vector<BackupEntry> read_back = staging.read_backup_journal(op_id);
    ASSERT_EQ(read_back.size(), 1u);
    ASSERT_EQ(read_back[0].destination, dest);
    ASSERT_EQ(read_back[0].backup_path, backup);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_escape_windows_paths() {
    TEST_BEGIN("Journal: Windows-style backslash paths roundtrip");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_escape_windows");

    std::string dest = "C:\\Users\\Test\\file.txt";
    std::string backup = "C:\\Archive\\.meta\\backups\\file.txt";

    std::vector<BackupEntry> entries;
    entries.push_back({dest, backup, "created"});
    staging.write_backup_journal(op_id, entries);

    std::vector<BackupEntry> read_back = staging.read_backup_journal(op_id);
    ASSERT_EQ(read_back.size(), 1u);
    ASSERT_EQ(read_back[0].destination, dest);
    ASSERT_EQ(read_back[0].backup_path, backup);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_escape_newline_and_tab() {
    TEST_BEGIN("Journal: newline and tab in values roundtrip");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_escape_nl");

    std::string dest = "file\nwith\nnewlines.txt";
    std::string backup = "file\twith\ttabs.txt";

    std::vector<BackupEntry> entries;
    entries.push_back({dest, backup, "created"});
    staging.write_backup_journal(op_id, entries);

    std::vector<BackupEntry> read_back = staging.read_backup_journal(op_id);
    ASSERT_EQ(read_back.size(), 1u);
    ASSERT_EQ(read_back[0].destination, dest);
    ASSERT_EQ(read_back[0].backup_path, backup);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_multiple_entries_append() {
    TEST_BEGIN("Journal: multiple entries write and append correctly");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_multi_append");

    std::vector<BackupEntry> entries;
    for (int i = 0; i < 5; i++) {
        std::string dest = TEST_BASE + "/dest/file_" + std::to_string(i) + ".txt";
        std::string backup = staging.get_staging_path(op_id) + "/.meta/backups/file_" + std::to_string(i) + ".txt";
        entries.push_back({dest, backup, "created"});
    }
    staging.write_backup_journal(op_id, entries);

    std::string extra_dest = TEST_BASE + "/dest/extra.txt";
    std::string extra_backup = staging.get_staging_path(op_id) + "/.meta/backups/extra.txt";
    entries.push_back({extra_dest, extra_backup, "created"});
    staging.write_backup_journal(op_id, entries);

    std::vector<BackupEntry> read_back = staging.read_backup_journal(op_id);
    ASSERT_EQ(read_back.size(), 6u);
    ASSERT_EQ(read_back[5].destination, extra_dest);
    ASSERT_EQ(read_back[5].backup_path, extra_backup);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

// === Corrupt Journal Tests ===

static void test_journal_corrupt_truncated() {
    TEST_BEGIN("Journal: truncated JSON throws parse error");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_corrupt_trunc");

    std::string journal = staging.get_staging_path(op_id) + "/.meta/backups.json";
    {
        std::ofstream f(journal);
        f << "[\n  {\n    \"destination\": \"foo.txt\",";
    }

    bool threw = false;
    try {
        staging.read_backup_journal(op_id);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_corrupt_missing_field() {
    TEST_BEGIN("Journal: missing destination field throws");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_corrupt_nodest");

    std::string journal = staging.get_staging_path(op_id) + "/.meta/backups.json";
    {
        std::ofstream f(journal);
        f << "[\n  {\n    \"backup_path\": \"b.txt\",\n    \"state\": \"created\"\n  }\n]\n";
    }

    bool threw = false;
    try {
        staging.read_backup_journal(op_id);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_corrupt_not_json() {
    TEST_BEGIN("Journal: non-JSON content throws");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_corrupt_notjson");

    std::string journal = staging.get_staging_path(op_id) + "/.meta/backups.json";
    {
        std::ofstream f(journal);
        f << "this is not json at all";
    }

    bool threw = false;
    try {
        staging.read_backup_journal(op_id);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_empty_array() {
    TEST_BEGIN("Journal: empty array returns empty vector");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_empty_journal");

    std::string journal = staging.get_staging_path(op_id) + "/.meta/backups.json";
    {
        std::ofstream f(journal);
        f << "[]\n";
    }

    auto entries = staging.read_backup_journal(op_id);
    ASSERT_EQ(entries.size(), 0u);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_no_file_returns_empty() {
    TEST_BEGIN("Journal: missing file returns empty vector");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_no_journal_file");

    auto entries = staging.read_backup_journal(op_id);
    ASSERT_EQ(entries.size(), 0u);

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_atomic_write_no_tmp_left() {
    TEST_BEGIN("Journal: atomic write leaves no .tmp file");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_atomic_write");

    std::string dest = TEST_BASE + "/dest.txt";
    std::string backup = staging.get_staging_path(op_id) + "/.meta/backups/dest.txt";

    std::vector<BackupEntry> entries;
    entries.push_back({dest, backup, "created"});
    staging.write_backup_journal(op_id, entries);

    std::string tmp_file = staging.get_staging_path(op_id) + "/.meta/backups.json.tmp";
    ASSERT_TRUE(!std::filesystem::exists(tmp_file));

    std::string journal = staging.get_staging_path(op_id) + "/.meta/backups.json";
    ASSERT_TRUE(std::filesystem::exists(journal));

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

static void test_journal_roundtrip_escape_full() {
    TEST_BEGIN("Journal: full roundtrip with all escape characters");
    setup_base();

    StagingManager staging(TEST_BASE);
    auto op_id = staging.create_staging_dir("test_escape_full");

    std::string dest = "path\\with\\\"quotes\" and\nnewlines\ttabs";
    std::string backup = "backup/{brackets}/[array] (parens).txt";

    std::vector<BackupEntry> entries;
    entries.push_back({dest, backup, "created"});
    entries.push_back({backup, dest, "restored"});
    staging.write_backup_journal(op_id, entries);

    std::vector<BackupEntry> read_back = staging.read_backup_journal(op_id);
    ASSERT_EQ(read_back.size(), 2u);
    ASSERT_EQ(read_back[0].destination, dest);
    ASSERT_EQ(read_back[0].backup_path, backup);
    ASSERT_EQ(read_back[0].state, "created");
    ASSERT_EQ(read_back[1].destination, backup);
    ASSERT_EQ(read_back[1].backup_path, dest);
    ASSERT_EQ(read_back[1].state, "restored");

    staging.cleanup_staging(op_id);
    cleanup_base();
    TEST_PASS();
}

void run_atomic_tests() {
    std::cout << "=== Atomic Operations Tests ===" << std::endl;

    test_transaction_commit();
    test_transaction_rollback();
    test_transaction_explicit_rollback();
    test_tracker_compensate();
    test_tracker_dirs_and_files();
    test_tracker_backup_restore_replaced_file();
    test_tracker_mark_success_cleans_backup();
    test_tracker_compensate_restores_multiple_replaced();
    test_staging_create_and_detect();
    test_staging_finalize();
    test_staging_state_transitions();
    test_staging_validate_reject_collision();
    test_staging_validate_skip_identical();
    test_staging_validate_reject_different_skip();
    test_staging_finalize_backup_and_replace();
    test_staging_rollback_cleans_up();
    test_staging_detect_corrupted();
    test_import_rollback_no_ghost_item();
    test_import_rollback_no_stored_object();
    test_import_rollback_restores_replaced_files();
    test_version_create_rollback();
    test_version_create_rollback_no_orphan();
    test_version_v1_v2_integrity();
    test_restore_v1_then_v2();
    test_restore_rollback();
    test_version_restore_atomicity();
    test_consistency_clean();
    test_consistency_item_without_version();
    test_consistency_stored_object_without_file();
    test_consistency_after_full_lifecycle();
    test_import_same_name_different_content();
    test_import_empty_file();
    test_folder_import_deep_nesting();
    test_version_unique_constraint();
    test_stored_object_unique_version_constraint();
    test_multiple_sequential_imports();
    test_permanent_delete_consistency();
    test_symlink_not_traversed();
    test_consistency_corrupted_staging_detected();
    test_consistency_after_version_create_failure();
    test_consistency_full_lifecycle_with_staging();
    test_path_traversal_rejected_in_staging();
    test_path_traversal_rejected_in_storage();
    test_valid_relative_path_accepted();
    test_rollback_restores_backups();
    test_finalize_verifies_copy();
    test_backup_cleanup_after_success();
    test_recovery_after_restart_new_instance();
    test_recovery_journal_persists_across_instances();
    test_recovery_rollback_after_restart();
    test_backup_failure_destination_unchanged();
    test_backup_verification_fails_destination_unchanged();
    test_skip_identical_collision_accepted();
    test_skip_identical_different_rejected_validate();
    test_skip_identical_different_rejected_finalize();
    test_finalize_verify_failure_restores_backup();
    test_verify_finalized_detects_mismatch();
    test_finalize_copy_failure_restores_backup();
    test_sibling_prefix_rejected();
    test_absolute_path_rejected();
    test_nested_valid_path_accepted();
    test_stage_folder_traversal_rejected();
    test_stage_folder_symlink_skipped();
    test_cleanup_rolled_back_staging();
    test_cleanup_committed_not_in_abandoned();
    test_cleanup_preparing_in_abandoned();
    test_cleanup_abandoned_only_rolled_back();
    test_cleanup_staged_in_abandoned();
    test_cleanup_finalizing_in_abandoned();
    test_cleanup_abandoned_state_survives();
    test_cleanup_corrupted_survives();
    test_cleanup_committed_staging_removed();
    test_backup_and_replace_collision_accepted();
    test_backup_journal_roundtrip();
    test_backup_journal_roundtrip_special_chars();
    test_journal_escape_quotes_and_backslashes();
    test_journal_escape_braces_in_string();
    test_journal_escape_windows_paths();
    test_journal_escape_newline_and_tab();
    test_journal_multiple_entries_append();
    test_journal_corrupt_truncated();
    test_journal_corrupt_missing_field();
    test_journal_corrupt_not_json();
    test_journal_empty_array();
    test_journal_no_file_returns_empty();
    test_journal_atomic_write_no_tmp_left();
    test_journal_roundtrip_escape_full();
}
