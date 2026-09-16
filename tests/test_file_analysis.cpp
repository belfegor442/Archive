#include "test_helpers.h"
#include "services/FileAnalysisEngine.h"
#include "services/RelationshipEngine.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace archive::services;

void test_filename_pattern_detection() {
    TEST_BEGIN("FileAnalysisEngine: filename pattern detection");
    FileAnalysisEngine engine;
    fs::path test_dir = fs::temp_directory_path() / "archive_test_patterns";
    fs::create_directories(test_dir);

    std::ofstream(test_dir / "screenshot_2024.png") << "x";
    std::ofstream(test_dir / "report.pdf") << "x";
    std::ofstream(test_dir / "backup_data.txt") << "x";
    std::ofstream(test_dir / "temp_file.log") << "x";

    std::string p1 = (test_dir / "screenshot_2024.png").string();
    std::string p2 = (test_dir / "report.pdf").string();
    std::string p3 = (test_dir / "backup_data.txt").string();
    std::string p4 = (test_dir / "temp_file.log").string();

    FileAnalysis a1 = engine.analyze_file(p1);
    FileAnalysis a2 = engine.analyze_file(p2);
    FileAnalysis a3 = engine.analyze_file(p3);
    FileAnalysis a4 = engine.analyze_file(p4);

    std::string msg1 = "screenshot_2024.png file_name=[" + a1.evidence.file_name + "] pattern=[" + a1.evidence.filename_pattern + "]";
    std::string msg2 = "report.pdf file_name=[" + a2.evidence.file_name + "] pattern=[" + a2.evidence.filename_pattern + "]";
    std::string msg3 = "backup_data.txt file_name=[" + a3.evidence.file_name + "] pattern=[" + a3.evidence.filename_pattern + "]";
    std::string msg4 = "temp_file.log file_name=[" + a4.evidence.file_name + "] pattern=[" + a4.evidence.filename_pattern + "]";

    std::cout << "  " << msg1 << std::endl;
    std::cout << "  " << msg2 << std::endl;
    std::cout << "  " << msg3 << std::endl;
    std::cout << "  " << msg4 << std::endl;

    ASSERT_EQ(a1.evidence.filename_pattern, "screenshot");
    ASSERT_EQ(a2.evidence.filename_pattern, "unique");
    ASSERT_EQ(a3.evidence.filename_pattern, "backup");
    ASSERT_EQ(a4.evidence.filename_pattern, "temporary");

    fs::remove_all(test_dir);
    TEST_PASS();
}

void test_file_analysis_engine() {
    TEST_BEGIN("FileAnalysisEngine: analyze_file detects extensions");
    FileAnalysisEngine engine;
    fs::path test_dir = fs::temp_directory_path() / "archive_test_analysis";
    fs::create_directories(test_dir);

    std::ofstream(test_dir / "main.cpp") << "#include <iostream>";
    std::ofstream(test_dir / "README.md") << "# Hello";
    std::ofstream(test_dir / "photo.jpg") << "fake jpeg";
    std::ofstream(test_dir / "config.json") << "{}";
    std::ofstream(test_dir / "data.csv") << "a,b,c";

    auto a1 = engine.analyze_file((test_dir / "main.cpp").string());
    ASSERT_EQ(a1.evidence.extension, ".cpp");
    ASSERT_EQ(a1.evidence.is_code, true);

    auto a2 = engine.analyze_file((test_dir / "README.md").string());
    ASSERT_EQ(a2.evidence.extension, ".md");
    ASSERT_EQ(a2.evidence.is_text, true);

    auto a3 = engine.analyze_file((test_dir / "photo.jpg").string());
    ASSERT_EQ(a3.evidence.extension, ".jpg");
    ASSERT_EQ(a3.evidence.is_image, true);

    auto a4 = engine.analyze_file((test_dir / "config.json").string());
    ASSERT_EQ(a4.evidence.is_config, true);

    auto a5 = engine.analyze_file((test_dir / "data.csv").string());
    ASSERT_EQ(a5.evidence.is_text, true);

    fs::remove_all(test_dir);
    TEST_PASS();
}

void test_file_analysis_signals() {
    TEST_BEGIN("FileAnalysisEngine: signals generated");
    FileAnalysisEngine engine;
    fs::path test_dir = fs::temp_directory_path() / "archive_test_signals";
    fs::create_directories(test_dir);

    std::ofstream(test_dir / "app.py") << "print('hello')";
    auto analysis = engine.analyze_file((test_dir / "app.py").string());
    ASSERT_EQ(analysis.signals.size() > 0, true);

    bool found_ext = false;
    bool found_lang = false;
    for (const auto& sig : analysis.signals) {
        if (sig.source == "extension") found_ext = true;
        if (sig.source == "language") found_lang = true;
    }
    ASSERT_EQ(found_ext, true);
    ASSERT_EQ(found_lang, true);

    fs::remove_all(test_dir);
    TEST_PASS();
}

void test_relationship_engine_source_header() {
    TEST_BEGIN("RelationshipEngine: source/header pair detection");
    RelationshipEngine engine;
    fs::path test_dir = fs::temp_directory_path() / "archive_test_srcheap";
    fs::create_directories(test_dir);

    std::ofstream(test_dir / "main.cpp") << "x";
    std::ofstream(test_dir / "main.h") << "x";
    std::ofstream(test_dir / "utils.cpp") << "x";
    std::ofstream(test_dir / "utils.h") << "x";

    std::vector<std::string> files = {
        (test_dir / "main.cpp").string(),
        (test_dir / "main.h").string(),
        (test_dir / "utils.cpp").string(),
        (test_dir / "utils.h").string(),
    };

    auto rels = engine.find_relationships(files);
    int src_hdr_count = 0;
    for (const auto& r : rels)
        if (r.relationship_type == "source_header") src_hdr_count++;

    ASSERT_EQ(src_hdr_count, 2);

    fs::remove_all(test_dir);
    TEST_PASS();
}

void test_relationship_engine_numbered_sequence() {
    TEST_BEGIN("RelationshipEngine: numbered sequence detection");
    RelationshipEngine engine;
    fs::path test_dir = fs::temp_directory_path() / "archive_test_seq";
    fs::create_directories(test_dir);

    std::ofstream(test_dir / "photo001.jpg") << "x";
    std::ofstream(test_dir / "photo002.jpg") << "x";
    std::ofstream(test_dir / "photo003.jpg") << "x";

    std::vector<std::string> files = {
        (test_dir / "photo001.jpg").string(),
        (test_dir / "photo002.jpg").string(),
        (test_dir / "photo003.jpg").string(),
    };

    auto rels = engine.find_relationships(files);
    int seq_count = 0;
    for (const auto& r : rels)
        if (r.relationship_type == "numbered_sequence") seq_count++;

    ASSERT_EQ(seq_count > 0, true);

    fs::remove_all(test_dir);
    TEST_PASS();
}

void test_relationship_engine_export_group() {
    TEST_BEGIN("RelationshipEngine: export group detection");
    RelationshipEngine engine;
    fs::path test_dir = fs::temp_directory_path() / "archive_test_export";
    fs::create_directories(test_dir);

    std::ofstream(test_dir / "report.pdf") << "x";
    std::ofstream(test_dir / "report.docx") << "x";
    std::ofstream(test_dir / "report.odt") << "x";

    std::vector<std::string> files = {
        (test_dir / "report.pdf").string(),
        (test_dir / "report.docx").string(),
        (test_dir / "report.odt").string(),
    };

    auto rels = engine.find_relationships(files);
    int export_count = 0;
    for (const auto& r : rels)
        if (r.relationship_type == "export_group") export_count++;

    ASSERT_EQ(export_count > 0, true);

    fs::remove_all(test_dir);
    TEST_PASS();
}

void test_relationship_engine_families() {
    TEST_BEGIN("RelationshipEngine: group_into_families");
    RelationshipEngine engine;
    fs::path test_dir = fs::temp_directory_path() / "archive_test_families";
    fs::create_directories(test_dir);

    std::ofstream(test_dir / "main.cpp") << "x";
    std::ofstream(test_dir / "main.h") << "x";
    std::ofstream(test_dir / "utils.cpp") << "x";
    std::ofstream(test_dir / "utils.h") << "x";
    std::ofstream(test_dir / "standalone.txt") << "x";

    std::vector<std::string> files = {
        (test_dir / "main.cpp").string(),
        (test_dir / "main.h").string(),
        (test_dir / "utils.cpp").string(),
        (test_dir / "utils.h").string(),
        (test_dir / "standalone.txt").string(),
    };

    auto families = engine.group_into_families(files);
    ASSERT_EQ(families.size() >= 2, true);

    int src_hdr_families = 0;
    for (const auto& fam : families)
        if (fam.members.size() >= 2) src_hdr_families++;

    ASSERT_EQ(src_hdr_families >= 2, true);

    fs::remove_all(test_dir);
    TEST_PASS();
}

void test_analyze_directory() {
    TEST_BEGIN("FileAnalysisEngine: analyze_directory");
    FileAnalysisEngine engine;
    fs::path test_dir = fs::temp_directory_path() / "archive_test_dir_analysis";
    fs::create_directories(test_dir / "src");
    fs::create_directories(test_dir / ".git");

    std::ofstream(test_dir / "src" / "main.cpp") << "int main() {}";
    std::ofstream(test_dir / "src" / "main.h") << "#pragma once";
    std::ofstream(test_dir / "README.md") << "# Test";
    std::ofstream(test_dir / ".git" / "config") << "[core]";

    auto analyses = engine.analyze_directory(test_dir.string());
    ASSERT_EQ(analyses.size(), 3);

    fs::remove_all(test_dir);
    TEST_PASS();
}

void run_file_analysis_tests() {
    test_file_analysis_engine();
    test_file_analysis_signals();
    test_filename_pattern_detection();
    test_relationship_engine_source_header();
    test_relationship_engine_numbered_sequence();
    test_relationship_engine_export_group();
    test_relationship_engine_families();
    test_analyze_directory();
}
