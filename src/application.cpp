#include "leschat/application.hpp"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace leschat {

namespace {

void send_json(
    evhttp_request* request,
    int status,
    const char* reason,
    std::string_view body
) {
    evbuffer* output = evbuffer_new();

    if (output == nullptr) {
        evhttp_send_error(
            request,
            HTTP_INTERNAL,
            "Unable to allocate response buffer"
        );
        return;
    }

    evhttp_add_header(
        evhttp_request_get_output_headers(request),
        "Content-Type",
        "application/json; charset=utf-8"
    );

    evhttp_add_header(
        evhttp_request_get_output_headers(request),
        "Cache-Control",
        "no-store"
    );

    evbuffer_add(
        output,
        body.data(),
        body.size()
    );

    evhttp_send_reply(
        request,
        status,
        reason,
        output
    );

    evbuffer_free(output);
}

}  // namespace

Application::Application(
    std::string bind_address,
    std::uint16_t port
)
    : bind_address_(std::move(bind_address)),
      port_(port) {
    event_base_ = event_base_new();

    if (event_base_ == nullptr) {
        throw std::runtime_error(
            "Failed to create libevent event base"
        );
    }

    http_server_ = evhttp_new(event_base_);

    if (http_server_ == nullptr) {
        event_base_free(event_base_);
        event_base_ = nullptr;

        throw std::runtime_error(
            "Failed to create HTTP server"
        );
    }

    evhttp_set_gencb(
        http_server_,
        &Application::handle_request,
        this
    );

    const int bind_result = evhttp_bind_socket(
        http_server_,
        bind_address_.c_str(),
        port_
    );

    if (bind_result != 0) {
        evhttp_free(http_server_);
        http_server_ = nullptr;

        event_base_free(event_base_);
        event_base_ = nullptr;

        throw std::runtime_error(
            "Failed to bind HTTP server to " +
            bind_address_ + ":" +
            std::to_string(port_)
        );
    }
}

Application::~Application() {
    if (http_server_ != nullptr) {
        evhttp_free(http_server_);
        http_server_ = nullptr;
    }

    if (event_base_ != nullptr) {
        event_base_free(event_base_);
        event_base_ = nullptr;
    }
}

void Application::run() {
    std::cout
        << "LES Mesh Chat 0.1.0\n"
        << "Listening on http://"
        << bind_address_
        << ':'
        << port_
        << '\n';

    const int result = event_base_dispatch(event_base_);

    if (result < 0) {
        throw std::runtime_error(
            "libevent dispatch failed"
        );
    }
}

void Application::handle_request(
    evhttp_request* request,
    void* context
) {
    auto* application =
        static_cast<Application*>(context);

    application->process_request(request);
}

void Application::process_request(
    evhttp_request* request
) {
    if (evhttp_request_get_command(request) != EVHTTP_REQ_GET) {
        send_json(
            request,
            HTTP_BADMETHOD,
            "Method Not Allowed",
            R"({"error":"method_not_allowed"})"
        );
        return;
    }

    const char* uri = evhttp_request_get_uri(request);

    if (uri != nullptr &&
        std::string_view{uri} == "/healthz") {
        send_json(
            request,
            HTTP_OK,
            "OK",
            R"({"status":"ok","service":"les-chatd","version":"0.1.0"})"
        );
        return;
    }

    if (uri != nullptr &&
        std::string_view{uri} == "/") {
        send_json(
            request,
            HTTP_OK,
            "OK",
            R"({"service":"LES Mesh Chat","health":"/healthz"})"
        );
        return;
    }

    send_json(
        request,
        HTTP_NOTFOUND,
        "Not Found",
        R"({"error":"not_found"})"
    );
}

}  // namespace leschat