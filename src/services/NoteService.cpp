#include "NoteService.h"

#include <random>
#include <sstream>
#include <chrono>
#include <iomanip>

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
    note.id = generate_id();
    note.item_id = item_id;
    note.content = content;
    note.created_at = now_iso();
    note.updated_at = now_iso();

    notes_.insert(note);

    core::Activity act;
    act.id = generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::NoteAdded;
    act.created_at = now_iso();
    activities_.insert(act);

    return note;
}

void NoteService::update(const std::string& note_id, const std::string& content) {
    auto all_notes = notes_.find_by_item("");
    for (auto& n : all_notes) {
        if (n.id == note_id) {
            n.content = content;
            n.updated_at = now_iso();
            notes_.update(n);
            break;
        }
    }
}

void NoteService::remove(const std::string& note_id, const std::string& item_id) {
    notes_.remove(note_id);

    core::Activity act;
    act.id = generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::NoteDeleted;
    act.created_at = now_iso();
    activities_.insert(act);
}

std::vector<core::Note> NoteService::get_notes(const std::string& item_id) {
    return notes_.find_by_item(item_id);
}

std::string NoteService::generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(0, std::numeric_limits<uint64_t>::max());
    std::ostringstream oss;
    oss << std::hex << dis(gen) << dis(gen);
    std::string id = oss.str();
    id.resize(32);
    return id;
}

std::string NoteService::now_iso() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace archive::services
