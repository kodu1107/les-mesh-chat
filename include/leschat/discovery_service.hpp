#pragma once

#include "leschat/node_identity.hpp"

#include <cstdint>
#include <string>

struct event;
struct event_base;

namespace leschat {

class PeerRegistry;

class DiscoveryService {
public:
    DiscoveryService(
        event_base* event_base,
        PeerRegistry& registry,
        NodeIdentity identity,
        std::uint16_t http_port,
        std::string discovery_address,
        std::uint16_t discovery_port
    );
    ~DiscoveryService();

    DiscoveryService(const DiscoveryService&) = delete;
    DiscoveryService& operator=(const DiscoveryService&) = delete;
    DiscoveryService(DiscoveryService&&) = delete;
    DiscoveryService& operator=(DiscoveryService&&) = delete;

private:
    static void receive_callback(
        int socket,
        short events,
        void* context
    ) noexcept;
    static void announce_callback(
        int socket,
        short events,
        void* context
    ) noexcept;

    void receive_announcements();
    void send_announce();
    void close_resources() noexcept;

    PeerRegistry& registry_;
    NodeIdentity identity_;
    std::uint16_t http_port_;
    std::string discovery_address_;
    std::uint16_t discovery_port_;
    int socket_{-1};
    event* receive_event_{nullptr};
    event* announce_event_{nullptr};
};

}  // namespace leschat
