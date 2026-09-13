#include "test_helpers.h"

#include "hashing/FileHasher.h"
#include "filesystem/FileUtils.h"

#include <fstream>
#include <cstdio>

using namespace archive::hashing;
using namespace archive::filesystem;

static const std::string TEST_FILE = "test_hash_input.bin";

static void create_test_file(const std::string& content) {
    std::ofstream f(TEST_FILE, std::ios::binary);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

static void cleanup() {
    std::remove(TEST_FILE.c_str());
}

// --- FileHasher tests ---

static void test_hash_string() {
    TEST_BEGIN("FileHasher::hash_buffer with known data");
    const char* data = "hello world";
    std::string hash = FileHasher::hash_buffer(data, 11);
    ASSERT_TRUE(hash.size() == 64);
    ASSERT_EQ(hash, "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
    TEST_PASS();
}

static void test_hash_file() {
    TEST_BEGIN("FileHasher::hash_file");
    create_test_file("test content for hashing");
    std::string hash = FileHasher::hash_file(TEST_FILE);
    ASSERT_TRUE(hash.size() == 64);
    cleanup();
    TEST_PASS();
}

static void test_hash_consistency() {
    TEST_BEGIN("FileHasher::hash_file consistent results");
    create_test_file("consistent data");
    std::string h1 = FileHasher::hash_file(TEST_FILE);
    std::string h2 = FileHasher::hash_file(TEST_FILE);
    ASSERT_EQ(h1, h2);
    cleanup();
    TEST_PASS();
}

static void test_hash_different_files() {
    TEST_BEGIN("FileHasher different files produce different hashes");
    create_test_file("content A");
    std::string h1 = FileHasher::hash_file(TEST_FILE);
    cleanup();

    create_test_file("content B");
    std::string h2 = FileHasher::hash_file(TEST_FILE);
    cleanup();

    ASSERT_TRUE(h1 != h2);
    TEST_PASS();
}

// --- FileUtils tests ---

static void test_file_exists() {
    TEST_BEGIN("FileUtils::file_exists");
    create_test_file("exists");
    ASSERT_TRUE(FileUtils::file_exists(TEST_FILE));
    cleanup();
    ASSERT_TRUE(!FileUtils::file_exists(TEST_FILE));
    TEST_PASS();
}

static void test_file_size() {
    TEST_BEGIN("FileUtils::file_size");
    create_test_file("12345");
    ASSERT_EQ(FileUtils::file_size(TEST_FILE), 5u);
    cleanup();
    TEST_PASS();
}

static void test_file_name_and_ext() {
    TEST_BEGIN("FileUtils::file_name and ::extension");
    ASSERT_EQ(FileUtils::file_name("/home/user/document.pdf"), "document.pdf");
    ASSERT_EQ(FileUtils::extension("/home/user/document.pdf"), ".pdf");
    ASSERT_EQ(FileUtils::parent_dir("/home/user/document.pdf"), "/home/user");
    TEST_PASS();
}

static void test_create_directories() {
    TEST_BEGIN("FileUtils::create_directories");
    std::string path = "test_dir_create/sub/nested";
    FileUtils::create_directories(path);
    ASSERT_TRUE(FileUtils::directory_exists(path));
    FileUtils::remove_directory("test_dir_create");
    TEST_PASS();
}

static void test_copy_file() {
    TEST_BEGIN("FileUtils::copy_file");
    create_test_file("copy me");
    std::string dest = "test_copy_dest.txt";
    FileUtils::copy_file(TEST_FILE, dest);
    ASSERT_TRUE(FileUtils::file_exists(dest));
    ASSERT_EQ(FileUtils::file_size(dest), FileUtils::file_size(TEST_FILE));
    cleanup();
    std::remove(dest.c_str());
    TEST_PASS();
}

static void test_count_files() {
    TEST_BEGIN("FileUtils::count_files");
    std::string dir = "test_count_dir";
    FileUtils::create_directories(dir + "/sub");
    {
        std::ofstream f1(dir + "/a.txt");
        f1 << "a";
    }
    {
        std::ofstream f2(dir + "/sub/b.txt");
        f2 << "b";
    }
    ASSERT_EQ(FileUtils::count_files(dir), 2);
    FileUtils::remove_directory(dir);
    TEST_PASS();
}

void run_hashing_tests() {
    std::cout << "=== Hashing Tests ===" << std::endl;

    test_hash_string();
    test_hash_file();
    test_hash_consistency();
    test_hash_different_files();
    test_file_exists();
    test_file_size();
    test_file_name_and_ext();
    test_create_directories();
    test_copy_file();
    test_count_files();

    cleanup();
}
