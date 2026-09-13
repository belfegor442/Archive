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

    void initialize();
    void close();

    sqlite3* handle() const { return db_; }

    void execute(const std::string& sql);
    void begin_transaction();
    void commit();
    void rollback();

    class Statement {
    public:
        Statement(sqlite3* db, const std::string& sql);
        ~Statement();

        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;

        void bind_int(int index, int value);
        void bind_int64(int index, int64_t value);
        void bind_text(int index, const std::string& value);
        void bind_text_null(int index);
        void bind_blob(int index, const void* data, int size);

        bool step();
        bool step_done();
        int column_int(int col) const;
        int64_t column_int64(int col) const;
        std::string column_text(int col) const;
        bool column_is_null(int col) const;
        const void* column_blob(int col) const;
        int column_bytes(int col) const;

        void reset();

    private:
        sqlite3_stmt* stmt_ = nullptr;
    };

    Statement prepare(const std::string& sql);

private:
    sqlite3* db_ = nullptr;
    std::string db_path_;

    void create_schema();
    void enable_wal();
};

} // namespace archive::storage
