#include "DatabaseManager.h"

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

    sqlite3_busy_timeout(db_, 5000);
    enable_wal();
    create_schema();
}

void DatabaseManager::close() {
    if (db_) {
        if (txn_depth_ > 0) {
            char* err = nullptr;
            int rc = sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &err);
            if (rc != SQLITE_OK && err) sqlite3_free(err);
            txn_depth_ = 0;
        }
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void DatabaseManager::execute(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string error = err ? err : "Unknown error";
        int code = rc;
        sqlite3_free(err);
        throw std::runtime_error(
            "SQL error (rc=" + std::to_string(code) + "): " + error
            + " [SQL: " + sql.substr(0, 120) + "]"
        );
    }
}

void DatabaseManager::begin_transaction() {
    if (txn_depth_ == 0) {
        execute("BEGIN IMMEDIATE");
    }
    txn_depth_++;
}

void DatabaseManager::commit() {
    if (txn_depth_ <= 0) {
        throw std::runtime_error("commit() called without active transaction");
    }
    txn_depth_--;
    if (txn_depth_ == 0) {
        execute("COMMIT");
    }
}

void DatabaseManager::rollback() {
    if (txn_depth_ <= 0) {
        throw std::runtime_error("rollback() called without active transaction");
    }
    txn_depth_ = 0;
    execute("ROLLBACK");
}

void DatabaseManager::enable_wal() {
    if (db_path_ != ":memory:") {
        execute("PRAGMA journal_mode=WAL");
    }
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
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            UNIQUE(item_id, version_number)
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

    execute(R"(
        CREATE TABLE IF NOT EXISTS stored_objects (
            id TEXT PRIMARY KEY,
            item_id TEXT NOT NULL REFERENCES archive_items(id) ON DELETE CASCADE,
            version_id TEXT,
            storage_path TEXT NOT NULL,
            size INTEGER NOT NULL DEFAULT 0,
            checksum TEXT DEFAULT '',
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            UNIQUE(version_id)
        )
    )");

    execute("CREATE INDEX IF NOT EXISTS idx_items_status ON archive_items(status)");
    execute("CREATE INDEX IF NOT EXISTS idx_items_type ON archive_items(type)");
    execute("CREATE INDEX IF NOT EXISTS idx_items_category ON archive_items(category_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_items_favorite ON archive_items(is_favorite)");
    execute("CREATE INDEX IF NOT EXISTS idx_versions_item ON versions(item_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_notes_item ON notes(item_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_activity_item ON activity_log(item_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_stored_objects_item ON stored_objects(item_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_stored_objects_version ON stored_objects(version_id)");
}

DatabaseManager::Statement::Statement(sqlite3* db, const std::string& sql) {
    int rc = sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(
            "Failed to prepare statement (rc=" + std::to_string(rc) + "): "
            + std::string(sqlite3_errmsg(db))
            + " [SQL: " + sql.substr(0, 120) + "]"
        );
    }
}

DatabaseManager::Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

DatabaseManager::Statement::Statement(Statement&& other) noexcept
    : stmt_(other.stmt_)
    , last_rc_(other.last_rc_)
{
    other.stmt_ = nullptr;
    other.last_rc_ = SQLITE_OK;
}

DatabaseManager::Statement& DatabaseManager::Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_) sqlite3_finalize(stmt_);
        stmt_ = other.stmt_;
        last_rc_ = other.last_rc_;
        other.stmt_ = nullptr;
        other.last_rc_ = SQLITE_OK;
    }
    return *this;
}

void DatabaseManager::Statement::bind_int(int index, int value) {
    int rc = sqlite3_bind_int(stmt_, index, value);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("bind_int failed (rc=" + std::to_string(rc) + ")");
    }
}

void DatabaseManager::Statement::bind_int64(int index, int64_t value) {
    int rc = sqlite3_bind_int64(stmt_, index, value);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("bind_int64 failed (rc=" + std::to_string(rc) + ")");
    }
}

void DatabaseManager::Statement::bind_text(int index, const std::string& value) {
    int rc = sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("bind_text failed (rc=" + std::to_string(rc) + ")");
    }
}

void DatabaseManager::Statement::bind_text_null(int index) {
    int rc = sqlite3_bind_null(stmt_, index);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("bind_text_null failed (rc=" + std::to_string(rc) + ")");
    }
}

void DatabaseManager::Statement::bind_blob(int index, const void* data, int size) {
    int rc = sqlite3_bind_blob(stmt_, index, data, size, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("bind_blob failed (rc=" + std::to_string(rc) + ")");
    }
}

bool DatabaseManager::Statement::step() {
    last_rc_ = sqlite3_step(stmt_);
    if (last_rc_ == SQLITE_ROW) return true;
    if (last_rc_ == SQLITE_DONE) return false;
    throw std::runtime_error(
        "step() failed (rc=" + std::to_string(last_rc_) + "): "
        + std::string(sqlite3_errmsg(sqlite3_db_handle(stmt_)))
    );
}

bool DatabaseManager::Statement::step_done() {
    last_rc_ = sqlite3_step(stmt_);
    if (last_rc_ == SQLITE_DONE) return true;
    if (last_rc_ == SQLITE_ROW) return false;
    throw std::runtime_error(
        "step_done() failed (rc=" + std::to_string(last_rc_) + "): "
        + std::string(sqlite3_errmsg(sqlite3_db_handle(stmt_)))
    );
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

DatabaseManager::Statement DatabaseManager::prepare(const std::string& sql) const {
    return Statement(db_, sql);
}

void DatabaseManager::check_rc(int rc, const std::string& context) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error(
            context + " failed (rc=" + std::to_string(rc) + "): "
            + std::string(sqlite3_errmsg(db_))
        );
    }
}

std::string DatabaseManager::rc_to_string(int rc) {
    switch (rc) {
        case SQLITE_OK:         return "SQLITE_OK";
        case SQLITE_ERROR:      return "SQLITE_ERROR";
        case SQLITE_INTERNAL:   return "SQLITE_INTERNAL";
        case SQLITE_PERM:       return "SQLITE_PERM";
        case SQLITE_ABORT:      return "SQLITE_ABORT";
        case SQLITE_BUSY:       return "SQLITE_BUSY";
        case SQLITE_LOCKED:     return "SQLITE_LOCKED";
        case SQLITE_NOMEM:      return "SQLITE_NOMEM";
        case SQLITE_READONLY:   return "SQLITE_READONLY";
        case SQLITE_INTERRUPT:  return "SQLITE_INTERRUPT";
        case SQLITE_IOERR:      return "SQLITE_IOERR";
        case SQLITE_CORRUPT:    return "SQLITE_CORRUPT";
        case SQLITE_NOTFOUND:   return "SQLITE_NOTFOUND";
        case SQLITE_FULL:       return "SQLITE_FULL";
        case SQLITE_CANTOPEN:   return "SQLITE_CANTOPEN";
        case SQLITE_PROTOCOL:   return "SQLITE_PROTOCOL";
        case SQLITE_EMPTY:      return "SQLITE_EMPTY";
        case SQLITE_SCHEMA:     return "SQLITE_SCHEMA";
        case SQLITE_TOOBIG:     return "SQLITE_TOOBIG";
        case SQLITE_CONSTRAINT: return "SQLITE_CONSTRAINT";
        case SQLITE_MISMATCH:   return "SQLITE_MISMATCH";
        case SQLITE_MISUSE:     return "SQLITE_MISUSE";
        case SQLITE_NOLFS:      return "SQLITE_NOLFS";
        case SQLITE_AUTH:       return "SQLITE_AUTH";
        case SQLITE_FORMAT:     return "SQLITE_FORMAT";
        case SQLITE_RANGE:      return "SQLITE_RANGE";
        case SQLITE_NOTADB:     return "SQLITE_NOTADB";
        case SQLITE_NOTICE:     return "SQLITE_NOTICE";
        case SQLITE_WARNING:    return "SQLITE_WARNING";
        case SQLITE_ROW:        return "SQLITE_ROW";
        case SQLITE_DONE:       return "SQLITE_DONE";
        default:                return "UNKNOWN(" + std::to_string(rc) + ")";
    }
}

} // namespace archive::storage
