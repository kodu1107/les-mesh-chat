#pragma once

#include "leschat/node_identity.hpp"

#include <cstdint>
#include <string>
#include <string_view>

struct event;
struct event_base;

namespace leschat {

class PeerRegistry;

enum class TimeSyncMode {
    Off,
    Authority,
    Client
};

class DiscoveryService {
public:
    DiscoveryService(
        event_base* event_base,
        PeerRegistry& registry,
        NodeIdentity identity,
        std::uint16_t http_port,
        std::string discovery_address,
        std::string discovery_interface,
        std::uint16_t discovery_port,
        TimeSyncMode time_sync_mode,
        std::string time_authority_id
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
    void announce() noexcept;
    void send_announce();
    void synchronize_time(
        std::string_view authority_id,
        std::int64_t authority_time_ms
    );
    void close_resources() noexcept;

    PeerRegistry& registry_;
    NodeIdentity identity_;
    std::uint16_t http_port_;
    std::string discovery_address_;
    std::string discovery_interface_;
    std::uint16_t discovery_port_;
    TimeSyncMode time_sync_mode_;
    std::string time_authority_id_;
    bool announce_failed_{false};
    int socket_{-1};
    event* receive_event_{nullptr};
    event* announce_event_{nullptr};
};

}  // namespace leschat
