#include "leschat/event_stream.hpp"

#include <event2/buffer.h>
#include <event2/http.h>

#include <algorithm>
#include <string>

namespace leschat {

EventStream::~EventStream() {
    for (evhttp_request* request : requests_) {
        evhttp_connection_set_closecb(
            evhttp_request_get_connection(request), nullptr, nullptr
        );
        evhttp_request_free(request);
    }
}

void EventStream::subscribe(evhttp_request* request) {
    evhttp_add_header(evhttp_request_get_output_headers(request),
                      "Content-Type", "text/event-stream; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(request),
                      "Cache-Control", "no-store");
    evhttp_add_header(evhttp_request_get_output_headers(request),
                      "X-Content-Type-Options", "nosniff");
    evhttp_add_header(evhttp_request_get_output_headers(request),
                      "Connection", "keep-alive");
    evhttp_request_own(request);
    requests_.push_back(request);
    evhttp_connection_set_closecb(
        evhttp_request_get_connection(request),
        &EventStream::handle_close, this
    );
    evhttp_send_reply_start(request, HTTP_OK, "OK");
    evbuffer* output = evhttp_request_get_output_buffer(request);
    constexpr std::string_view ready{"event: ready\ndata: {}\n\n"};
    evbuffer_add(output, ready.data(), ready.size());
    evhttp_send_reply_chunk(request, output);
}

void EventStream::publish(
    std::string_view event_name,
    std::string_view json_data
) {
    const std::string payload =
        "event: " + std::string{event_name} +
        "\ndata: " + std::string{json_data} + "\n\n";
    for (evhttp_request* request : requests_) {
        evbuffer* output = evhttp_request_get_output_buffer(request);
        evbuffer_add(output, payload.data(), payload.size());
        evhttp_send_reply_chunk(request, output);
    }
}

void EventStream::handle_close(
    evhttp_connection* connection,
    void* context
) noexcept {
    auto* stream = static_cast<EventStream*>(context);
    const auto match = std::ranges::find_if(
        stream->requests_,
        [connection](evhttp_request* request) {
            return evhttp_request_get_connection(request) == connection;
        }
    );
    if (match != stream->requests_.end()) {
        evhttp_request_free(*match);
        stream->requests_.erase(match);
    }
}

}  // namespace leschat
