#pragma once

#include <cstdint>
#include <string>

struct event_base;
struct evhttp;
struct evhttp_request;

namespace leschat {
    class Application {
        public:
            Application(std::string bind_address, uint16_t port);
            ~Application();

            Application(const Application&) = delete;
            Application& operator=(const Application&) = delete;
            Application(Application&&) = delete;
            Application& operator=(Application&&) = delete;

            void run();
        private:
            static void handle_request(evhttp_request* request, void* context);

            void process_request(evhttp_request* request);

            std::string bind_address_;
            std::uint16_t port_;
            event_base* event_base_{nullptr};
            evhttp* http_server_{nullptr};
    };
}