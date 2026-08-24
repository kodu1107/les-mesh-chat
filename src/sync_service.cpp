#include "leschat/sync_service.hpp"

#include "leschat/clock.hpp"
#include "leschat/http.hpp"
#include "leschat/json.hpp"
#include "leschat/message_codec.hpp"
#include "leschat/message_store.hpp"
#include "leschat/peer.hpp"
#include "leschat/peer_registry.hpp"
#include "leschat/protocol.hpp"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace leschat {

struct SyncService::RequestContext {
    SyncService* service;
    evhttp_connection* connection;
    std::string address;
    std::uint16_t port;
};

SyncService::SyncService(
    event_base* event_base,
    const PeerRegistry& peers,
    const MessageStore& store
)
    : event_base_(event_base), peers_(peers), store_(store) {
    timer_ = event_new(
        event_base_, -1, EV_PERSIST,
        &SyncService::timer_callback, this
    );
    if (timer_ == nullptr) {
        throw std::runtime_error("Unable to create sync timer");
    }
    const timeval interval{5, 0};
    if (event_add(timer_, &interval) != 0) {
        event_free(timer_);
        timer_ = nullptr;
        throw std::runtime_error("Unable to activate sync timer");
    }
}

SyncService::~SyncService() {
    if (timer_ != nullptr) {
        event_free(timer_);
    }
}

void SyncService::timer_callback(int, short, void* context) noexcept {
    try {
        static_cast<SyncService*>(context)->synchronize();
    } catch (const std::exception& error) {
        std::cerr << "Sync error: " << error.what() << '\n';
    }
}

void SyncService::synchronize() {
    JsonPtr root = make_json_object();
    JsonPtr origins = make_json_object();
    for (const auto& [origin, sequence] : store_.sequence_summary()) {
        json_object_object_add(origins.get(), origin.c_str(),
            json_object_new_int64(static_cast<std::int64_t>(sequence)));
    }
    json_object_object_add(root.get(), "protocol",
                           json_object_new_int(protocol_version));
    json_object_object_add(root.get(), "type",
                           json_object_new_string("sync_summary"));
    json_object_object_add(root.get(), "origins", origins.release());
    const std::string payload = serialize_json(root.get());

    for (const Peer& peer : peers_.active_peers(current_time_ms())) {
        evhttp_connection* connection = evhttp_connection_base_new(
            event_base_, nullptr, peer.address.c_str(), peer.http_port
        );
        if (connection == nullptr) {
            continue;
        }
        auto context = std::make_unique<RequestContext>(
            RequestContext{this, connection, peer.address, peer.http_port}
        );
        evhttp_request* request = evhttp_request_new(
            &SyncService::summary_response_callback, context.get()
        );
        if (request == nullptr) {
            evhttp_connection_free(connection);
            continue;
        }
        evhttp_add_header(evhttp_request_get_output_headers(request),
                          "Content-Type", "application/json");
        evbuffer_add(evhttp_request_get_output_buffer(request),
                     payload.data(), payload.size());
        if (evhttp_make_request(connection, request, EVHTTP_REQ_POST,
                                "/api/v1/sync/summary") != 0) {
            evhttp_request_free(request);
            evhttp_connection_free(connection);
            continue;
        }
        static_cast<void>(context.release());
    }
}

void SyncService::summary_response_callback(
    evhttp_request* request, void* context
) noexcept {
    std::unique_ptr<RequestContext> state{
        static_cast<RequestContext*>(context)
    };
    try {
        if (request == nullptr ||
            evhttp_request_get_response_code(request) != HTTP_OK) {
            throw std::runtime_error("sync summary request failed");
        }
        state->service->handle_summary_response(request, *state);
    } catch (const std::exception& error) {
        std::cerr << "Sync summary error: " << error.what() << '\n';
    }
    evhttp_connection_free(state->connection);
}

void SyncService::handle_summary_response(
    evhttp_request* request,
    const RequestContext& context
) {
    send_missing(
        context,
        read_request_body(request, max_sync_request_bytes)
    );
}

void SyncService::send_missing(
    const RequestContext& context,
    std::string_view summary_json
) {
    JsonParse parsed = parse_json(summary_json);
    if (parsed.error != json_tokener_success ||
        !json_is_object(parsed.object.get())) {
        throw std::runtime_error("Invalid sync summary response");
    }
    json_object* origins = nullptr;
    if (!json_object_object_get_ex(parsed.object.get(), "origins", &origins) ||
        !json_object_is_type(origins, json_type_object)) {
        throw std::runtime_error("Sync summary has no origins");
    }

    JsonPtr batch = make_json_object();
    JsonPtr messages = make_json_array();
    std::size_t count = 0;
    for (const ChatMessage& message : store_.messages()) {
        json_object* last_value = nullptr;
        std::uint64_t last = 0;
        if (json_object_object_get_ex(origins, message.origin.c_str(),
                                      &last_value) &&
            json_object_is_type(last_value, json_type_int)) {
            const std::int64_t parsed_last = json_object_get_int64(last_value);
            if (parsed_last > 0) {
                last = static_cast<std::uint64_t>(parsed_last);
            }
        }
        if (message.sequence > last && count < max_sync_batch_messages) {
            JsonPtr item = message_to_json(message);
            json_object_array_add(messages.get(), item.release());
            ++count;
        }
    }
    if (count == 0U) {
        return;
    }
    json_object_object_add(batch.get(), "protocol",
                           json_object_new_int(protocol_version));
    json_object_object_add(batch.get(), "type",
                           json_object_new_string("sync_messages"));
    json_object_object_add(batch.get(), "messages", messages.release());
    send_batch(context.address, context.port, serialize_json(batch.get()));
}

void SyncService::send_batch(
    std::string_view address,
    std::uint16_t port,
    std::string_view payload
) {
    evhttp_connection* connection = evhttp_connection_base_new(
        event_base_, nullptr, std::string{address}.c_str(), port
    );
    if (connection == nullptr) {
        return;
    }
    auto context = std::make_unique<RequestContext>(
        RequestContext{this, connection, std::string{address}, port}
    );
    evhttp_request* request = evhttp_request_new(
        &SyncService::send_response_callback, context.get()
    );
    if (request == nullptr) {
        evhttp_connection_free(connection);
        return;
    }
    evhttp_add_header(evhttp_request_get_output_headers(request),
                      "Content-Type", "application/json");
    evbuffer_add(evhttp_request_get_output_buffer(request),
                 payload.data(), payload.size());
    if (evhttp_make_request(connection, request, EVHTTP_REQ_POST,
                            "/api/v1/sync/messages") != 0) {
        evhttp_request_free(request);
        evhttp_connection_free(connection);
        return;
    }
    static_cast<void>(context.release());
}

void SyncService::send_response_callback(
    evhttp_request*, void* context
) noexcept {
    std::unique_ptr<RequestContext> state{
        static_cast<RequestContext*>(context)
    };
    evhttp_connection_free(state->connection);
}

}  // namespace leschat
