#pragma once

#include "leschat/message.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;

namespace leschat {

class MessageStore {
public:
    MessageStore(std::string database_path, std::uint64_t max_bytes);
    ~MessageStore();

    MessageStore(const MessageStore&) = delete;
    MessageStore& operator=(const MessageStore&) = delete;
    MessageStore(MessageStore&&) = delete;
    MessageStore& operator=(MessageStore&&) = delete;

    [[nodiscard]] bool contains(const std::string& message_id) const;
    [[nodiscard]] bool add(ChatMessage message);
    [[nodiscard]] const std::vector<ChatMessage>& messages() const noexcept;
    [[nodiscard]] std::uint64_t next_sequence(
        const std::string& origin
    ) const;
    [[nodiscard]] std::unordered_map<std::string, std::uint64_t>
    sequence_summary() const;

private:
    void execute(const std::string& sql);
    void initialize_schema(std::uint64_t max_bytes);
    void load_messages();
    [[nodiscard]] bool insert_message(const ChatMessage& message);
    void prune_oldest_messages();

    sqlite3* database_{nullptr};
    std::vector<ChatMessage> messages_;
};

}  // namespace leschat
