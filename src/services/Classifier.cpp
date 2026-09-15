#include "Classifier.h"

#include <algorithm>
#include <cctype>

#include "../core/utils/Uuid.h"
#include "../core/utils/Logger.h"
#include "../storage/Transaction.h"

namespace archive::services {

using namespace archive::core;

Classifier::Classifier(storage::DatabaseManager& db,
                       storage::ClassificationRepository& classifications,
                       storage::ClassificationRuleRepository& rules,
                       storage::ScanItemRepository& scan_items)
    : db_(db)
    , classifications_(classifications)
    , rules_repo_(rules)
    , scan_items_(scan_items)
{}

std::vector<core::Classification> Classifier::classify_scan(const std::string& scan_id, int intensity) {
    auto items = scan_items_.find_by_scan(scan_id);
    std::vector<core::Classification> results;
    results.reserve(items.size());

    for (const auto& item : items) {
        core::Classification cls = classify_item(item, intensity);
        results.push_back(cls);
    }

    detect_relationships(results, items);

    {
        storage::Transaction tx(db_);
        for (const auto& cls : results) {
            classifications_.insert(cls);
        }
        tx.commit();
    }

    return results;
}

core::Classification Classifier::classify_item(const core::ScanItem& item, int intensity) {
    core::Classification cls;
    cls.id = core::utils::generate_id();
    cls.scan_item_id = item.id;

    std::string taxonomy_path = try_user_rules(item);
    if (!taxonomy_path.empty()) {
        cls.taxonomy_path = taxonomy_path;
        cls.confidence = 0.95;
        cls.reason = "Matched user-defined rule";
        return cls;
    }

    taxonomy_path = classify_by_extension(item, intensity);
    if (!taxonomy_path.empty()) {
        cls.taxonomy_path = taxonomy_path;
        cls.confidence = compute_confidence(item, taxonomy_path, intensity);
        cls.reason = generate_reason(item, taxonomy_path);
        return cls;
    }

    taxonomy_path = classify_by_content(item, intensity);
    if (!taxonomy_path.empty()) {
        cls.taxonomy_path = taxonomy_path;
        cls.confidence = compute_confidence(item, taxonomy_path, intensity);
        cls.reason = generate_reason(item, taxonomy_path);
        return cls;
    }

    taxonomy_path = classify_by_mime(item, intensity);
    if (!taxonomy_path.empty()) {
        cls.taxonomy_path = taxonomy_path;
        cls.confidence = compute_confidence(item, taxonomy_path, intensity);
        cls.reason = generate_reason(item, taxonomy_path);
        return cls;
    }

    taxonomy_path = classify_by_filename_pattern(item, intensity);
    if (!taxonomy_path.empty()) {
        cls.taxonomy_path = taxonomy_path;
        cls.confidence = 0.30;
        cls.reason = "Filename pattern heuristic";
        return cls;
    }

    cls.taxonomy_path = "Unknown";
    cls.confidence = 0.0;
    cls.reason = "No classification match";
    return cls;
}

void Classifier::set_rules(const std::vector<core::ClassificationRule>& rules) {
    rules_ = rules;
}

std::vector<std::pair<std::string, int>> Classifier::get_taxonomy_summary(const std::string& scan_id) const {
    auto classes = classifications_.find_by_scan(scan_id);
    std::map<std::string, int> taxonomy_counts;

    for (const auto& cls : classes) {
        std::string top_level = cls.taxonomy_path;
        auto pos = top_level.find('/');
        if (pos != std::string::npos) {
            top_level = top_level.substr(0, pos);
        }
        taxonomy_counts[top_level]++;
    }

    std::vector<std::pair<std::string, int>> sorted(taxonomy_counts.begin(), taxonomy_counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    return sorted;
}

std::string Classifier::try_user_rules(const core::ScanItem& item) {
    std::vector<core::ClassificationRule> active_rules;
    if (!rules_.empty()) {
        active_rules = rules_;
    } else {
        active_rules = rules_repo_.find_enabled();
    }

    std::sort(active_rules.begin(), active_rules.end(),
              [](const core::ClassificationRule& a, const core::ClassificationRule& b) {
                  return a.priority > b.priority;
              });

    for (const auto& rule : active_rules) {
        if (!rule.enabled) continue;
        if (matches_pattern(item.filename, rule.pattern)) {
            return rule.target_path;
        }
    }

    return "";
}

std::string Classifier::classify_by_extension(const core::ScanItem& item, int intensity) {
    std::string ext = item.extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext.empty()) return "";

    struct ExtMapping {
        const char* extension;
        const char* category;
        const char* subcategory;
        const char* detail;
    };

    static const ExtMapping mappings[] = {
        {".cpp", "Development", "C++", "Source"},
        {".cc", "Development", "C++", "Source"},
        {".cxx", "Development", "C++", "Source"},
        {".c", "Development", "C", "Source"},
        {".h", "Development", "C++", "Header"},
        {".hpp", "Development", "C++", "Header"},

        {".py", "Development", "Python", "Source"},
        {".js", "Development", "JavaScript", "Source"},
        {".ts", "Development", "TypeScript", "Source"},
        {".jsx", "Development", "React", "Component"},
        {".tsx", "Development", "React", "Component"},

        {".rs", "Development", "Rust", "Source"},
        {".go", "Development", "Go", "Source"},
        {".java", "Development", "Java", "Source"},
        {".cs", "Development", "CSharp", "Source"},
        {".swift", "Development", "Swift", "Source"},
        {".rb", "Development", "Ruby", "Source"},
        {".php", "Development", "PHP", "Source"},

        {".html", "Development", "Web", "HTML"},
        {".htm", "Development", "Web", "HTML"},
        {".css", "Development", "Web", "CSS"},
        {".scss", "Development", "Web", "CSS"},
        {".less", "Development", "Web", "CSS"},

        {".sql", "Data", "SQL", "Query"},

        {".json", "Configuration", "Data", "JSON"},
        {".yaml", "Configuration", "Data", "YAML"},
        {".yml", "Configuration", "Data", "YAML"},
        {".toml", "Configuration", "Data", "TOML"},
        {".xml", "Configuration", "Data", "XML"},
        {".ini", "Configuration", "Data", "INI"},
        {".cfg", "Configuration", "Data", "Config"},
        {".conf", "Configuration", "Data", "Config"},

        {".txt", "Documents", "Text", "Plain"},
        {".md", "Documents", "Text", "Markdown"},
        {".rst", "Documents", "Text", "reStructuredText"},
        {".doc", "Documents", "Word", "Legacy"},
        {".docx", "Documents", "Word", "Modern"},
        {".pdf", "Documents", "PDF", "Document"},
        {".tex", "Documents", "LaTeX", "Document"},
        {".bib", "Documents", "LaTeX", "Bibliography"},

        {".jpg", "Images", "Photos", "JPEG"},
        {".jpeg", "Images", "Photos", "JPEG"},
        {".png", "Images", "Photos", "PNG"},
        {".gif", "Images", "Photos", "GIF"},
        {".bmp", "Images", "Photos", "BMP"},
        {".webp", "Images", "Photos", "WebP"},
        {".svg", "Images", "Vector", "SVG"},
        {".tiff", "Images", "Photos", "TIFF"},
        {".tif", "Images", "Photos", "TIFF"},
        {".ico", "Images", "Icon", "ICO"},

        {".mp4", "Video", "Container", "MP4"},
        {".avi", "Video", "Container", "AVI"},
        {".mkv", "Video", "Container", "MKV"},
        {".mov", "Video", "Container", "MOV"},
        {".wmv", "Video", "Container", "WMV"},
        {".flv", "Video", "Container", "FLV"},
        {".webm", "Video", "Container", "WebM"},

        {".mp3", "Audio", "Music", "MP3"},
        {".wav", "Audio", "Music", "WAV"},
        {".flac", "Audio", "Music", "FLAC"},
        {".ogg", "Audio", "Music", "OGG"},
        {".aac", "Audio", "Music", "AAC"},
        {".wma", "Audio", "Music", "WMA"},

        {".zip", "Archives", "Compressed", "ZIP"},
        {".tar", "Archives", "Compressed", "TAR"},
        {".gz", "Archives", "Compressed", "GZIP"},
        {".bz2", "Archives", "Compressed", "BZIP2"},
        {".7z", "Archives", "Compressed", "7Z"},
        {".rar", "Archives", "Compressed", "RAR"},
        {".xz", "Archives", "Compressed", "XZ"},

        {".exe", "Software", "Binary", "Windows"},
        {".msi", "Software", "Binary", "Windows"},
        {".dmg", "Software", "Binary", "macOS"},
        {".deb", "Software", "Binary", "Debian"},
        {".rpm", "Software", "Binary", "RPM"},
        {".app", "Software", "Binary", "macOS"},

        {".ttf", "Fonts", "TrueType", "TTF"},
        {".otf", "Fonts", "OpenType", "OTF"},
        {".woff", "Fonts", "WebFont", "WOFF"},
        {".woff2", "Fonts", "WebFont", "WOFF2"},

        {".csv", "Data", "Spreadsheet", "CSV"},
        {".xls", "Data", "Spreadsheet", "Legacy"},
        {".xlsx", "Data", "Spreadsheet", "Modern"},

        {".log", "Documents", "Logs", "Text"},
        {".env", "Configuration", "Environment", "Variables"},
    };

    for (const auto& m : mappings) {
        if (ext == m.extension) {
            return build_taxonomy_path(m.category, m.subcategory, m.detail, intensity);
        }
    }

    return "";
}

std::string Classifier::classify_by_mime(const core::ScanItem& item, int intensity) {
    std::string mime = item.mime_type;
    if (mime.empty()) return "";

    if (mime.find("image/") == 0) {
        return build_taxonomy_path("Images", "Photos", mime.substr(6), intensity);
    }
    if (mime.find("video/") == 0) {
        return build_taxonomy_path("Video", "Media", mime.substr(6), intensity);
    }
    if (mime.find("audio/") == 0) {
        return build_taxonomy_path("Audio", "Music", mime.substr(6), intensity);
    }
    if (mime == "application/pdf") {
        return build_taxonomy_path("Documents", "PDF", "Document", intensity);
    }
    if (mime.find("text/") == 0) {
        return build_taxonomy_path("Documents", "Text", "Plain", intensity);
    }
    if (mime.find("application/zip") != std::string::npos ||
        mime.find("application/x-tar") != std::string::npos ||
        mime.find("application/gzip") != std::string::npos ||
        mime.find("application/x-bzip2") != std::string::npos ||
        mime.find("application/x-7z") != std::string::npos ||
        mime.find("application/vnd.rar") != std::string::npos) {
        return build_taxonomy_path("Archives", "Compressed", "Archive", intensity);
    }
    if (mime.find("font/") != std::string::npos) {
        return build_taxonomy_path("Fonts", "Font", "Font", intensity);
    }
    if (mime == "application/json" || mime == "application/xml" || mime == "application/x-yaml") {
        return build_taxonomy_path("Configuration", "Data", "Structured", intensity);
    }

    return "";
}

std::string Classifier::classify_by_content(const core::ScanItem& item, int intensity) {
    if (item.content_preview.empty()) return "";

    const std::string& preview = item.content_preview;

    if (preview.find("#!/usr/bin/env python") != std::string::npos ||
        preview.find("#!python") != std::string::npos ||
        preview.find("import ") == 0 ||
        preview.find("from ") == 0) {
        return build_taxonomy_path("Development", "Python", "Script", intensity);
    }

    if (preview.find("#!/bin/bash") != std::string::npos ||
        preview.find("#!/bin/sh") != std::string::npos ||
        preview.find("#!/usr/bin/env bash") != std::string::npos) {
        return build_taxonomy_path("Development", "Shell", "Script", intensity);
    }

    if (preview.find("<!DOCTYPE") != std::string::npos ||
        preview.find("<html") != std::string::npos ||
        preview.find("<HTML") != std::string::npos) {
        return build_taxonomy_path("Development", "Web", "HTML", intensity);
    }

    if (preview.find("<?xml") != std::string::npos ||
        preview.find("<xml") != std::string::npos) {
        return build_taxonomy_path("Configuration", "Data", "XML", intensity);
    }

    if (preview.find("{") != std::string::npos && preview.find(":") != std::string::npos) {
        return build_taxonomy_path("Configuration", "Data", "JSON", intensity);
    }

    if (preview.find("[") == 0 && preview.find(",") != std::string::npos) {
        return build_taxonomy_path("Configuration", "Data", "Data", intensity);
    }

    int comma_count = 0;
    int newline_count = 0;
    for (char c : preview) {
        if (c == ',') comma_count++;
        if (c == '\n') newline_count++;
    }

    if (newline_count > 2 && comma_count > newline_count) {
        return build_taxonomy_path("Data", "Spreadsheet", "CSV", intensity);
    }

    return "";
}

std::string Classifier::classify_by_project(const core::ScanItem& item, int intensity) {
    std::string project = item.detected_project;
    if (project.empty()) return "";

    static const std::map<std::string, std::pair<std::string, std::string>> project_to_taxonomy = {
        {"C++", {"Development", "C++"}},
        {"C/C++", {"Development", "C++"}},
        {"Rust", {"Development", "Rust"}},
        {"Go", {"Development", "Go"}},
        {"Python", {"Development", "Python"}},
        {"Node.js", {"Development", "JavaScript"}},
        {"Java", {"Development", "Java"}},
        {"Java/Kotlin", {"Development", "Java"}},
        {"C#", {"Development", "CSharp"}},
        {"Swift", {"Development", "Swift"}},
        {"Dart/Flutter", {"Development", "Dart"}},
        {"Docker", {"DevOps", "Docker"}},
        {"Git", {"Configuration", "VCS"}},
    };

    auto it = project_to_taxonomy.find(project);
    if (it != project_to_taxonomy.end()) {
        return build_taxonomy_path(it->second.first, it->second.second, "Project", intensity);
    }

    return build_taxonomy_path("Development", "Other", project, intensity);
}

std::string Classifier::classify_by_filename_pattern(const core::ScanItem& item, int intensity) {
    std::string name = item.filename;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    if (name.find("readme") != std::string::npos) {
        return build_taxonomy_path("Documents", "Documentation", "README", intensity);
    }
    if (name.find("license") != std::string::npos || name.find("licence") != std::string::npos) {
        return build_taxonomy_path("Documents", "Legal", "License", intensity);
    }
    if (name.find("changelog") != std::string::npos) {
        return build_taxonomy_path("Documents", "Documentation", "Changelog", intensity);
    }
    if (name.find("makefile") != std::string::npos) {
        return build_taxonomy_path("Development", "Build", "Makefile", intensity);
    }
    if (name.find("dockerfile") != std::string::npos) {
        return build_taxonomy_path("DevOps", "Docker", "Dockerfile", intensity);
    }
    if (name.find(".gitignore") != std::string::npos) {
        return build_taxonomy_path("Configuration", "VCS", "Git", intensity);
    }
    if (name.find(".env") != std::string::npos) {
        return build_taxonomy_path("Configuration", "Environment", "Variables", intensity);
    }

    return "";
}

double Classifier::compute_confidence(const core::ScanItem& item, const std::string& taxonomy_path, int intensity) {
    if (taxonomy_path == "Unknown") return 0.0;

    if (!item.detected_project.empty()) {
        return 0.85 + (static_cast<double>(intensity) / 100.0) * 0.10;
    }

    if (!item.extension.empty() && taxonomy_path.find("Unknown") == std::string::npos) {
        return 0.70 + (static_cast<double>(intensity) / 100.0) * 0.15;
    }

    if (!item.mime_type.empty() && item.mime_type != "application/octet-stream") {
        return 0.50 + (static_cast<double>(intensity) / 100.0) * 0.20;
    }

    if (!item.content_preview.empty()) {
        return 0.30 + (static_cast<double>(intensity) / 100.0) * 0.20;
    }

    return 0.0;
}

std::string Classifier::generate_reason(const core::ScanItem& item, const std::string& taxonomy_path) {
    if (!item.detected_project.empty() &&
        taxonomy_path.find(item.detected_project) != std::string::npos) {
        return "Extension match with project context (" + item.detected_project + ")";
    }

    if (!item.extension.empty()) {
        return "Extension " + item.extension + " maps to " + taxonomy_path;
    }

    if (!item.mime_type.empty() && item.mime_type != "application/octet-stream") {
        return "MIME type " + item.mime_type + " maps to " + taxonomy_path;
    }

    if (!item.content_preview.empty()) {
        return "Content analysis identifies " + taxonomy_path;
    }

    return "Filename pattern match";
}

std::string Classifier::build_taxonomy_path(const std::string& category,
                                             const std::string& subcategory,
                                             const std::string& detail,
                                             int intensity) {
    if (intensity <= 25) {
        return category;
    }
    if (intensity <= 50) {
        return category + "/" + subcategory;
    }
    if (intensity <= 75) {
        return category + "/" + subcategory + "/" + detail;
    }
    return category + "/" + subcategory + "/" + detail;
}

void Classifier::detect_relationships(std::vector<core::Classification>& classifications,
                                       const std::vector<core::ScanItem>& items) {
    auto groups = find_name_groups(items);

    for (auto& [base_name, member_ids] : groups) {
        if (member_ids.size() < 2) continue;

        for (auto& cls : classifications) {
            bool found = false;
            for (const auto& member_id : member_ids) {
                if (cls.scan_item_id == member_id) {
                    found = true;
                    break;
                }
            }
            if (found) {
                if (cls.reason.find("Grouped with") == std::string::npos) {
                    cls.reason += " (Grouped with " + std::to_string(member_ids.size() - 1) + " related files)";
                }
            }
        }
    }
}

std::map<std::string, std::vector<std::string>> Classifier::find_name_groups(const std::vector<core::ScanItem>& items) {
    std::map<std::string, std::vector<std::string>> groups;

    for (const auto& item : items) {
        std::string stem = item.filename;
        auto dot_pos = stem.rfind('.');
        if (dot_pos != std::string::npos) {
            stem = stem.substr(0, dot_pos);
        }

        std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);

        if (!stem.empty()) {
            groups[stem].push_back(item.id);
        }
    }

    std::map<std::string, std::vector<std::string>> result;
    for (auto& [key, ids] : groups) {
        if (ids.size() >= 2) {
            result[key] = std::move(ids);
        }
    }

    return result;
}

bool Classifier::matches_pattern(const std::string& filename, const std::string& pattern) {
    if (pattern.empty()) return false;

    std::string lower_name = filename;
    std::string lower_pattern = pattern;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    std::transform(lower_pattern.begin(), lower_pattern.end(), lower_pattern.begin(), ::tolower);

    if (lower_pattern.find('*') == std::string::npos) {
        return lower_name == lower_pattern;
    }

    size_t star_pos = lower_pattern.find('*');
    std::string prefix = lower_pattern.substr(0, star_pos);
    std::string suffix = lower_pattern.substr(star_pos + 1);

    if (!prefix.empty() && lower_name.find(prefix) != 0) return false;
    if (!suffix.empty()) {
        if (suffix.size() > lower_name.size()) return false;
        if (lower_name.compare(lower_name.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
    }

    return true;
}

} // namespace archive::services
