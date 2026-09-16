#include "RelationshipEngine.h"
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <cmath>

namespace fs = std::filesystem;

namespace archive {
namespace services {

std::string RelationshipEngine::base_name(const std::string& path) const {
    std::string name = fs::path(path).filename().string();
    auto dot = name.rfind('.');
    return (dot != std::string::npos) ? name.substr(0, dot) : name;
}

std::string RelationshipEngine::extension_of(const std::string& path) const {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

bool RelationshipEngine::are_source_header_pair(const std::string& a, const std::string& b) const {
    std::string ext_a = extension_of(a);
    std::string ext_b = extension_of(b);
    std::string base_a = base_name(a);
    std::string base_b = base_name(b);

    if (base_a != base_b) return false;

    static const std::vector<std::string> src_exts = {".c", ".cpp", ".cc", ".cxx"};
    static const std::vector<std::string> hdr_exts = {".h", ".hpp", ".hh", ".hxx"};

    bool a_src = std::find(src_exts.begin(), src_exts.end(), ext_a) != src_exts.end();
    bool a_hdr = std::find(hdr_exts.begin(), hdr_exts.end(), ext_a) != hdr_exts.end();
    bool b_src = std::find(src_exts.begin(), src_exts.end(), ext_b) != src_exts.end();
    bool b_hdr = std::find(hdr_exts.begin(), hdr_exts.end(), ext_b) != hdr_exts.end();

    return (a_src && b_hdr) || (a_hdr && b_src);
}

bool RelationshipEngine::are_numbered_sequence(const std::string& a, const std::string& b) const {
    std::string base_a = base_name(a);
    std::string base_b = base_name(b);
    std::string ext_a = extension_of(a);
    std::string ext_b = extension_of(b);

    if (ext_a != ext_b) return false;

    auto extract_num = [](const std::string& name) -> std::pair<std::string, int> {
        int end = (int)name.size() - 1;
        while (end >= 0 && (std::isdigit((unsigned char)name[end]) || name[end] == '_' || name[end] == '-'))
            end--;
        std::string suffix = name.substr(end + 1);
        std::string prefix = name.substr(0, end + 1);
        int num = 0;
        if (!suffix.empty()) {
            std::string digits;
            for (char c : suffix)
                if (std::isdigit((unsigned char)c)) digits += c;
            if (!digits.empty()) num = std::stoi(digits);
        }
        return {prefix, num};
    };

    auto [prefix_a, num_a] = extract_num(base_a);
    auto [prefix_b, num_b] = extract_num(base_b);

    return prefix_a == prefix_b && num_a != num_b &&
           std::abs(num_a - num_b) <= 100 &&
           num_a > 0 && num_b > 0;
}

bool RelationshipEngine::are_same_name_variants(const std::string& a, const std::string& b) const {
    std::string name_a = fs::path(a).filename().string();
    std::string name_b = fs::path(b).filename().string();
    if (name_a == name_b) return false;

    std::string base_a = base_name(a);
    std::string base_b = base_name(b);

    auto strip_suffix = [](const std::string& name, const std::vector<std::string>& suffixes) -> std::string {
        for (const auto& s : suffixes) {
            if (name.size() > s.size() && name.substr(name.size() - s.size()) == s) {
                std::string stripped = name.substr(0, name.size() - s.size());
                while (!stripped.empty() && (stripped.back() == '_' || stripped.back() == '-'))
                    stripped.pop_back();
                return stripped;
            }
        }
        return name;
    };

    std::string stripped_a = strip_suffix(base_a, {"_copy", " (copy)", " - Copy", "_backup", "_old", "_new", "_v2", "_v3"});
    std::string stripped_b = strip_suffix(base_b, {"_copy", " (copy)", " - Copy", "_backup", "_old", "_new", "_v2", "_v3"});

    return stripped_a == stripped_b && stripped_a != base_a;
}

bool RelationshipEngine::are_export_groups(const std::string& a, const std::string& b) const {
    std::string base_a = base_name(a);
    std::string base_b = base_name(b);
    std::string ext_a = extension_of(a);
    std::string ext_b = extension_of(b);

    if (ext_a == ext_b) return false;

    static const std::vector<std::pair<std::string, std::string>> export_pairs = {
        {".png", ".jpg"}, {".png", ".jpeg"}, {".png", ".webp"}, {".png", ".svg"},
        {".pdf", ".docx"}, {".pdf", ".doc"}, {".pdf", ".odt"},
        {".csv", ".xlsx"}, {".csv", ".xls"},
        {".html", ".pdf"}, {".md", ".pdf"},
    };

    for (const auto& [e1, e2] : export_pairs) {
        if ((ext_a == e1 && ext_b == e2) || (ext_a == e2 && ext_b == e1))
            return base_a == base_b;
    }
    return false;
}

bool RelationshipEngine::are_document_attachments(const std::string& a, const std::string& b) const {
    std::string name_a = fs::path(a).filename().string();
    std::string name_b = fs::path(b).filename().string();

    if (name_a == name_b) return false;

    std::string base_a = base_name(a);
    std::string base_b = base_name(b);

    if (base_a.find(base_b) != std::string::npos || base_b.find(base_a) != std::string::npos)
        return true;

    auto clean = [](const std::string& s) -> std::string {
        std::string r;
        for (char c : s)
            if (std::isalnum((unsigned char)c)) r += std::tolower((unsigned char)c);
        return r;
    };

    std::string ca = clean(base_a);
    std::string cb = clean(base_b);

    if (ca.find(cb) != std::string::npos || cb.find(ca) != std::string::npos)
        return true;

    return false;
}

bool RelationshipEngine::are_project_files(const std::string& a, const std::string& b) const {
    std::string dir_a = fs::path(a).parent_path().string();
    std::string dir_b = fs::path(b).parent_path().string();
    return dir_a == dir_b;
}

std::vector<FileRelationship> RelationshipEngine::find_relationships(
    const std::vector<std::string>& file_paths) const {

    std::vector<FileRelationship> relationships;
    int n = (int)file_paths.size();

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (are_source_header_pair(file_paths[i], file_paths[j])) {
                relationships.push_back({file_paths[i], file_paths[j],
                    "source_header", 0.90, "Source/header pair"});
            } else if (are_numbered_sequence(file_paths[i], file_paths[j])) {
                relationships.push_back({file_paths[i], file_paths[j],
                    "numbered_sequence", 0.85, "Numbered sequence"});
            } else if (are_export_groups(file_paths[i], file_paths[j])) {
                relationships.push_back({file_paths[i], file_paths[j],
                    "export_group", 0.80, "Export group (same base, different format)"});
            } else if (are_same_name_variants(file_paths[i], file_paths[j])) {
                relationships.push_back({file_paths[i], file_paths[j],
                    "variant", 0.75, "Same-name variant (copy/backup)"});
            } else if (are_document_attachments(file_paths[i], file_paths[j])) {
                relationships.push_back({file_paths[i], file_paths[j],
                    "attachment", 0.65, "Document attachment relationship"});
            }
        }
    }

    return relationships;
}

std::vector<FileFamily> RelationshipEngine::group_into_families(
    const std::vector<std::string>& file_paths) const {

    auto rels = find_relationships(file_paths);

    std::map<std::string, int> file_to_group;
    int next_group = 0;

    for (const auto& r : rels) {
        auto a_it = file_to_group.find(r.file_a);
        auto b_it = file_to_group.find(r.file_b);

        if (a_it == file_to_group.end() && b_it == file_to_group.end()) {
            file_to_group[r.file_a] = next_group;
            file_to_group[r.file_b] = next_group;
            next_group++;
        } else if (a_it != file_to_group.end() && b_it == file_to_group.end()) {
            file_to_group[r.file_b] = a_it->second;
        } else if (a_it == file_to_group.end() && b_it != file_to_group.end()) {
            file_to_group[r.file_a] = b_it->second;
        } else if (a_it->second != b_it->second) {
            int old_group = b_it->second;
            int new_group = a_it->second;
            for (auto& [f, g] : file_to_group)
                if (g == old_group) g = new_group;
        }
    }

    std::map<int, FileFamily> groups;
    for (const auto& fp : file_paths) {
        auto it = file_to_group.find(fp);
        int gid = (it != file_to_group.end()) ? it->second : -1;
        if (gid < 0) continue;

        auto& fam = groups[gid];
        fam.members.push_back(fp);
        fam.name = fs::path(fp).stem().string();
    }

    std::vector<FileFamily> result;
    for (auto& [gid, fam] : groups) {
        if (fam.members.size() < 2) continue;

        std::string ext = extension_of(fam.members[0]);
        bool all_same_ext = true;
        for (const auto& m : fam.members)
            if (extension_of(m) != ext) { all_same_ext = false; break; }

        if (all_same_ext) {
            bool is_numbered = false;
            for (const auto& r : rels)
                if (r.relationship_type == "numbered_sequence") {
                    is_numbered = true;
                    break;
                }
            fam.family_type = is_numbered ? "numbered_sequence" : "same_type_group";
        } else {
            fam.family_type = "mixed_export_group";
        }

        result.push_back(std::move(fam));
    }

    return result;
}

}
}
