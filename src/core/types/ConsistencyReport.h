#pragma once

#include <string>
#include <vector>

namespace archive::core {

struct ConsistencyIssue {
    enum class Severity { Error, Warning };
    enum class Kind {
        ItemWithoutVersion,
        VersionWithoutStoredObject,
        StoredObjectWithoutFile,
        FileWithoutStoredObject,
        ChecksumMismatch,
        SizeMismatch,
        StoragePathMissing,
        OrphanStaging
    };

    Severity severity = Severity::Error;
    Kind kind;
    std::string entity_id;
    std::string details;

    ConsistencyIssue() = default;
};

struct ConsistencyReport {
    std::vector<ConsistencyIssue> issues;
    int error_count = 0;
    int warning_count = 0;
    int orphan_files = 0;
    int missing_objects = 0;
    int invalid_checksums = 0;
    int broken_relations = 0;

    ConsistencyReport() = default;

    bool is_clean() const { return error_count == 0; }

    void add_issue(ConsistencyIssue issue) {
        issues.push_back(std::move(issue));
        const auto& back = issues.back();
        if (back.severity == ConsistencyIssue::Severity::Error) error_count++;
        else warning_count++;

        switch (back.kind) {
            case ConsistencyIssue::Kind::FileWithoutStoredObject: orphan_files++; break;
            case ConsistencyIssue::Kind::StoredObjectWithoutFile: missing_objects++; break;
            case ConsistencyIssue::Kind::ChecksumMismatch: invalid_checksums++; break;
            case ConsistencyIssue::Kind::ItemWithoutVersion:
            case ConsistencyIssue::Kind::VersionWithoutStoredObject:
                broken_relations++; break;
            default: break;
        }
    }
};

} // namespace archive::core
