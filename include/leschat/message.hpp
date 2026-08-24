#pragma once

#include <cstdint>
#include <string>

namespace leschat {

struct ChatMessage {
    std::uint64_t sequence;
    std::string message_id;
    std::string origin;
    std::string callsign;
    std::string channel;
    std::int64_t created_at_ms;
    std::string body;
};

}  // namespace leschat
