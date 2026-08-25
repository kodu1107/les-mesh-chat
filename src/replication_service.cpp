#include "leschat/replication_service.hpp"

#include "leschat/clock.hpp"
#include "leschat/peer.hpp"
#include "leschat/peer_registry.hpp"

#include <event2/buffer.h>
#include <event2/http.h>

#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace leschat {

struct ReplicationService::RequestContext {
    evhttp_connection* connection;
};

ReplicationService::ReplicationService(
    event_base* event_base,
    const PeerRegistry& peers,
    std::string local_address
)
    : event_base_(event_base),
      peers_(peers),
      local_address_(std::move(local_address)) {}

void ReplicationService::broadcast(std::string_view json_payload) {
    const std::vector<Peer> peers = peers_.active_peers(current_time_ms());
    for (const Peer& peer : peers) {
        evhttp_connection* connection = evhttp_connection_base_new(
            event_base_, nullptr, peer.address.c_str(), peer.http_port
        );
        if (connection == nullptr) {
            std::cerr << "Unable to create replication connection for peer "
                      << peer.node_id << '\n';
            continue;
        }
        if (!local_address_.empty()) {
            evhttp_connection_set_local_address(
                connection, local_address_.c_str()
            );
        }

        auto context = std::make_unique<RequestContext>();
        context->connection = connection;
        evhttp_request* outgoing = evhttp_request_new(
            &ReplicationService::handle_response, context.get()
        );
        if (outgoing == nullptr) {
            evhttp_connection_free(connection);
            continue;
        }
        evhttp_add_header(evhttp_request_get_output_headers(outgoing),
                          "Content-Type", "application/json");
        evhttp_add_header(evhttp_request_get_output_headers(outgoing),
                          "Accept", "application/json");
        evbuffer_add(evhttp_request_get_output_buffer(outgoing),
                     json_payload.data(), json_payload.size());
        if (evhttp_make_request(connection, outgoing, EVHTTP_REQ_POST,
                                "/api/v1/replicate") != 0) {
            evhttp_request_free(outgoing);
            evhttp_connection_free(connection);
            continue;
        }
        static_cast<void>(context.release());
    }
}

void ReplicationService::handle_response(
    evhttp_request* request,
    void* context
) noexcept {
    std::unique_ptr<RequestContext> state{
        static_cast<RequestContext*>(context)
    };
    if (request == nullptr ||
        evhttp_request_get_response_code(request) != HTTP_OK) {
        std::cerr << "Message replication request failed\n";
    }
    evhttp_connection_free(state->connection);
}

}  // namespace leschat
