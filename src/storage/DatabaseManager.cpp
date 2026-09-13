#include "DatabaseManager.h"

#include <iostream>
#include <stdexcept>

namespace archive::storage {

DatabaseManager::DatabaseManager(const std::string& db_path)
    : db_path_(db_path)
{}

DatabaseManager::~DatabaseManager() {
    close();
}

void DatabaseManager::initialize() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = "Failed to open database: " + db_path_;
        if (db_) {
            err += " (" + std::string(sqlite3_errmsg(db_)) + ")";
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw std::runtime_error(err);
    }

    enable_wal();
    create_schema();
}

void DatabaseManager::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void DatabaseManager::execute(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string error = err ? err : "Unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + error);
    }
}

void DatabaseManager::begin_transaction() {
    execute("BEGIN TRANSACTION");
}

void DatabaseManager::commit() {
    execute("COMMIT");
}

void DatabaseManager::rollback() {
    execute("ROLLBACK");
}

void DatabaseManager::enable_wal() {
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA foreign_keys=ON");
}

void DatabaseManager::create_schema() {
    execute(R"(
        CREATE TABLE IF NOT EXISTS categories (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT DEFAULT '',
            color TEXT DEFAULT '#6366f1',
            icon TEXT DEFAULT '',
            parent_id TEXT REFERENCES categories(id),
            created_at TEXT NOT NULL DEFAULT (datetime('now'))
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS archive_items (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            type TEXT NOT NULL DEFAULT 'file',
            status TEXT NOT NULL DEFAULT 'archived',
            description TEXT DEFAULT '',
            original_path TEXT NOT NULL,
            storage_path TEXT NOT NULL,
            size INTEGER NOT NULL DEFAULT 0,
            file_count INTEGER DEFAULT 0,
            category_id TEXT REFERENCES categories(id),
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            archived_at TEXT NOT NULL DEFAULT (datetime('now')),
            last_modified_at TEXT NOT NULL DEFAULT (datetime('now')),
            checksum TEXT DEFAULT '',
            current_version INTEGER DEFAULT 1,
            is_favorite INTEGER DEFAULT 0
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS tags (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL UNIQUE,
            color TEXT DEFAULT '#6366f1'
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS item_tags (
            item_id TEXT NOT NULL REFERENCES archive_items(id) ON DELETE CASCADE,
            tag_id TEXT NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
            PRIMARY KEY (item_id, tag_id)
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS versions (
            id TEXT PRIMARY KEY,
            item_id TEXT NOT NULL REFERENCES archive_items(id) ON DELETE CASCADE,
            version_number INTEGER NOT NULL,
            storage_path TEXT NOT NULL,
            checksum TEXT DEFAULT '',
            size INTEGER NOT NULL DEFAULT 0,
            notes TEXT DEFAULT '',
            created_at TEXT NOT NULL DEFAULT (datetime('now'))
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS notes (
            id TEXT PRIMARY KEY,
            item_id TEXT NOT NULL REFERENCES archive_items(id) ON DELETE CASCADE,
            content TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at TEXT NOT NULL DEFAULT (datetime('now'))
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS activity_log (
            id TEXT PRIMARY KEY,
            item_id TEXT NOT NULL REFERENCES archive_items(id) ON DELETE CASCADE,
            action TEXT NOT NULL,
            details TEXT DEFAULT '',
            created_at TEXT NOT NULL DEFAULT (datetime('now'))
        )
    )");

    execute("CREATE INDEX IF NOT EXISTS idx_items_status ON archive_items(status)");
    execute("CREATE INDEX IF NOT EXISTS idx_items_type ON archive_items(type)");
    execute("CREATE INDEX IF NOT EXISTS idx_items_category ON archive_items(category_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_items_favorite ON archive_items(is_favorite)");
    execute("CREATE INDEX IF NOT EXISTS idx_versions_item ON versions(item_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_notes_item ON notes(item_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_activity_item ON activity_log(item_id)");
}

DatabaseManager::Statement::Statement(sqlite3* db, const std::string& sql) {
    int rc = sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
}

DatabaseManager::Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

void DatabaseManager::Statement::bind_int(int index, int value) {
    sqlite3_bind_int(stmt_, index, value);
}

void DatabaseManager::Statement::bind_int64(int index, int64_t value) {
    sqlite3_bind_int64(stmt_, index, value);
}

void DatabaseManager::Statement::bind_text(int index, const std::string& value) {
    sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void DatabaseManager::Statement::bind_text_null(int index) {
    sqlite3_bind_null(stmt_, index);
}

void DatabaseManager::Statement::bind_blob(int index, const void* data, int size) {
    sqlite3_bind_blob(stmt_, index, data, size, SQLITE_TRANSIENT);
}

bool DatabaseManager::Statement::step() {
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_ROW;
}

int DatabaseManager::Statement::column_int(int col) const {
    return sqlite3_column_int(stmt_, col);
}

int64_t DatabaseManager::Statement::column_int64(int col) const {
    return sqlite3_column_int64(stmt_, col);
}

std::string DatabaseManager::Statement::column_text(int col) const {
    const unsigned char* text = sqlite3_column_text(stmt_, col);
    return text ? std::string(reinterpret_cast<const char*>(text)) : "";
}

bool DatabaseManager::Statement::column_is_null(int col) const {
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
}

const void* DatabaseManager::Statement::column_blob(int col) const {
    return sqlite3_column_blob(stmt_, col);
}

int DatabaseManager::Statement::column_bytes(int col) const {
    return sqlite3_column_bytes(stmt_, col);
}

void DatabaseManager::Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

DatabaseManager::Statement DatabaseManager::prepare(const std::string& sql) {
    return Statement(db_, sql);
}

} // namespace archive::storage
