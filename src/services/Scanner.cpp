#include "Scanner.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <map>

#include "../core/utils/Uuid.h"
#include "../core/utils/Logger.h"
#include "../hashing/FileHasher.h"
#include "../filesystem/FileUtils.h"

namespace archive::services {

using namespace archive::core;

Scanner::Scanner(storage::DatabaseManager& db,
                 storage::ScanRepository& scans,
                 storage::ScanItemRepository& scan_items)
    : db_(db)
    , scans_(scans)
    , scan_items_(scan_items)
{}

core::Scan Scanner::scan_directory(const std::string& root_path) {
    core::Scan scan;
    scan.id = core::utils::generate_id();
    scan.root_path = root_path;
    scan.status = core::ScanStatus::Running;
    scan.started_at = core::utils::now_iso();
    scans_.insert(scan);

    int file_count = 0;
    int folder_count = 0;

    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(
        root_path,
        std::filesystem::directory_options::skip_permission_denied,
        ec
    );

    if (ec) {
        scan.status = core::ScanStatus::Failed;
        scan.completed_at = core::utils::now_iso();
        scans_.update(scan);
        return scan;
    }

    auto end = std::filesystem::recursive_directory_iterator();

    for (; it != end; ++it) {
        const auto& entry = *it;

        if (entry.is_directory()) {
            std::string dirname = filesystem::FileUtils::file_name(entry.path().string());
            if (is_ignored(dirname)) {
                it.disable_recursion_pending();
                continue;
            }
            folder_count++;
            continue;
        }

        if (!entry.is_regular_file()) continue;

        if (entry.is_symlink()) continue;

        std::string filename = filesystem::FileUtils::file_name(entry.path().string());
        if (is_ignored(filename)) continue;

        try {
            core::ScanItem item = analyze_file(entry.path().string(), scan.id);
            scan_items_.insert(item);
            file_count++;
        } catch (const std::exception& e) {
            LOG_WARN("Failed to analyze file: " + entry.path().string() + " - " + e.what());
        }
    }

    scan.file_count = file_count;
    scan.folder_count = folder_count;
    scan.status = core::ScanStatus::Completed;
    scan.completed_at = core::utils::now_iso();
    scans_.update(scan);

    return scan;
}

core::AnalysisResult Scanner::analyze(const std::string& scan_id) const {
    core::AnalysisResult result;

    auto items = scan_items_.find_by_scan(scan_id);

    result.total_files = static_cast<int>(items.size());
    result.root_path = items.empty() ? "" : filesystem::FileUtils::parent_dir(items[0].path);

    for (const auto& item : items) {
        result.total_size += static_cast<int>(item.size);

        std::string ext = item.extension;
        if (!ext.empty()) {
            result.by_extension[ext]++;
        }

        std::string mime = item.mime_type;
        if (!mime.empty()) {
            result.by_mime[mime]++;
        }

        std::string proj = item.detected_project;
        if (!proj.empty()) {
            result.by_project[proj]++;
        }
    }

    return result;
}

std::vector<core::ScanItem> Scanner::get_items(const std::string& scan_id) const {
    return scan_items_.find_by_scan(scan_id);
}

core::ScanItem Scanner::analyze_file(const std::string& filepath, const std::string& scan_id) {
    core::ScanItem item;
    item.id = core::utils::generate_id();
    item.scan_id = scan_id;
    item.path = filepath;
    item.filename = filesystem::FileUtils::file_name(filepath);
    item.extension = filesystem::FileUtils::extension(filepath);
    item.size = static_cast<int64_t>(filesystem::FileUtils::file_size(filepath));
    item.created_at = core::utils::now_iso();
    item.modified_at = core::utils::now_iso();

    item.mime_type = detect_mime(item.extension);

    if (is_text_extension(item.extension)) {
        item.content_preview = read_content_preview(filepath);
    }

    item.detected_project = detect_project_context(filepath);

    try {
        item.checksum = hashing::FileHasher::hash_file(filepath);
    } catch (...) {
        item.checksum = "";
    }

    return item;
}

std::string Scanner::detect_mime(const std::string& extension) {
    static const std::map<std::string, std::string> mime_map = {
        {".txt", "text/plain"},
        {".md", "text/markdown"},
        {".rst", "text/x-rst"},
        {".csv", "text/csv"},
        {".log", "text/plain"},
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".ts", "application/typescript"},
        {".jsx", "application/javascript"},
        {".tsx", "application/typescript"},
        {".json", "application/json"},
        {".yaml", "application/x-yaml"},
        {".yml", "application/x-yaml"},
        {".toml", "application/toml"},
        {".xml", "application/xml"},
        {".sql", "application/sql"},
        {".py", "text/x-python"},
        {".rb", "text/x-ruby"},
        {".php", "text/x-php"},
        {".java", "text/x-java"},
        {".c", "text/x-c"},
        {".cpp", "text/x-c++"},
        {".cc", "text/x-c++"},
        {".cxx", "text/x-c++"},
        {".h", "text/x-c"},
        {".hpp", "text/x-c++"},
        {".cs", "text/x-csharp"},
        {".go", "text/x-go"},
        {".rs", "text/x-rust"},
        {".swift", "text/x-swift"},
        {".sh", "application/x-shellscript"},
        {".bash", "application/x-shellscript"},
        {".bat", "application/x-bat"},
        {".ps1", "application/x-powershell"},
        {".pdf", "application/pdf"},
        {".doc", "application/msword"},
        {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {".xls", "application/vnd.ms-excel"},
        {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {".ppt", "application/vnd.ms-powerpoint"},
        {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".webp", "image/webp"},
        {".svg", "image/svg+xml"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".ico", "image/x-icon"},
        {".mp4", "video/mp4"},
        {".avi", "video/x-msvideo"},
        {".mkv", "video/x-matroska"},
        {".mov", "video/quicktime"},
        {".wmv", "video/x-ms-wmv"},
        {".flv", "video/x-flv"},
        {".webm", "video/webm"},
        {".mp3", "audio/mpeg"},
        {".wav", "audio/wav"},
        {".flac", "audio/flac"},
        {".ogg", "audio/ogg"},
        {".aac", "audio/aac"},
        {".wma", "audio/x-ms-wma"},
        {".zip", "application/zip"},
        {".tar", "application/x-tar"},
        {".gz", "application/gzip"},
        {".bz2", "application/x-bzip2"},
        {".7z", "application/x-7z-compressed"},
        {".rar", "application/vnd.rar"},
        {".xz", "application/x-xz"},
        {".exe", "application/x-msdownload"},
        {".msi", "application/x-msdownload"},
        {".dmg", "application/x-apple-diskimage"},
        {".deb", "application/x-debian-package"},
        {".rpm", "application/x-rpm"},
        {".ttf", "font/ttf"},
        {".otf", "font/otf"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
    };

    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto it = mime_map.find(ext);
    if (it != mime_map.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

std::string Scanner::read_content_preview(const std::string& path, int max_bytes) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    std::vector<char> buffer(max_bytes, 0);
    file.read(buffer.data(), max_bytes);
    auto bytes_read = file.gcount();

    std::string preview(buffer.data(), static_cast<size_t>(bytes_read));

    size_t printable = 0;
    for (char c : preview) {
        if (c == '\n' || c == '\r' || c == '\t' ||
            (c >= 32 && c <= 126)) {
            printable++;
        }
    }

    double ratio = static_cast<double>(printable) / static_cast<double>(bytes_read > 0 ? bytes_read : 1);
    if (ratio < 0.7) {
        return "";
    }

    return preview;
}

std::string Scanner::detect_project_context(const std::string& filepath) {
    namespace fs = std::filesystem;

    fs::path file_path(filepath);
    fs::path dir = file_path.parent_path();

    static const std::vector<std::pair<std::string, std::string>> project_indicators = {
        {"CMakeLists.txt", "C++"},
        {"Makefile", "C/C++"},
        {"meson.build", "C/C++"},
        {"Cargo.toml", "Rust"},
        {"go.mod", "Go"},
        {"package.json", "Node.js"},
        {"tsconfig.json", "Node.js"},
        {"setup.py", "Python"},
        {"pyproject.toml", "Python"},
        {"requirements.txt", "Python"},
        {"Pipfile", "Python"},
        {"pom.xml", "Java"},
        {"build.gradle", "Java"},
        {"build.gradle.kts", "Java/Kotlin"},
        {"*.csproj", "C#"},
        {"*.sln", "C#"},
        {"Package.swift", "Swift"},
        {"pubspec.yaml", "Dart/Flutter"},
        {"Dockerfile", "Docker"},
        {"docker-compose.yml", "Docker"},
        {"docker-compose.yaml", "Docker"},
        {".gitignore", "Git"},
    };

    fs::path current = dir;
    int max_depth = 10;

    while (current != current.root_path() && max_depth > 0) {
        for (const auto& [indicator, project_type] : project_indicators) {
            if (indicator.find('*') != std::string::npos) {
                std::string stem_part = indicator.substr(0, indicator.find('*'));
                std::string ext_part = indicator.substr(indicator.find('*') + 1);

                for (const auto& entry : fs::directory_iterator(current)) {
                    if (entry.is_regular_file()) {
                        std::string name = entry.path().filename().string();
                        if (name.size() >= stem_part.size() + ext_part.size() &&
                            name.substr(0, stem_part.size()) == stem_part &&
                            name.substr(name.size() - ext_part.size()) == ext_part) {
                            return project_type;
                        }
                    }
                }
            } else {
                if (fs::exists(current / indicator)) {
                    return project_type;
                }
            }
        }

        current = current.parent_path();
        max_depth--;
    }

    return "";
}

bool Scanner::is_text_extension(const std::string& ext) {
    static const std::vector<std::string> text_exts = {
        ".txt", ".md", ".rst", ".csv", ".log",
        ".html", ".htm", ".css", ".js", ".ts", ".jsx", ".tsx",
        ".json", ".yaml", ".yml", ".toml", ".xml", ".sql",
        ".py", ".rb", ".php", ".java", ".c", ".cpp", ".cc",
        ".cxx", ".h", ".hpp", ".cs", ".go", ".rs", ".swift",
        ".sh", ".bash", ".bat", ".ps1",
        ".ini", ".cfg", ".conf", ".env",
        ".gitignore", ".dockerignore", ".editorconfig",
        ".tex", ".bib", ".sty", ".cls",
    };

    std::string lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    return std::find(text_exts.begin(), text_exts.end(), lower) != text_exts.end();
}

bool Scanner::is_ignored(const std::string& filename) {
    static const std::vector<std::string> ignored = {
        ".git", ".svn", ".hg",
        "__pycache__", ".pytest_cache", ".mypy_cache",
        "node_modules", ".npm", ".yarn",
        ".DS_Store", "Thumbs.db", "desktop.ini",
        ".vs", ".idea", ".vscode",
        "bin", "obj", "build", "out", "dist",
        ".cache", ".parcel-cache",
    };

    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    return std::find(ignored.begin(), ignored.end(), lower) != ignored.end();
}

} // namespace archive::services
