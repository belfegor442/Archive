#include "NoteService.h"

#include <random>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <limits>

#include "../core/utils/Uuid.h"

namespace archive::services {

NoteService::NoteService(
    storage::NoteRepository& notes,
    storage::ArchiveItemRepository& items,
    storage::ActivityRepository& activities
) : notes_(notes)
  , items_(items)
  , activities_(activities)
{}

core::Note NoteService::add(const std::string& item_id, const std::string& content) {
    core::Note note;
    note.id = core::utils::generate_id();
    note.item_id = item_id;
    note.content = content;
    note.created_at = core::utils::now_iso();
    note.updated_at = core::utils::now_iso();

    notes_.insert(note);

    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::NoteAdded;
    act.created_at = core::utils::now_iso();
    activities_.insert(act);

    return note;
}

void NoteService::update(const std::string& note_id, const std::string& content) {
    auto note_opt = notes_.find_by_note_id(note_id);
    if (!note_opt) return;
    auto& note = *note_opt;
    note.content = content;
    note.updated_at = core::utils::now_iso();
    notes_.update(note);
}

void NoteService::remove(const std::string& note_id, const std::string& item_id) {
    notes_.remove(note_id);

    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::NoteDeleted;
    act.created_at = core::utils::now_iso();
    activities_.insert(act);
}

std::vector<core::Note> NoteService::get_notes(const std::string& item_id) {
    return notes_.find_by_item(item_id);
}

} // namespace archive::services
