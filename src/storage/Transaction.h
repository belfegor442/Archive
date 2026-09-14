#pragma once

#include "DatabaseManager.h"
#include <stdexcept>

namespace archive::storage {

class Transaction {
public:
    explicit Transaction(DatabaseManager& db)
        : db_(db)
        , committed_(false)
        , active_(true)
    {
        db_.begin_transaction();
    }

    ~Transaction() {
        if (active_ && !committed_) {
            try {
                db_.rollback();
            } catch (...) {}
        }
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    Transaction(Transaction&& other) noexcept
        : db_(other.db_)
        , committed_(other.committed_)
        , active_(other.active_)
    {
        other.active_ = false;
    }

    Transaction& operator=(Transaction&& other) noexcept {
        if (this != &other) {
            if (active_ && !committed_) {
                try { db_.rollback(); } catch (...) {}
            }
            committed_ = other.committed_;
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }

    void commit() {
        if (!active_) throw std::runtime_error("Transaction already moved or destroyed");
        if (committed_) throw std::runtime_error("Transaction already committed");
        db_.commit();
        committed_ = true;
        active_ = false;
    }

    void rollback() {
        if (!active_) return;
        if (committed_) return;
        db_.rollback();
        active_ = false;
    }

    bool is_active() const { return active_ && !committed_; }

private:
    DatabaseManager& db_;
    bool committed_;
    bool active_;
};

} // namespace archive::storage
