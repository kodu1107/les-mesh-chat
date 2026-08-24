#include "leschat/message_store.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {

constexpr std::uint64_t test_database_limit = 10U * 1024U * 1024U;

leschat::ChatMessage make_message(std::uint64_t sequence) {
    return leschat::ChatMessage{
        .sequence = sequence,
        .message_id = "node-a:" + std::to_string(sequence),
        .origin = "node-a",
        .callsign = "Bolt",
        .channel = "les-manet",
        .created_at_ms = static_cast<std::int64_t>(sequence),
        .body = "영구 저장 시험"
    };
}

}  // namespace

int main() {
    const std::filesystem::path database_path =
        std::filesystem::temp_directory_path() /
        "les-chat-message-store-test.db";
    std::filesystem::remove(database_path);

    {
        leschat::MessageStore store{
            database_path.string(), test_database_limit
        };
        assert(store.add(make_message(1)));
        assert(!store.add(make_message(1)));
        assert(store.messages().size() == 1U);
        assert(store.next_sequence("node-a") == 2U);
    }

    {
        leschat::MessageStore reopened{
            database_path.string(), test_database_limit
        };
        assert(reopened.messages().size() == 1U);
        assert(reopened.contains("node-a:1"));
        assert(reopened.next_sequence("node-a") == 2U);
        assert(reopened.add(make_message(2)));
    }

    assert(std::filesystem::file_size(database_path) <= test_database_limit);
    std::filesystem::remove(database_path);

    const std::filesystem::path bounded_path =
        std::filesystem::temp_directory_path() /
        "les-chat-message-store-limit-test.db";
    std::filesystem::remove(bounded_path);
    {
        leschat::MessageStore store{
            bounded_path.string(), test_database_limit
        };
        leschat::ChatMessage message = make_message(1);
        message.body.assign(2048U, 'x');
        for (std::uint64_t sequence = 1; sequence <= 6000U; ++sequence) {
            message.sequence = sequence;
            message.message_id = "node-a:" + std::to_string(sequence);
            assert(store.add(message));
        }
    }
    assert(
        std::filesystem::file_size(bounded_path) <= test_database_limit
    );
    std::filesystem::remove(bounded_path);
}
