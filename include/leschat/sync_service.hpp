#pragma once

#include <cstdint>
#include <string_view>

struct event;
struct event_base;
struct evhttp_request;

namespace leschat {

class MessageStore;
class PeerRegistry;

class SyncService {
public:
    SyncService(
        event_base* event_base,
        const PeerRegistry& peers,
        const MessageStore& store
    );
    ~SyncService();

    SyncService(const SyncService&) = delete;
    SyncService& operator=(const SyncService&) = delete;
    SyncService(SyncService&&) = delete;
    SyncService& operator=(SyncService&&) = delete;

private:
    struct RequestContext;
    static void timer_callback(int socket, short events, void* context) noexcept;
    static void summary_response_callback(
        evhttp_request* request,
        void* context
    ) noexcept;
    static void send_response_callback(
        evhttp_request* request,
        void* context
    ) noexcept;

    void synchronize();
    void handle_summary_response(
        evhttp_request* request,
        const RequestContext& context
    );
    void send_missing(
        const RequestContext& context,
        std::string_view summary_json
    );
    void send_batch(
        std::string_view address,
        std::uint16_t port,
        std::string_view payload
    );

    event_base* event_base_;
    const PeerRegistry& peers_;
    const MessageStore& store_;
    event* timer_{nullptr};
};

}  // namespace leschat
