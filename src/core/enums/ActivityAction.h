#pragma once

#include <string>
#include <stdexcept>

namespace archive::core {

enum class ActivityAction {
    Imported,
    Updated,
    Deleted,
    Restored,
    VersionCreated,
    VersionRestored,
    NoteAdded,
    NoteDeleted,
    CategoryChanged,
    TagAdded,
    TagRemoved,
    IntegrityVerified,
    IntegrityFailed
};

inline std::string to_string(ActivityAction action) {
    switch (action) {
        case ActivityAction::Imported:          return "Imported";
        case ActivityAction::Updated:           return "Updated";
        case ActivityAction::Deleted:           return "Deleted";
        case ActivityAction::Restored:          return "Restored";
        case ActivityAction::VersionCreated:    return "Version created";
        case ActivityAction::VersionRestored:   return "Version restored";
        case ActivityAction::NoteAdded:         return "Note added";
        case ActivityAction::NoteDeleted:       return "Note deleted";
        case ActivityAction::CategoryChanged:   return "Category changed";
        case ActivityAction::TagAdded:          return "Tag added";
        case ActivityAction::TagRemoved:        return "Tag removed";
        case ActivityAction::IntegrityVerified: return "Integrity verified";
        case ActivityAction::IntegrityFailed:   return "Integrity failed";
    }
    throw std::invalid_argument("Unknown ActivityAction");
}

inline ActivityAction activity_action_from_string(const std::string& s) {
    if (s == "Imported")          return ActivityAction::Imported;
    if (s == "Updated")           return ActivityAction::Updated;
    if (s == "Deleted")           return ActivityAction::Deleted;
    if (s == "Restored")          return ActivityAction::Restored;
    if (s == "Version created")   return ActivityAction::VersionCreated;
    if (s == "Version restored")  return ActivityAction::VersionRestored;
    if (s == "Note added")        return ActivityAction::NoteAdded;
    if (s == "Note deleted")      return ActivityAction::NoteDeleted;
    if (s == "Category changed")  return ActivityAction::CategoryChanged;
    if (s == "Tag added")         return ActivityAction::TagAdded;
    if (s == "Tag removed")       return ActivityAction::TagRemoved;
    if (s == "Integrity verified")return ActivityAction::IntegrityVerified;
    if (s == "Integrity failed")  return ActivityAction::IntegrityFailed;
    throw std::invalid_argument("Unknown ActivityAction: " + s);
}

} // namespace archive::core
