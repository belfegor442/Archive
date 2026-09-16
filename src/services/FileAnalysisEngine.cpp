#include "FileAnalysisEngine.h"
#include "../filesystem/FileUtils.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace fs = std::filesystem;

namespace archive {
namespace services {

static const std::map<std::string, std::string> MIME_MAP = {
    {".txt", "text/plain"}, {".md", "text/markdown"}, {".csv", "text/csv"},
    {".json", "application/json"}, {".xml", "application/xml"},
    {".yaml", "text/yaml"}, {".yml", "text/yaml"}, {".toml", "text/plain"},
    {".ini", "text/plain"}, {".cfg", "text/plain"}, {".conf", "text/plain"},
    {".cpp", "text/x-c++src"}, {".h", "text/x-c++hdr"}, {".hpp", "text/x-c++hdr"},
    {".c", "text/x-csrc"}, {".cc", "text/x-c++src"},
    {".py", "text/x-python"}, {".js", "text/javascript"}, {".ts", "text/typescript"},
    {".java", "text/x-java"}, {".rs", "text/x-rust"}, {".go", "text/x-go"},
    {".rb", "text/x-ruby"}, {".php", "text/x-php"}, {".swift", "text/x-swift"},
    {".kt", "text/x-kotlin"}, {".cs", "text/x-csharp"},
    {".html", "text/html"}, {".css", "text/css"}, {".scss", "text/css"},
    {".sql", "text/x-sql"}, {".sh", "text/x-shellscript"}, {".bat", "text/x-bat"},
    {".ps1", "text/x-powershell"},
    {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
    {".gif", "image/gif"}, {".bmp", "image/bmp"}, {".svg", "image/svg+xml"},
    {".webp", "image/webp"}, {".ico", "image/x-icon"}, {".tiff", "image/tiff"},
    {".pdf", "application/pdf"}, {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".zip", "application/zip"}, {".tar", "application/x-tar"},
    {".gz", "application/gzip"}, {".7z", "application/x-7z-compressed"},
    {".rar", "application/vnd.rar"}, {".bz2", "application/x-bzip2"},
    {".exe", "application/x-executable"}, {".msi", "application/x-msi"},
    {".dll", "application/x-sharedlib"}, {".so", "application/x-sharedlib"},
    {".dylib", "application/x-sharedlib"}, {".a", "application/x-archive"},
    {".o", "application/x-object"}, {".obj", "application/x-object"},
    {".lib", "application/x-archive"}, {".bin", "application/octet-stream"},
    {".dat", "application/octet-stream"}, {".log", "text/plain"},
};

static const std::vector<std::pair<std::string, std::string>> MAGIC_SIGNATURES = {
    {"89504e47", "image/png"}, {"ffd8ff", "image/jpeg"}, {"47494638", "image/gif"},
    {"25504446", "application/pdf"}, {"504b0304", "application/zip"},
    {"1f8b", "application/gzip"}, {"425a68", "application/x-bzip2"},
    {"377abcaf", "application/x-7z-compressed"}, {"52617221", "application/vnd.rar"},
    {"7f454c46", "application/x-executable"}, {"4d5a", "application/x-executable"},
    {"cfd8", "application/x-executable"},
};

static bool starts_with_ci(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); i++)
        if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)prefix[i]))
            return false;
    return true;
}

bool FileAnalysisEngine::is_text_extension(const std::string& ext) {
    static const std::vector<std::string> text_exts = {
        ".txt", ".md", ".csv", ".json", ".xml", ".yaml", ".yml", ".toml",
        ".ini", ".cfg", ".conf", ".log", ".rtf",
        ".cpp", ".h", ".hpp", ".c", ".cc", ".hh",
        ".py", ".js", ".ts", ".java", ".rs", ".go", ".rb", ".php",
        ".swift", ".kt", ".cs", ".m", ".mm",
        ".html", ".css", ".scss", ".less", ".sass",
        ".sql", ".sh", ".bash", ".zsh", ".bat", ".cmd", ".ps1",
        ".r", ".R", ".lua", ".pl", ".pm", ".tcl",
        ".tex", ".bib", ".sty", ".cls",
        ".makefile", ".cmake", ".dockerfile",
        ".gitignore", ".gitattributes", ".editorconfig",
        ".env", ".properties", ".gradle",
        ".sln", ".csproj", ".vcxproj", ".pro",
    };
    for (const auto& t : text_exts)
        if (ext == t) return true;
    return false;
}

bool FileAnalysisEngine::is_code_extension(const std::string& ext) {
    static const std::vector<std::string> code_exts = {
        ".cpp", ".c", ".cc", ".cxx", ".h", ".hpp", ".hh", ".hxx",
        ".py", ".pyw", ".js", ".jsx", ".ts", ".tsx", ".mjs", ".cjs",
        ".java", ".kt", ".kts", ".scala", ".groovy",
        ".rs", ".go", ".rb", ".php", ".swift", ".m", ".mm",
        ".cs", ".fs", ".vb", ".clj", ".cljs",
        ".lua", ".pl", ".pm", ".r", ".R",
        ".dart", ".ex", ".exs", ".erl", ".hs", ".ml",
        ".vue", ".svelte",
    };
    for (const auto& c : code_exts)
        if (ext == c) return true;
    return false;
}

bool FileAnalysisEngine::is_image_extension(const std::string& ext) {
    static const std::vector<std::string> img_exts = {
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".svg", ".webp",
        ".ico", ".tiff", ".tif", ".psd", ".ai", ".eps", ".raw", ".cr2",
        ".nef", ".arw", ".dng", ".heic", ".heif", ".avif",
    };
    for (const auto& i : img_exts)
        if (ext == i) return true;
    return false;
}

bool FileAnalysisEngine::is_config_extension(const std::string& ext) {
    static const std::vector<std::string> cfg_exts = {
        ".json", ".xml", ".yaml", ".yml", ".toml", ".ini", ".cfg",
        ".conf", ".properties", ".env", ".rc", ".config",
    };
    for (const auto& c : cfg_exts)
        if (ext == c) return true;
    return false;
}

bool FileAnalysisEngine::is_document_extension(const std::string& ext) {
    static const std::vector<std::string> doc_exts = {
        ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
        ".odt", ".ods", ".odp", ".rtf", ".tex", ".bib",
    };
    for (const auto& d : doc_exts)
        if (ext == d) return true;
    return false;
}

bool FileAnalysisEngine::is_archive_extension(const std::string& ext) {
    static const std::vector<std::string> arc_exts = {
        ".zip", ".tar", ".gz", ".bz2", ".xz", ".7z", ".rar",
        ".tar.gz", ".tar.bz2", ".tar.xz", ".tgz",
    };
    for (const auto& a : arc_exts)
        if (ext == a) return true;
    return false;
}

bool FileAnalysisEngine::is_executable_extension(const std::string& ext) {
    static const std::vector<std::string> exe_exts = {
        ".exe", ".msi", ".dll", ".so", ".dylib", ".a", ".lib",
        ".o", ".obj", ".app", ".deb", ".rpm", ".apk",
    };
    for (const auto& e : exe_exts)
        if (ext == e) return true;
    return false;
}

std::string FileAnalysisEngine::read_magic_bytes(const std::string& path, int count) const {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::vector<char> buf(count);
    f.read(buf.data(), count);
    std::ostringstream ss;
    for (int i = 0; i < (int)f.gcount(); i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (unsigned char)buf[i];
    return ss.str();
}

std::string FileAnalysisEngine::read_content_preview(const std::string& path, int max_bytes) const {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::string buf(max_bytes, '\0');
    f.read(&buf[0], max_bytes);
    buf.resize(f.gcount());
    bool has_null = false;
    for (char c : buf)
        if (c == '\0') { has_null = true; break; }
    if (has_null) return "";
    return buf;
}

std::string FileAnalysisEngine::detect_mime(const std::string& ext, const std::string& magic) const {
    auto it = MIME_MAP.find(ext);
    if (it != MIME_MAP.end()) return it->second;
    for (const auto& [sig, mime] : MAGIC_SIGNATURES)
        if (starts_with_ci(magic, sig)) return mime;
    return "application/octet-stream";
}

std::string FileAnalysisEngine::detect_language(const std::string& ext, const std::string& preview) const {
    if (ext == ".py") return "Python";
    if (ext == ".js" || ext == ".mjs" || ext == ".cjs") return "JavaScript";
    if (ext == ".ts" || ext == ".tsx") return "TypeScript";
    if (ext == ".java") return "Java";
    if (ext == ".rs") return "Rust";
    if (ext == ".go") return "Go";
    if (ext == ".rb") return "Ruby";
    if (ext == ".php") return "PHP";
    if (ext == ".swift") return "Swift";
    if (ext == ".kt") return "Kotlin";
    if (ext == ".cs") return "C#";
    if (ext == ".dart") return "Dart";
    if (ext == ".lua") return "Lua";
    if (ext == ".r" || ext == ".R") return "R";
    if (ext == ".scala") return "Scala";
    if (ext == ".hs") return "Haskell";
    if (ext == ".ex" || ext == ".exs") return "Elixir";
    if (ext == ".vue") return "Vue";
    if (ext == ".svelte") return "Svelte";
    if (ext == ".sql") return "SQL";
    if (ext == ".sh" || ext == ".bash") return "Shell";
    if (ext == ".ps1") return "PowerShell";
    if (ext == ".bat" || ext == ".cmd") return "Batch";

    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return "C++";
    if (ext == ".c") {
        if (preview.find("include <") != std::string::npos) return "C";
        return "C";
    }
    if (ext == ".h" || ext == ".hpp" || ext == ".hh") {
        if (!preview.empty()) {
            if (preview.find("class ") != std::string::npos ||
                preview.find("namespace ") != std::string::npos ||
                preview.find("template") != std::string::npos) return "C++";
            return "C";
        }
        return "C/C++";
    }
    if (ext == ".m" || ext == ".mm") return "Objective-C";

    if (!preview.empty()) {
        if (preview.find("#include") != std::string::npos) return "C/C++";
        if (preview.find("import ") != std::string::npos) return "Python/Java";
        if (preview.find("func ") != std::string::npos) return "Go";
        if (preview.find("fn ") != std::string::npos) return "Rust";
        if (preview.find("def ") != std::string::npos) return "Python";
        if (preview.find("function ") != std::string::npos) return "JavaScript";
        if (preview.find("class ") != std::string::npos) return "Java/C++";
    }
    return "";
}

std::string FileAnalysisEngine::detect_project_type(const std::string& dir_path) const {
    if (fs::exists(dir_path + "/CMakeLists.txt") || fs::exists(dir_path + "/Makefile") ||
        fs::exists(dir_path + "/meson.build") || fs::exists(dir_path + "/configure.ac"))
        return "C/C++";
    if (fs::exists(dir_path + "/package.json")) return "JavaScript/Node";
    if (fs::exists(dir_path + "/Cargo.toml")) return "Rust";
    if (fs::exists(dir_path + "/pyproject.toml") || fs::exists(dir_path + "/setup.py") ||
        fs::exists(dir_path + "/requirements.txt"))
        return "Python";
    if (fs::exists(dir_path + "/go.mod")) return "Go";
    if (fs::exists(dir_path + "/pom.xml") || fs::exists(dir_path + "/build.gradle"))
        return "Java";
    if (fs::exists(dir_path + "/Gemfile")) return "Ruby";
    if (fs::exists(dir_path + "/composer.json")) return "PHP";
    if (fs::exists(dir_path + "/Dockerfile")) return "Docker";
    if (fs::exists(dir_path + "/.git")) return "Git";
    return "";
}

std::string FileAnalysisEngine::detect_filename_pattern(const std::string& name) const {
    auto dot = name.rfind('.');
    std::string base = (dot != std::string::npos) ? name.substr(0, dot) : name;

    static const std::vector<std::pair<std::string, std::string>> patterns = {
        {"IMG_", "image_sequence"}, {"DSC_", "image_sequence"}, {"DCIM", "image_sequence"},
        {"photo", "image_sequence"}, {"Photo", "image_sequence"},
        {"screenshot", "screenshot"}, {"Screenshot", "screenshot"},
        {"capture", "screenshot"}, {"Capture", "screenshot"},
        {"copy", "copy_marker"}, {"Copy", "copy_marker"},
        {"backup", "backup"}, {"Backup", "backup"},
        {"temp", "temporary"}, {"tmp", "temporary"}, {"Temp", "temporary"},
        {"(1)", "download_copy"}, {"(2)", "download_copy"}, {"(3)", "download_copy"},
        {" - Copy", "copy_marker"}, {" - copy", "copy_marker"},
        {"_old", "version_marker"}, {"_new", "version_marker"},
        {"_v", "versioned"}, {"-v", "versioned"},
    };
    for (const auto& [pat, cat] : patterns)
        if (starts_with_ci(base, pat) || base.find(pat) != std::string::npos)
            return cat;

    auto is_seq_char = [](char c) -> bool { return std::isdigit((unsigned char)c) || c == '_' || c == '-'; };
    std::string digits;
    for (int i = (int)base.size() - 1; i >= 0 && is_seq_char(base[i]); i--)
        digits = base[i] + digits;

    if (!digits.empty() && digits.size() >= 2) {
        std::string prefix = base.substr(0, base.size() - digits.size());
        if (!prefix.empty()) {
            bool all_same = true;
            for (char c : prefix)
                if (c != prefix[0]) { all_same = false; break; }
            if (all_same && prefix.size() <= 4) return "numbered_sequence";
        }
        if (digits.size() >= 1) {
            std::string clean;
            for (char c : digits)
                if (std::isdigit((unsigned char)c)) clean += c;
            if (!clean.empty() && std::stoi(clean) > 0) return "numbered_sequence";
        }
    }

    return "unique";
}

FileEvidence FileAnalysisEngine::extract_evidence(const std::string& path) const {
    FileEvidence ev;
    ev.path_hint = path;

    fs::path p(path);
    ev.file_name = p.filename().string();
    ev.extension = p.extension().string();
    std::transform(ev.extension.begin(), ev.extension.end(), ev.extension.begin(), ::tolower);

    auto dir = p.parent_path();
    ev.depth = 0;
    auto temp = dir;
    while (temp.has_parent_path() && temp.parent_path() != temp) {
        ev.depth++;
        temp = temp.parent_path();
    }

    std::string dir_name;
    if (dir.has_filename()) dir_name = dir.filename().string();
    std::transform(dir_name.begin(), dir_name.end(), dir_name.begin(), ::tolower);
    ev.in_src_dir = (dir_name == "src" || dir_name == "source" || dir_name == "lib" || dir_name == "app");
    ev.in_test_dir = (dir_name == "test" || dir_name == "tests" || dir_name == "__tests__" ||
                      dir_name == "spec" || dir_name == "testing");
    ev.in_docs_dir = (dir_name == "docs" || dir_name == "doc" || dir_name == "documentation");
    ev.in_assets_dir = (dir_name == "assets" || dir_name == "images" || dir_name == "img" ||
                        dir_name == "media" || dir_name == "resources" || dir_name == "res");

    try { ev.size = fs::file_size(path); } catch (...) {}

    ev.magic_hex = read_magic_bytes(path);
    ev.mime_type = detect_mime(ev.extension, ev.magic_hex);
    ev.is_text = is_text_extension(ev.extension);
    ev.is_code = is_code_extension(ev.extension);
    ev.is_image = is_image_extension(ev.extension);
    ev.is_config = is_config_extension(ev.extension);
    ev.is_document = is_document_extension(ev.extension);
    ev.is_archive = is_archive_extension(ev.extension);
    ev.is_executable = is_executable_extension(ev.extension);
    ev.is_binary = !ev.is_text && !ev.is_config;

    if (ev.is_text || ev.is_code || ev.is_config) {
        std::string preview = read_content_preview(path);
        ev.language = detect_language(ev.extension, preview);
    }

    ev.project_type = detect_project_type(dir.string());
    ev.filename_pattern = detect_filename_pattern(ev.file_name);

    return ev;
}

std::vector<AnalysisSignal> FileAnalysisEngine::build_signals(const FileEvidence& ev) const {
    std::vector<AnalysisSignal> signals;

    if (!ev.extension.empty()) {
        signals.push_back({"extension", "type", 0.40, "Extension: " + ev.extension});
    }

    if (!ev.language.empty()) {
        signals.push_back({"language", "content", 0.30, "Language: " + ev.language});
    }

    if (!ev.project_type.empty()) {
        signals.push_back({"project", "context", 0.25, "Project: " + ev.project_type});
    }

    if (ev.in_src_dir)
        signals.push_back({"directory", "context", 0.15, "In source directory"});
    if (ev.in_test_dir)
        signals.push_back({"directory", "context", 0.15, "In test directory"});
    if (ev.in_docs_dir)
        signals.push_back({"directory", "context", 0.15, "In documentation directory"});
    if (ev.in_assets_dir)
        signals.push_back({"directory", "context", 0.15, "In assets directory"});

    if (!ev.mime_type.empty() && ev.mime_type != "application/octet-stream") {
        signals.push_back({"mime", "type", 0.10, "MIME: " + ev.mime_type});
    }

    if (ev.filename_pattern != "unique") {
        signals.push_back({"filename_pattern", "identity", 0.10, "Pattern: " + ev.filename_pattern});
    }

    if (ev.is_code) signals.push_back({"code", "type", 0.20, "Code file"});
    if (ev.is_image) signals.push_back({"image", "type", 0.30, "Image file"});
    if (ev.is_document) signals.push_back({"document", "type", 0.30, "Document file"});
    if (ev.is_config) signals.push_back({"config", "type", 0.25, "Configuration file"});
    if (ev.is_archive) signals.push_back({"archive", "type", 0.30, "Archive/compressed"});
    if (ev.is_executable) signals.push_back({"executable", "type", 0.30, "Executable file"});

    return signals;
}

FileAnalysis FileAnalysisEngine::analyze_file(const std::string& path) const {
    FileAnalysis analysis;
    analysis.file_path = path;
    analysis.file_name = fs::path(path).filename().string();
    analysis.evidence = extract_evidence(path);
    analysis.signals = build_signals(analysis.evidence);
    return analysis;
}

std::vector<FileAnalysis> FileAnalysisEngine::analyze_directory(
    const std::string& root_path,
    std::function<void(int done, int total)> progress) const {

    std::vector<FileAnalysis> results;
    std::vector<std::string> file_paths;

    for (auto& entry : fs::recursive_directory_iterator(root_path,
            fs::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name == "." || name == "..") continue;
        if (entry.path().string().find("\\.git\\") != std::string::npos ||
            entry.path().string().find("/.git/") != std::string::npos) continue;
        if (entry.path().string().find("\\node_modules\\") != std::string::npos ||
            entry.path().string().find("/node_modules/") != std::string::npos) continue;
        if (entry.path().string().find("\\__pycache__\\") != std::string::npos ||
            entry.path().string().find("/__pycache__/") != std::string::npos) continue;
        file_paths.push_back(entry.path().string());
    }

    int total = (int)file_paths.size();
    results.reserve(total);

    for (int i = 0; i < total; i++) {
        results.push_back(analyze_file(file_paths[i]));
        if (progress && (i % 100 == 0 || i == total - 1))
            progress(i + 1, total);
    }

    return results;
}

}
}
