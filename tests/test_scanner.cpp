#include "test_helpers.h"

#include "storage/DatabaseManager.h"
#include "storage/ScanRepository.h"
#include "storage/ScanItemRepository.h"
#include "services/Scanner.h"

#include <fstream>
#include <filesystem>
#include <cstdio>

using namespace archive::storage;
using namespace archive::services;
using namespace archive::core;

static const std::string TEST_DIR = "scanner_test_dir";

static void cleanup() {
    std::error_code ec;
    std::filesystem::remove_all(TEST_DIR, ec);
}

static void create_test_file(const std::string& path, const std::string& content = "test content") {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

static void setup_directory() {
    cleanup();
    create_test_file(TEST_DIR + "/hello.txt", "Hello World");
    create_test_file(TEST_DIR + "/main.cpp", "#include <iostream>\nint main() {}");
    create_test_file(TEST_DIR + "/script.py", "print('hello')");
    create_test_file(TEST_DIR + "/photo.jpg", std::string("\xFF\xD8\xFF\xE0", 4));
    create_test_file(TEST_DIR + "/docs/readme.md", "# README");
    create_test_file(TEST_DIR + "/config.json", "{\"key\": \"value\"}");
}

// ============================================================
// Scanner tests
// ============================================================

static void test_scan_directory_finds_all_files() {
    TEST_BEGIN("scan_directory finds all files");
    setup_directory();
    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    Scanner scanner(db, scan_repo, item_repo);

    auto scan = scanner.scan_directory(TEST_DIR);
    ASSERT_EQ(scan.status, ScanStatus::Completed);
    ASSERT_TRUE(scan.file_count >= 6);
    cleanup();
    TEST_PASS();
}

static void test_scan_directory_skips_git() {
    TEST_BEGIN("scan_directory skips .git");
    setup_directory();
    create_test_file(TEST_DIR + "/.git/config", "git config");
    create_test_file(TEST_DIR + "/.git/objects/abc", "obj");

    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    Scanner scanner(db, scan_repo, item_repo);

    auto scan = scanner.scan_directory(TEST_DIR);
    auto items = scanner.get_items(scan.id);

    for (const auto& item : items) {
        ASSERT_TRUE(item.path.find(".git") == std::string::npos);
    }
    cleanup();
    TEST_PASS();
}

static void test_scan_directory_skips_node_modules() {
    TEST_BEGIN("scan_directory skips node_modules");
    setup_directory();
    create_test_file(TEST_DIR + "/node_modules/package/index.js", "module.exports = {}");

    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    Scanner scanner(db, scan_repo, item_repo);

    auto scan = scanner.scan_directory(TEST_DIR);
    auto items = scanner.get_items(scan.id);

    for (const auto& item : items) {
        ASSERT_TRUE(item.path.find("node_modules") == std::string::npos);
    }
    cleanup();
    TEST_PASS();
}

static void test_scan_directory_skips_pycache() {
    TEST_BEGIN("scan_directory skips __pycache__");
    setup_directory();
    create_test_file(TEST_DIR + "/__pycache__/module.cpython-310.pyc", "bytecode");

    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    Scanner scanner(db, scan_repo, item_repo);

    auto scan = scanner.scan_directory(TEST_DIR);
    auto items = scanner.get_items(scan.id);

    for (const auto& item : items) {
        ASSERT_TRUE(item.path.find("__pycache__") == std::string::npos);
    }
    cleanup();
    TEST_PASS();
}

static void test_scan_directory_skips_symlinks() {
    TEST_BEGIN("scan_directory skips symlinks");
    setup_directory();
    create_test_file(TEST_DIR + "/target.txt", "target content");

    std::error_code ec;
    std::filesystem::create_symlink(
        TEST_DIR + "/target.txt",
        TEST_DIR + "/link.txt",
        ec
    );

    if (!ec) {
        DatabaseManager db(":memory:");
        db.initialize();
        ScanRepository scan_repo(db);
        ScanItemRepository item_repo(db);
        Scanner scanner(db, scan_repo, item_repo);

        auto scan = scanner.scan_directory(TEST_DIR);
        auto items = scanner.get_items(scan.id);

        bool found_link = false;
        for (const auto& item : items) {
            if (item.filename == "link.txt") found_link = true;
        }
        ASSERT_TRUE(!found_link);
    }
    cleanup();
    TEST_PASS();
}

static void test_analyze_returns_correct_counts() {
    TEST_BEGIN("analyze returns correct counts by extension/MIME");
    setup_directory();
    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    Scanner scanner(db, scan_repo, item_repo);

    auto scan = scanner.scan_directory(TEST_DIR);
    auto result = scanner.analyze(scan.id);

    ASSERT_TRUE(result.total_files >= 6);
    ASSERT_TRUE(result.by_extension.size() >= 3);
    ASSERT_TRUE(result.total_size > 0);
    cleanup();
    TEST_PASS();
}

static void test_content_preview_populated() {
    TEST_BEGIN("content_preview is populated for text files");
    setup_directory();
    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    Scanner scanner(db, scan_repo, item_repo);

    auto scan = scanner.scan_directory(TEST_DIR);
    auto items = scanner.get_items(scan.id);

    bool found_preview = false;
    for (const auto& item : items) {
        if (!item.content_preview.empty()) {
            found_preview = true;
            break;
        }
    }
    ASSERT_TRUE(found_preview);
    cleanup();
    TEST_PASS();
}

static void test_checksum_computed() {
    TEST_BEGIN("checksum is computed");
    setup_directory();
    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    Scanner scanner(db, scan_repo, item_repo);

    auto scan = scanner.scan_directory(TEST_DIR);
    auto items = scanner.get_items(scan.id);

    bool found_checksum = false;
    for (const auto& item : items) {
        if (!item.checksum.empty()) {
            found_checksum = true;
            break;
        }
    }
    ASSERT_TRUE(found_checksum);
    cleanup();
    TEST_PASS();
}

static void test_scan_empty_directory() {
    TEST_BEGIN("scan with empty directory returns zero counts");
    std::string empty_dir = TEST_DIR + "/empty";
    std::filesystem::create_directories(empty_dir);

    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    Scanner scanner(db, scan_repo, item_repo);

    auto scan = scanner.scan_directory(empty_dir);
    ASSERT_EQ(scan.file_count, 0);
    ASSERT_EQ(scan.status, ScanStatus::Completed);

    std::error_code ec;
    std::filesystem::remove_all(empty_dir, ec);
    cleanup();
    TEST_PASS();
}

static void test_scan_preserves_extension() {
    TEST_BEGIN("scan preserves file extensions");
    setup_directory();
    DatabaseManager db(":memory:");
    db.initialize();
    ScanRepository scan_repo(db);
    ScanItemRepository item_repo(db);
    Scanner scanner(db, scan_repo, item_repo);

    auto scan = scanner.scan_directory(TEST_DIR);
    auto items = scanner.get_items(scan.id);

    bool found_cpp = false;
    bool found_py = false;
    bool found_txt = false;
    for (const auto& item : items) {
        if (item.extension == ".cpp") found_cpp = true;
        if (item.extension == ".py") found_py = true;
        if (item.extension == ".txt") found_txt = true;
    }
    ASSERT_TRUE(found_cpp);
    ASSERT_TRUE(found_py);
    ASSERT_TRUE(found_txt);
    cleanup();
    TEST_PASS();
}

void run_scanner_tests() {
    std::cout << "=== Scanner Tests ===" << std::endl;

    test_scan_directory_finds_all_files();
    test_scan_directory_skips_git();
    test_scan_directory_skips_node_modules();
    test_scan_directory_skips_pycache();
    test_scan_directory_skips_symlinks();
    test_analyze_returns_correct_counts();
    test_content_preview_populated();
    test_checksum_computed();
    test_scan_empty_directory();
    test_scan_preserves_extension();
}
