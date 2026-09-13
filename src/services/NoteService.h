#pragma once

#include <string>
#include <vector>

#include "../core/models/Note.h"
#include "../storage/NoteRepository.h"
#include "../storage/ArchiveItemRepository.h"
#include "../storage/ActivityRepository.h"

namespace archive::services {

class NoteService {
public:
    NoteService(
        storage::NoteRepository& notes,
        storage::ArchiveItemRepository& items,
        storage::ActivityRepository& activities
    );

    core::Note add(const std::string& item_id, const std::string& content);
    void update(const std::string& note_id, const std::string& content);
    void remove(const std::string& note_id, const std::string& item_id);
    std::vector<core::Note> get_notes(const std::string& item_id);

private:
    storage::NoteRepository& notes_;
    storage::ArchiveItemRepository& items_;
    storage::ActivityRepository& activities_;
};

} // namespace archive::services
