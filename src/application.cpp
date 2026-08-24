#include "leschat/application.hpp"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <json-c/json.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <fstream>
#include <sstream>

#ifndef LESCHAT_WEB_ROOT
#define LESCHAT_WEB_ROOT "web"
#endif

namespace leschat {

namespace {

struct JsonObjectDeleter {
    void operator()(json_object* object) const noexcept {
        if (object != nullptr) {
            json_object_put(object);
        }
    }
};

using JsonObjectPtr = std::unique_ptr<json_object, JsonObjectDeleter>;

void send_response(evhttp_request* request,int status,const char* reason,std::string_view content_type,std::string_view body) {
    evbuffer* output = evbuffer_new();

    if (output == nullptr) {
        evhttp_send_error(
            request,
            HTTP_INTERNAL,
            "Unable to allocate response buffer"
        );
        return;
    }

    const std::string content_type_text{
        content_type
    };

    evhttp_add_header(
        evhttp_request_get_output_headers(request),
        "Content-Type",
        content_type_text.c_str()
    );

    evhttp_add_header(
        evhttp_request_get_output_headers(request),
        "Cache-Control",
        "no-store"
    );

    evhttp_add_header(
        evhttp_request_get_output_headers(request),
        "X-Content-Type-Options",
        "nosniff"
    );

    if (content_type == "text/html; charset=utf-8") {
        evhttp_add_header(
            evhttp_request_get_output_headers(request),
            "Content-Security-Policy",
            "default-src 'self'; "
            "script-src 'self'; "
            "style-src 'self'; "
            "connect-src 'self'; "
            "img-src 'self'; "
            "object-src 'none'; "
            "base-uri 'none'"
        );
    }

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

void send_json(
    evhttp_request* request,
    int status,
    const char* reason,
    std::string_view body
) {
    send_response(
        request,
        status,
        reason,
        "application/json; charset=utf-8",
        body
    );
}

std::string load_web_asset(
    std::string_view filename
) {
    const std::string path =
        std::string{LESCHAT_WEB_ROOT} +
        "/" +
        std::string{filename};

    std::ifstream input{
        path,
        std::ios::binary
    };

    if (!input.is_open()) {
        throw std::runtime_error(
            "Unable to open web asset: " + path
        );
    }

    std::ostringstream contents;
    contents << input.rdbuf();

    if (input.bad()) {
        throw std::runtime_error(
            "Unable to read web asset: " + path
        );
    }

    return contents.str();
}

void send_web_asset(
    evhttp_request* request,
    std::string_view filename,
    std::string_view content_type
) {
    send_response(
        request,
        HTTP_OK,
        "OK",
        content_type,
        load_web_asset(filename)
    );
}

std::string make_status_json(
    const NodeIdentity& identity,
    const std::string& bind_address,
    std::uint16_t port
) {
    JsonObjectPtr root{json_object_new_object()};

    if (!root) {
        throw std::runtime_error(
            "Unable to create status JSON"
        );
    }

    json_object_object_add(
        root.get(),
        "status",
        json_object_new_string("ok")
    );

    json_object_object_add(
        root.get(),
        "service",
        json_object_new_string("les-chatd")
    );

    json_object_object_add(
        root.get(),
        "version",
        json_object_new_string("0.1.0")
    );

    json_object_object_add(
        root.get(),
        "node_id",
        json_object_new_string(
            identity.node_id.c_str()
        )
    );

    json_object_object_add(
        root.get(),
        "callsign",
        json_object_new_string(
            identity.callsign.c_str()
        )
    );

    json_object_object_add(
        root.get(),
        "bind_address",
        json_object_new_string(
            bind_address.c_str()
        )
    );

    json_object_object_add(
        root.get(),
        "port",
        json_object_new_int(
            static_cast<int>(port)
        )
    );

    const char* serialized =
        json_object_to_json_string_ext(
            root.get(),
            JSON_C_TO_STRING_PLAIN
        );

    if (serialized == nullptr) {
        throw std::runtime_error(
            "Unable to serialize status JSON"
        );
    }

    return serialized;
}

}  // namespace

Application::Application(
    NodeIdentity identity,
    std::string bind_address,
    std::uint16_t port
)
    : identity_(std::move(identity)),
      bind_address_(std::move(bind_address)),
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
        << "Node ID: " << identity_.node_id << '\n'
        << "Callsign: " << identity_.callsign << '\n'
        << "Listening on http://"
        << bind_address_
        << ':'
        << port_
        << '\n';

    const int result =
        event_base_dispatch(event_base_);

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

    try {
        application->process_request(request);
    } catch (const std::exception& error) {
        std::cerr
            << "Request handling error: "
            << error.what()
            << '\n';

        send_json(
            request,
            HTTP_INTERNAL,
            "Internal Server Error",
            R"({"error":"internal_error"})"
        );
    }
}

void Application::process_request(
    evhttp_request* request
) {
    if (evhttp_request_get_command(request) !=
        EVHTTP_REQ_GET) {
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
        std::string_view{uri} == "/") {
        send_web_asset(
            request,
            "index.html",
            "text/html; charset=utf-8"
        );
        return;
    }

    if (uri != nullptr &&
        std::string_view{uri} == "/style.css") {
        send_web_asset(
            request,
            "style.css",
            "text/css; charset=utf-8"
        );
        return;
    }

    if (uri != nullptr &&
        std::string_view{uri} == "/app.js") {
        send_web_asset(
            request,
            "app.js",
            "text/javascript; charset=utf-8"
        );
        return;
    }
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
        std::string_view{uri} ==
            "/api/v1/status") {
        send_json(
            request,
            HTTP_OK,
            "OK",
            make_status_json(
                identity_,
                bind_address_,
                port_
            )
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