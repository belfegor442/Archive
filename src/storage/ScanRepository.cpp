#include "ScanRepository.h"
#include "../core/utils/Uuid.h"
#include "../core/enums/ScanStatus.h"

namespace archive::storage {

ScanRepository::ScanRepository(DatabaseManager& db)
    : db_(db)
{}

void ScanRepository::insert(const core::Scan& scan) {
    auto stmt = db_.prepare(R"(
        INSERT INTO scans (id, root_path, status, file_count, folder_count, started_at, completed_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, scan.id);
    stmt.bind_text(2, scan.root_path);
    stmt.bind_text(3, core::to_string(scan.status));
    stmt.bind_int(4, scan.file_count);
    stmt.bind_int(5, scan.folder_count);
    stmt.bind_text(6, scan.started_at);
    stmt.bind_text(7, scan.completed_at);
    stmt.step_done();
}

void ScanRepository::update(const core::Scan& scan) {
    auto stmt = db_.prepare(R"(
        UPDATE scans SET root_path=?, status=?, file_count=?, folder_count=?,
            started_at=?, completed_at=?
        WHERE id=?
    )");
    stmt.bind_text(1, scan.root_path);
    stmt.bind_text(2, core::to_string(scan.status));
    stmt.bind_int(3, scan.file_count);
    stmt.bind_int(4, scan.folder_count);
    stmt.bind_text(5, scan.started_at);
    stmt.bind_text(6, scan.completed_at);
    stmt.bind_text(7, scan.id);
    stmt.step_done();
}

std::optional<core::Scan> ScanRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare(R"(
        SELECT id, root_path, status, file_count, folder_count, started_at, completed_at
        FROM scans WHERE id=?
    )");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_scan(stmt);
    return std::nullopt;
}

std::vector<core::Scan> ScanRepository::find_all() {
    std::vector<core::Scan> scans;
    auto stmt = db_.prepare(R"(
        SELECT id, root_path, status, file_count, folder_count, started_at, completed_at
        FROM scans ORDER BY started_at DESC
    )");
    while (stmt.step()) {
        scans.push_back(read_scan(stmt));
    }
    return scans;
}

void ScanRepository::update_status(const std::string& id, core::ScanStatus status) {
    auto stmt = db_.prepare("UPDATE scans SET status=? WHERE id=?");
    stmt.bind_text(1, core::to_string(status));
    stmt.bind_text(2, id);
    stmt.step_done();
}

void ScanRepository::update_counts(const std::string& id, int files, int folders) {
    auto stmt = db_.prepare("UPDATE scans SET file_count=?, folder_count=? WHERE id=?");
    stmt.bind_int(1, files);
    stmt.bind_int(2, folders);
    stmt.bind_text(3, id);
    stmt.step_done();
}

void ScanRepository::set_completed(const std::string& id, int files, int folders) {
    auto stmt = db_.prepare(R"(
        UPDATE scans SET status='completed', file_count=?, folder_count=?, completed_at=?
        WHERE id=?
    )");
    stmt.bind_int(1, files);
    stmt.bind_int(2, folders);
    stmt.bind_text(3, core::utils::now_iso());
    stmt.bind_text(4, id);
    stmt.step_done();
}

core::Scan ScanRepository::read_scan(DatabaseManager::Statement& stmt) {
    core::Scan scan;
    scan.id = stmt.column_text(0);
    scan.root_path = stmt.column_text(1);
    scan.status = core::scan_status_from_string(stmt.column_text(2));
    scan.file_count = stmt.column_int(3);
    scan.folder_count = stmt.column_int(4);
    scan.started_at = stmt.column_text(5);
    scan.completed_at = stmt.column_text(6);
    return scan;
}

} // namespace archive::storage
