#pragma once

#include <string>
#include <vector>

#include "../core/models/Note.h"
#include "DatabaseManager.h"

namespace archive::storage {

class NoteRepository {
public:
    explicit NoteRepository(DatabaseManager& db);

    std::string insert(const core::Note& note);
    void update(const core::Note& note);
    void remove(const std::string& id);
    std::vector<core::Note> find_by_item(const std::string& item_id);

private:
    DatabaseManager& db_;
    core::Note read_note(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
