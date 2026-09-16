#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <sqlite3.h>

namespace archive::storage {

class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&& other) noexcept;
    DatabaseManager& operator=(DatabaseManager&& other) noexcept;

    void initialize();
    void close();
    bool is_open() const { return db_ != nullptr; }

    void execute(const std::string& sql);
    void begin_transaction();
    void commit();
    void rollback();
    bool in_transaction() const { return txn_depth_ > 0; }

    class Statement {
    public:
        Statement(sqlite3* db, const std::string& sql);
        ~Statement();

        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;
        Statement(Statement&& other) noexcept;
        Statement& operator=(Statement&& other) noexcept;

        void bind_int(int index, int value);
        void bind_int64(int index, int64_t value);
        void bind_text(int index, const std::string& value);
        void bind_text_null(int index);
        void bind_blob(int index, const void* data, int size);

        bool step();
        bool step_done();
        int last_step_rc() const { return last_rc_; }
        int column_int(int col) const;
        int64_t column_int64(int col) const;
        std::string column_text(int col) const;
        bool column_is_null(int col) const;
        const void* column_blob(int col) const;
        int column_bytes(int col) const;

        void reset();

    private:
        sqlite3_stmt* stmt_ = nullptr;
        int last_rc_ = SQLITE_OK;
    };

    [[nodiscard]] Statement prepare(const std::string& sql) const;

    static std::string rc_to_string(int rc);

private:
    sqlite3* db_ = nullptr;
    std::string db_path_;
    int txn_depth_ = 0;

    void create_schema();
    void enable_wal();
    void check_rc(int rc, const std::string& context);
};

} // namespace archive::storage
