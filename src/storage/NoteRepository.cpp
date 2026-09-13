#include "NoteRepository.h"

namespace archive::storage {

NoteRepository::NoteRepository(DatabaseManager& db)
    : db_(db)
{}

std::string NoteRepository::insert(const core::Note& note) {
    auto stmt = db_.prepare(R"(
        INSERT INTO notes (id, item_id, content, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, note.id);
    stmt.bind_text(2, note.item_id);
    stmt.bind_text(3, note.content);
    stmt.bind_text(4, note.created_at);
    stmt.bind_text(5, note.updated_at);
    stmt.step_done();
    return note.id;
}

void NoteRepository::update(const core::Note& note) {
    auto stmt = db_.prepare("UPDATE notes SET content=?, updated_at=? WHERE id=?");
    stmt.bind_text(1, note.content);
    stmt.bind_text(2, note.updated_at);
    stmt.bind_text(3, note.id);
    stmt.step_done();
}

void NoteRepository::remove(const std::string& id) {
    auto stmt = db_.prepare("DELETE FROM notes WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step_done();
}

std::vector<core::Note> NoteRepository::find_by_item(const std::string& item_id) {
    std::vector<core::Note> notes;
    auto stmt = db_.prepare("SELECT * FROM notes WHERE item_id=? ORDER BY created_at DESC");
    stmt.bind_text(1, item_id);
    while (stmt.step()) {
        notes.push_back(read_note(stmt));
    }
    return notes;
}

core::Note NoteRepository::read_note(DatabaseManager::Statement& stmt) {
    core::Note note;
    note.id = stmt.column_text(0);
    note.item_id = stmt.column_text(1);
    note.content = stmt.column_text(2);
    note.created_at = stmt.column_text(3);
    note.updated_at = stmt.column_text(4);
    return note;
}

} // namespace archive::storage
