#pragma once
#include <string>
#include <vector>
#include <map>

namespace archive {
namespace services {

struct FileRelationship {
    std::string file_a;
    std::string file_b;
    std::string relationship_type;
    double strength = 0.0;
    std::string reason;
};

struct FileFamily {
    std::string name;
    std::vector<std::string> members;
    std::string family_type;
    std::string suggested_category;
};

class RelationshipEngine {
public:
    std::vector<FileRelationship> find_relationships(
        const std::vector<std::string>& file_paths) const;

    std::vector<FileFamily> group_into_families(
        const std::vector<std::string>& file_paths) const;

private:
    bool are_source_header_pair(const std::string& a, const std::string& b) const;
    bool are_numbered_sequence(const std::string& a, const std::string& b) const;
    bool are_same_name_variants(const std::string& a, const std::string& b) const;
    bool are_export_groups(const std::string& a, const std::string& b) const;
    bool are_document_attachments(const std::string& a, const std::string& b) const;
    bool are_project_files(const std::string& a, const std::string& b) const;

    std::string base_name(const std::string& path) const;
    std::string extension_of(const std::string& path) const;
};

}
}
