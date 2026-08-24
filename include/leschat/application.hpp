#pragma once

#include "leschat/node_identity.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct event_base;
struct evhttp;
struct evhttp_request;

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

class Application {
public:
    Application(
        NodeIdentity identity,
        std::string bind_address,
        std::uint16_t port
    );

    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();

private:
    static void handle_request(
        evhttp_request* request,
        void* context
    );

    void process_request(evhttp_request* request);
    void get_messages(evhttp_request* request);
    void create_message(evhttp_request* request);

    NodeIdentity identity_;
    std::string bind_address_;
    std::uint16_t port_;

    std::vector<ChatMessage> messages_;
    std::uint64_t next_sequence_{1};

    event_base* event_base_{nullptr};
    evhttp* http_server_{nullptr};
};

}  // namespace leschat