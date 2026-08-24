#pragma once

#include <string_view>

struct event_base;
struct evhttp_request;

namespace leschat {

class PeerRegistry;

class ReplicationService {
public:
    ReplicationService(event_base* event_base, const PeerRegistry& peers);

    void broadcast(std::string_view json_payload);

private:
    struct RequestContext;
    static void handle_response(
        evhttp_request* request,
        void* context
    ) noexcept;

    event_base* event_base_;
    const PeerRegistry& peers_;
};

}  // namespace leschat
