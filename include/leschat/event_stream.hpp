#pragma once

#include <string_view>
#include <vector>

struct evhttp_connection;
struct evhttp_request;

namespace leschat {

class EventStream {
public:
    EventStream() = default;
    ~EventStream();

    EventStream(const EventStream&) = delete;
    EventStream& operator=(const EventStream&) = delete;
    EventStream(EventStream&&) = delete;
    EventStream& operator=(EventStream&&) = delete;

    void subscribe(evhttp_request* request);
    void publish(std::string_view event_name, std::string_view json_data);

private:
    static void handle_close(
        evhttp_connection* connection,
        void* context
    ) noexcept;

    std::vector<evhttp_request*> requests_;
};

}  // namespace leschat
