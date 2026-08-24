#include "leschat/message_store.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace leschat {

namespace {

class DatabaseFull final : public std::runtime_error {
public:
    DatabaseFull() : std::runtime_error("SQLite database reached 10 MiB") {}
};

class Statement {
public:
    Statement(sqlite3* database, const char* sql) {
        if (sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr) !=
            SQLITE_OK) {
            throw std::runtime_error(
                std::string{"SQLite prepare failed: "} +
                sqlite3_errmsg(database)
            );
        }
    }

    ~Statement() { sqlite3_finalize(statement_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    [[nodiscard]] sqlite3_stmt* get() const noexcept { return statement_; }

private:
    sqlite3_stmt* statement_{nullptr};
};

void bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
    if (sqlite3_bind_text(
            statement, index, value.data(),
            static_cast<int>(value.size()), SQLITE_TRANSIENT
        ) != SQLITE_OK) {
        throw std::runtime_error("SQLite text binding failed");
    }
}

std::string column_text(sqlite3_stmt* statement, int column) {
    const auto* text = sqlite3_column_text(statement, column);
    const int bytes = sqlite3_column_bytes(statement, column);
    if (text == nullptr || bytes < 0) {
        return {};
    }
    return {reinterpret_cast<const char*>(text),
            static_cast<std::size_t>(bytes)};
}

}  // namespace

MessageStore::MessageStore(
    std::string database_path,
    std::uint64_t max_bytes
) {
    if (database_path.empty() || max_bytes == 0U) {
        throw std::invalid_argument(
            "Database path and size limit must be provided"
        );
    }
    if (sqlite3_open_v2(
            database_path.c_str(), &database_,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr
        ) != SQLITE_OK) {
        const std::string error = database_ == nullptr
            ? "unknown SQLite error" : sqlite3_errmsg(database_);
        if (database_ != nullptr) {
            sqlite3_close(database_);
            database_ = nullptr;
        }
        throw std::runtime_error("Unable to open message database: " + error);
    }

    try {
        sqlite3_busy_timeout(database_, 2000);
        initialize_schema(max_bytes);
        load_messages();
    } catch (...) {
        sqlite3_close(database_);
        database_ = nullptr;
        throw;
    }
}

MessageStore::~MessageStore() {
    if (database_ != nullptr) {
        sqlite3_close(database_);
    }
}

void MessageStore::execute(const std::string& sql) {
    char* error = nullptr;
    const int result = sqlite3_exec(
        database_, sql.c_str(), nullptr, nullptr, &error
    );
    if (result != SQLITE_OK) {
        const std::string message = error == nullptr
            ? sqlite3_errmsg(database_) : error;
        sqlite3_free(error);
        if (result == SQLITE_FULL) {
            throw DatabaseFull{};
        }
        throw std::runtime_error("SQLite operation failed: " + message);
    }
}

void MessageStore::initialize_schema(std::uint64_t max_bytes) {
    execute("PRAGMA journal_mode=DELETE");
    execute("PRAGMA synchronous=NORMAL");
    execute("PRAGMA foreign_keys=ON");

    Statement page_size_query{database_, "PRAGMA page_size"};
    if (sqlite3_step(page_size_query.get()) != SQLITE_ROW) {
        throw std::runtime_error("Unable to read SQLite page size");
    }
    const std::int64_t page_size =
        sqlite3_column_int64(page_size_query.get(), 0);
    if (page_size <= 0) {
        throw std::runtime_error("SQLite returned an invalid page size");
    }
    const std::uint64_t max_pages = std::max<std::uint64_t>(
        1U, max_bytes / static_cast<std::uint64_t>(page_size)
    );
    execute("PRAGMA max_page_count=" + std::to_string(max_pages));

    execute(
        "CREATE TABLE IF NOT EXISTS messages ("
        "storage_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "origin TEXT NOT NULL, sequence INTEGER NOT NULL,"
        "message_id TEXT NOT NULL UNIQUE, callsign TEXT NOT NULL,"
        "channel TEXT NOT NULL, created_at_ms INTEGER NOT NULL,"
        "body TEXT NOT NULL, UNIQUE(origin, sequence))"
    );
    execute(
        "CREATE TABLE IF NOT EXISTS sequences ("
        "origin TEXT PRIMARY KEY, last_sequence INTEGER NOT NULL)"
    );
}

void MessageStore::load_messages() {
    messages_.clear();
    Statement query{
        database_,
        "SELECT sequence, message_id, origin, callsign, channel, "
        "created_at_ms, body FROM messages ORDER BY storage_id"
    };
    for (;;) {
        const int result = sqlite3_step(query.get());
        if (result == SQLITE_DONE) {
            return;
        }
        if (result != SQLITE_ROW) {
            throw std::runtime_error(
                std::string{"Unable to load messages: "} +
                sqlite3_errmsg(database_)
            );
        }
        const std::int64_t sequence = sqlite3_column_int64(query.get(), 0);
        if (sequence < 1) {
            throw std::runtime_error("Database contains an invalid sequence");
        }
        messages_.push_back(ChatMessage{
            .sequence = static_cast<std::uint64_t>(sequence),
            .message_id = column_text(query.get(), 1),
            .origin = column_text(query.get(), 2),
            .callsign = column_text(query.get(), 3),
            .channel = column_text(query.get(), 4),
            .created_at_ms = sqlite3_column_int64(query.get(), 5),
            .body = column_text(query.get(), 6)
        });
    }
}

bool MessageStore::contains(const std::string& message_id) const {
    Statement query{
        database_, "SELECT 1 FROM messages WHERE message_id=?1 LIMIT 1"
    };
    bind_text(query.get(), 1, message_id);
    return sqlite3_step(query.get()) == SQLITE_ROW;
}

bool MessageStore::insert_message(const ChatMessage& message) {
    execute("BEGIN IMMEDIATE");
    try {
        if (message.sequence > static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max())) {
            throw std::runtime_error("Message sequence exceeds SQLite range");
        }
        Statement insert{
            database_,
            "INSERT OR IGNORE INTO messages "
            "(origin, sequence, message_id, callsign, channel, "
            "created_at_ms, body) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)"
        };
        bind_text(insert.get(), 1, message.origin);
        sqlite3_bind_int64(insert.get(), 2,
                           static_cast<std::int64_t>(message.sequence));
        bind_text(insert.get(), 3, message.message_id);
        bind_text(insert.get(), 4, message.callsign);
        bind_text(insert.get(), 5, message.channel);
        sqlite3_bind_int64(insert.get(), 6, message.created_at_ms);
        bind_text(insert.get(), 7, message.body);
        const int insert_result = sqlite3_step(insert.get());
        if (insert_result == SQLITE_FULL) {
            throw DatabaseFull{};
        }
        if (insert_result != SQLITE_DONE) {
            throw std::runtime_error(
                std::string{"Unable to store message: "} +
                sqlite3_errmsg(database_)
            );
        }
        const bool inserted = sqlite3_changes(database_) != 0;
        if (inserted) {
            Statement update{
                database_,
                "INSERT INTO sequences(origin, last_sequence) VALUES(?1, ?2) "
                "ON CONFLICT(origin) DO UPDATE SET last_sequence="
                "MAX(last_sequence, excluded.last_sequence)"
            };
            bind_text(update.get(), 1, message.origin);
            sqlite3_bind_int64(update.get(), 2,
                               static_cast<std::int64_t>(message.sequence));
            const int update_result = sqlite3_step(update.get());
            if (update_result == SQLITE_FULL) {
                throw DatabaseFull{};
            }
            if (update_result != SQLITE_DONE) {
                throw std::runtime_error("Unable to update message sequence");
            }
        }
        execute("COMMIT");
        return inserted;
    } catch (...) {
        try { execute("ROLLBACK"); } catch (...) {}
        throw;
    }
}

void MessageStore::prune_oldest_messages() {
    execute(
        "DELETE FROM messages WHERE storage_id IN ("
        "SELECT storage_id FROM messages ORDER BY storage_id LIMIT 100)"
    );
    load_messages();
}

bool MessageStore::add(ChatMessage message) {
    try {
        const bool inserted = insert_message(message);
        if (inserted) {
            messages_.push_back(std::move(message));
        }
        return inserted;
    } catch (const DatabaseFull&) {
    }

    prune_oldest_messages();
    const bool inserted = insert_message(message);
    if (inserted) {
        messages_.push_back(std::move(message));
    }
    return inserted;
}

const std::vector<ChatMessage>& MessageStore::messages() const noexcept {
    return messages_;
}

std::uint64_t MessageStore::next_sequence(const std::string& origin) const {
    Statement query{
        database_, "SELECT last_sequence FROM sequences WHERE origin=?1"
    };
    bind_text(query.get(), 1, origin);
    if (sqlite3_step(query.get()) != SQLITE_ROW) {
        return 1;
    }
    const std::int64_t last = sqlite3_column_int64(query.get(), 0);
    if (last < 0 || last == std::numeric_limits<std::int64_t>::max()) {
        throw std::runtime_error("Stored message sequence is invalid");
    }
    return static_cast<std::uint64_t>(last) + 1U;
}

std::unordered_map<std::string, std::uint64_t>
MessageStore::sequence_summary() const {
    std::unordered_map<std::string, std::uint64_t> summary;
    Statement query{
        database_, "SELECT origin, last_sequence FROM sequences"
    };
    for (;;) {
        const int result = sqlite3_step(query.get());
        if (result == SQLITE_DONE) {
            return summary;
        }
        if (result != SQLITE_ROW) {
            throw std::runtime_error("Unable to read sequence summary");
        }
        const std::int64_t sequence = sqlite3_column_int64(query.get(), 1);
        if (sequence < 0) {
            throw std::runtime_error("Stored sequence summary is invalid");
        }
        summary.emplace(
            column_text(query.get(), 0),
            static_cast<std::uint64_t>(sequence)
        );
    }
}

}  // namespace leschat
