#pragma once

#include "leschat/node_identity.hpp"

#include <cstdint>
#include <memory>
#include <string>

struct event_base;
struct evhttp;
struct evhttp_request;

namespace leschat {

class EventStream;
class MessageService;
class PeerRegistry;

struct EvhttpDeleter {
    void operator()(evhttp* server) const noexcept;
};

class HttpApi {
public:
    HttpApi(
        event_base* event_base,
        NodeIdentity identity,
        std::string bind_address,
        std::uint16_t port,
        PeerRegistry& peers,
        MessageService& messages,
        EventStream& events
    );
    ~HttpApi();

    HttpApi(const HttpApi&) = delete;
    HttpApi& operator=(const HttpApi&) = delete;
    HttpApi(HttpApi&&) = delete;
    HttpApi& operator=(HttpApi&&) = delete;

private:
    static void handle_request(evhttp_request* request, void* context);
    void process_request(evhttp_request* request);

    void serve_index(evhttp_request* request);
    void serve_stylesheet(evhttp_request* request);
    void serve_script(evhttp_request* request);
    void get_healthz(evhttp_request* request);
    void get_status(evhttp_request* request);
    void get_messages(evhttp_request* request);
    void create_message(evhttp_request* request);
    void get_peers(evhttp_request* request);
    void replicate_message(evhttp_request* request);
    void sync_summary(evhttp_request* request);
    void sync_messages(evhttp_request* request);
    void open_event_stream(evhttp_request* request);

    NodeIdentity identity_;
    std::string bind_address_;
    std::uint16_t port_;
    PeerRegistry& peers_;
    MessageService& messages_;
    EventStream& events_;
    std::unique_ptr<evhttp, EvhttpDeleter> http_server_;
};

}  // namespace leschat
