#pragma once

#include <cstdint>
#include <string>

namespace leschat {

struct Peer {
    std::string node_id;
    std::string callsign;
    std::string address;
    std::uint16_t http_port;
    std::string app_version;
    std::int64_t last_seen_ms;
};

}  // namespace leschat
