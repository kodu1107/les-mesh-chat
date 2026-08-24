#pragma once

#include "leschat/node_identity.hpp"
#include "leschat/peer_registry.hpp"

#include <cstdint>
#include <memory>
#include <string>

struct event_base;

namespace leschat {

class DiscoveryService;
class EventStream;
class HttpApi;
class MessageService;
class MessageStore;
class ReplicationService;
class SyncService;

struct EventBaseDeleter {
    void operator()(event_base* base) const noexcept;
};

class Application {
public:
    Application(
        NodeIdentity identity,
        std::string bind_address,
        std::uint16_t port,
        std::string discovery_address,
        std::uint16_t discovery_port,
        std::string database_path
    );
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();

private:
    NodeIdentity identity_;
    std::string bind_address_;
    std::uint16_t port_;
    std::unique_ptr<event_base, EventBaseDeleter> event_base_;
    PeerRegistry peer_registry_;
    std::unique_ptr<MessageStore> message_store_;
    std::unique_ptr<DiscoveryService> discovery_service_;
    std::unique_ptr<ReplicationService> replication_service_;
    std::unique_ptr<SyncService> sync_service_;
    std::unique_ptr<EventStream> event_stream_;
    std::unique_ptr<MessageService> message_service_;
    std::unique_ptr<HttpApi> http_api_;
};

}  // namespace leschat
