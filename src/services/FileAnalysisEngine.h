#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <functional>

namespace archive {
namespace services {

struct FileEvidence {
    std::string file_name;
    std::string extension;
    std::string mime_type;
    std::string magic_hex;
    int64_t size = 0;
    bool is_text = false;
    bool is_binary = false;
    bool is_image = false;
    bool is_code = false;
    bool is_config = false;
    bool is_document = false;
    bool is_archive = false;
    bool is_executable = false;
    std::string language;
    std::string project_type;
    std::string filename_pattern;
    std::vector<std::string> content_signals;
    std::string path_hint;
    int depth = 0;
    bool in_src_dir = false;
    bool in_test_dir = false;
    bool in_docs_dir = false;
    bool in_assets_dir = false;
};

struct AnalysisSignal {
    std::string source;
    std::string category;
    double weight = 0.0;
    std::string description;
};

struct FileAnalysis {
    std::string file_path;
    std::string file_name;
    FileEvidence evidence;
    std::vector<AnalysisSignal> signals;
    std::vector<std::string> related_files;
    std::vector<std::string> family_members;
    std::string detected_project;
    double confidence = 0.0;
};

class FileAnalysisEngine {
public:
    FileAnalysis analyze_file(const std::string& path) const;
    std::vector<FileAnalysis> analyze_directory(const std::string& root_path,
        std::function<void(int done, int total)> progress = nullptr) const;

    static bool is_text_extension(const std::string& ext);
    static bool is_code_extension(const std::string& ext);
    static bool is_image_extension(const std::string& ext);
    static bool is_config_extension(const std::string& ext);
    static bool is_document_extension(const std::string& ext);
    static bool is_archive_extension(const std::string& ext);
    static bool is_executable_extension(const std::string& ext);

private:
    FileEvidence extract_evidence(const std::string& path) const;
    std::string read_magic_bytes(const std::string& path, int count = 16) const;
    std::string detect_mime(const std::string& ext, const std::string& magic) const;
    std::string detect_language(const std::string& ext, const std::string& content_preview) const;
    std::string detect_project_type(const std::string& dir_path) const;
    std::string detect_filename_pattern(const std::string& name) const;
    std::vector<AnalysisSignal> build_signals(const FileEvidence& ev) const;
    std::string read_content_preview(const std::string& path, int max_bytes = 4096) const;
};

}
}
