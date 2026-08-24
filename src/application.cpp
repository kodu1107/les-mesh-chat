#include "leschat/application.hpp"

#include "leschat/discovery_service.hpp"
#include "leschat/event_stream.hpp"
#include "leschat/http_api.hpp"
#include "leschat/message_service.hpp"
#include "leschat/message_store.hpp"
#include "leschat/protocol.hpp"
#include "leschat/replication_service.hpp"
#include "leschat/sync_service.hpp"

#include <event2/event.h>

#include <iostream>
#include <stdexcept>
#include <utility>

namespace leschat {

void EventBaseDeleter::operator()(event_base* base) const noexcept {
    if (base != nullptr) {
        event_base_free(base);
    }
}

Application::Application(
    NodeIdentity identity,
    std::string bind_address,
    std::uint16_t port,
    std::string discovery_address,
    std::uint16_t discovery_port,
    std::string database_path
)
    : identity_(std::move(identity)),
      bind_address_(std::move(bind_address)),
      port_(port) {
    event_base_.reset(event_base_new());
    if (!event_base_) {
        throw std::runtime_error(
            "Failed to create libevent event base"
        );
    }

    message_store_ = std::make_unique<MessageStore>(
        std::move(database_path),
        max_database_bytes
    );
    discovery_service_ = std::make_unique<DiscoveryService>(
        event_base_.get(),
        peer_registry_,
        identity_,
        port_,
        std::move(discovery_address),
        discovery_port
    );
    replication_service_ = std::make_unique<ReplicationService>(
        event_base_.get(),
        peer_registry_
    );
    sync_service_ = std::make_unique<SyncService>(
        event_base_.get(),
        peer_registry_,
        *message_store_
    );
    event_stream_ = std::make_unique<EventStream>();
    message_service_ = std::make_unique<MessageService>(
        identity_,
        *message_store_,
        *replication_service_,
        *event_stream_
    );
    http_api_ = std::make_unique<HttpApi>(
        event_base_.get(),
        identity_,
        bind_address_,
        port_,
        peer_registry_,
        *message_service_,
        *event_stream_
    );
}

Application::~Application() {
    discovery_service_.reset();
    sync_service_.reset();
    event_stream_.reset();
    http_api_.reset();
    message_service_.reset();
    replication_service_.reset();
    message_store_.reset();
    event_base_.reset();
}

void Application::run() {
    std::cout
        << "LES Mesh Chat " << app_version << '\n'
        << "Node ID: " << identity_.node_id << '\n'
        << "Callsign: " << identity_.callsign << '\n'
        << "Listening on http://"
        << bind_address_
        << ':'
        << port_
        << '\n';

    const int result = event_base_dispatch(event_base_.get());
    if (result < 0) {
        throw std::runtime_error("libevent dispatch failed");
    }
}

}  // namespace leschat
