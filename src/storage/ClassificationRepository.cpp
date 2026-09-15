#include "ClassificationRepository.h"

namespace archive::storage {

ClassificationRepository::ClassificationRepository(DatabaseManager& db)
    : db_(db)
{}

void ClassificationRepository::insert(const core::Classification& cls) {
    auto stmt = db_.prepare(R"(
        INSERT INTO classifications (id, scan_item_id, taxonomy_path, confidence, reason)
        VALUES (?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, cls.id);
    stmt.bind_text(2, cls.scan_item_id);
    stmt.bind_text(3, cls.taxonomy_path);
    stmt.bind_int(4, static_cast<int>(cls.confidence * 100));
    stmt.bind_text(5, cls.reason);
    stmt.step_done();
}

void ClassificationRepository::insert_batch(const std::vector<core::Classification>& items) {
    if (items.empty()) return;

    db_.begin_transaction();
    auto stmt = db_.prepare(R"(
        INSERT INTO classifications (id, scan_item_id, taxonomy_path, confidence, reason)
        VALUES (?, ?, ?, ?, ?)
    )");

    for (const auto& cls : items) {
        stmt.bind_text(1, cls.id);
        stmt.bind_text(2, cls.scan_item_id);
        stmt.bind_text(3, cls.taxonomy_path);
        stmt.bind_int(4, static_cast<int>(cls.confidence * 100));
        stmt.bind_text(5, cls.reason);
        stmt.step_done();
        stmt.reset();
    }

    db_.commit();
}

std::vector<core::Classification> ClassificationRepository::find_by_scan_item(const std::string& item_id) {
    std::vector<core::Classification> results;
    auto stmt = db_.prepare(R"(
        SELECT id, scan_item_id, taxonomy_path, confidence, reason
        FROM classifications WHERE scan_item_id=? ORDER BY confidence DESC
    )");
    stmt.bind_text(1, item_id);
    while (stmt.step()) {
        results.push_back(read_classification(stmt));
    }
    return results;
}

std::vector<core::Classification> ClassificationRepository::find_by_scan(const std::string& scan_id) {
    std::vector<core::Classification> results;
    auto stmt = db_.prepare(R"(
        SELECT c.id, c.scan_item_id, c.taxonomy_path, c.confidence, c.reason
        FROM classifications c
        JOIN scan_items si ON c.scan_item_id = si.id
        WHERE si.scan_id=? ORDER BY c.confidence DESC
    )");
    stmt.bind_text(1, scan_id);
    while (stmt.step()) {
        results.push_back(read_classification(stmt));
    }
    return results;
}

std::optional<core::Classification> ClassificationRepository::find_best(const std::string& item_id) {
    auto stmt = db_.prepare(R"(
        SELECT id, scan_item_id, taxonomy_path, confidence, reason
        FROM classifications WHERE scan_item_id=? ORDER BY confidence DESC LIMIT 1
    )");
    stmt.bind_text(1, item_id);
    if (stmt.step()) return read_classification(stmt);
    return std::nullopt;
}

void ClassificationRepository::clear_scan(const std::string& scan_id) {
    auto stmt = db_.prepare(R"(
        DELETE FROM classifications WHERE scan_item_id IN
        (SELECT id FROM scan_items WHERE scan_id=?)
    )");
    stmt.bind_text(1, scan_id);
    stmt.step_done();
}

core::Classification ClassificationRepository::read_classification(DatabaseManager::Statement& stmt) {
    core::Classification cls;
    cls.id = stmt.column_text(0);
    cls.scan_item_id = stmt.column_text(1);
    cls.taxonomy_path = stmt.column_text(2);
    cls.confidence = static_cast<double>(stmt.column_int(3)) / 100.0;
    cls.reason = stmt.column_text(4);
    return cls;
}

} // namespace archive::storage
