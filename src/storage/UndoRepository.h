#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/UndoRecord.h"
#include "../core/models/UndoEntry.h"
#include "DatabaseManager.h"

namespace archive::storage {

class UndoRepository {
public:
    explicit UndoRepository(DatabaseManager& db);

    void insert_record(const core::UndoRecord& record);
    void insert_entries(const std::vector<core::UndoEntry>& entries);
    void update_status(const std::string& id, core::UndoStatus status);
    std::optional<core::UndoRecord> find_record_by_id(const std::string& id);
    std::vector<core::UndoRecord> find_available();
    std::vector<core::UndoEntry> find_entries(const std::string& undo_id);
    std::vector<core::UndoRecord> find_all();

private:
    DatabaseManager& db_;
    core::UndoRecord read_record(DatabaseManager::Statement& stmt);
    core::UndoEntry read_entry(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
