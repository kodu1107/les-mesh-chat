#include "leschat/application.hpp"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <json-c/json.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#ifndef LESCHAT_WEB_ROOT
#define LESCHAT_WEB_ROOT "web"
#endif

namespace leschat {

namespace {

constexpr std::size_t max_message_bytes = 2048;
constexpr std::size_t max_request_bytes = 4096;
constexpr std::size_t max_memory_messages = 5000;

struct JsonObjectDeleter {
    void operator()(json_object* object) const noexcept {
        if (object != nullptr) {
            json_object_put(object);
        }
    }
};

using JsonObjectPtr =
    std::unique_ptr<json_object, JsonObjectDeleter>;

void send_response(
    evhttp_request* request,
    int status,
    const char* reason,
    std::string_view content_type,
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

    const std::string content_type_text{content_type};

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

    evbuffer_add(output, body.data(), body.size());
    evhttp_send_reply(request, status, reason, output);
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

std::string serialize_json(json_object* object) {
    const char* serialized =
        json_object_to_json_string_ext(
            object,
            JSON_C_TO_STRING_PLAIN
        );

    if (serialized == nullptr) {
        throw std::runtime_error(
            "Unable to serialize JSON"
        );
    }

    return serialized;
}

std::string load_web_asset(std::string_view filename) {
    const std::string path =
        std::string{LESCHAT_WEB_ROOT} +
        "/" +
        std::string{filename};

    std::ifstream input{path, std::ios::binary};

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

std::string request_path(evhttp_request* request) {
    const char* uri_text =
        evhttp_request_get_uri(request);

    if (uri_text == nullptr) {
        return {};
    }

    evhttp_uri* uri = evhttp_uri_parse(uri_text);

    if (uri == nullptr) {
        return {};
    }

    const char* path_text = evhttp_uri_get_path(uri);

    std::string path =
        path_text == nullptr ? "/" : path_text;

    evhttp_uri_free(uri);
    return path;
}

std::string read_request_body(
    evhttp_request* request
) {
    evbuffer* input =
        evhttp_request_get_input_buffer(request);

    const std::size_t length =
        evbuffer_get_length(input);

    if (length > max_request_bytes) {
        throw std::length_error("request_too_large");
    }

    std::string body(length, '\0');

    if (length != 0) {
        const auto copied = evbuffer_copyout(
            input,
            body.data(),
            length
        );

        if (copied < 0 ||
            static_cast<std::size_t>(copied) != length) {
            throw std::runtime_error(
                "Unable to read request body"
            );
        }
    }

    return body;
}

bool is_valid_utf8(std::string_view text) {
    std::size_t index = 0;

    while (index < text.size()) {
        const auto first =
            static_cast<unsigned char>(text[index]);

        std::size_t continuation_count = 0;
        std::uint32_t code_point = 0;

        if (first <= 0x7F) {
            continuation_count = 0;
            code_point = first;
        } else if (
            first >= 0xC2 &&
            first <= 0xDF
        ) {
            continuation_count = 1;
            code_point = first & 0x1F;
        } else if (
            first >= 0xE0 &&
            first <= 0xEF
        ) {
            continuation_count = 2;
            code_point = first & 0x0F;
        } else if (
            first >= 0xF0 &&
            first <= 0xF4
        ) {
            continuation_count = 3;
            code_point = first & 0x07;
        } else {
            return false;
        }

        if (index + continuation_count >= text.size()) {
            return false;
        }

        for (std::size_t offset = 1;
             offset <= continuation_count;
             ++offset) {
            const auto next = static_cast<unsigned char>(
                text[index + offset]
            );

            if ((next & 0xC0) != 0x80) {
                return false;
            }

            code_point =
                (code_point << 6) |
                (next & 0x3F);
        }

        if (
            (continuation_count == 2 &&
             code_point < 0x800) ||
            (continuation_count == 3 &&
             code_point < 0x10000) ||
            code_point > 0x10FFFF ||
            (code_point >= 0xD800 &&
             code_point <= 0xDFFF)
        ) {
            return false;
        }

        index += continuation_count + 1;
    }

    return true;
}

std::int64_t current_time_ms() {
    const auto now =
        std::chrono::system_clock::now();

    return std::chrono::duration_cast<
        std::chrono::milliseconds
    >(
        now.time_since_epoch()
    ).count();
}

JsonObjectPtr message_to_json(
    const ChatMessage& message
) {
    JsonObjectPtr object{json_object_new_object()};

    if (!object) {
        throw std::runtime_error(
            "Unable to create message JSON"
        );
    }

    json_object_object_add(
        object.get(),
        "sequence",
        json_object_new_int64(
            static_cast<std::int64_t>(
                message.sequence
            )
        )
    );

    json_object_object_add(
        object.get(),
        "id",
        json_object_new_string(
            message.message_id.c_str()
        )
    );

    json_object_object_add(
        object.get(),
        "callsign",
        json_object_new_string(
            message.callsign.c_str()
        )
    );

    json_object_object_add(
        object.get(),
        "channel",
        json_object_new_string(
            message.channel.c_str()
        )
    );

    json_object_object_add(
        object.get(),
        "created_at_ms",
        json_object_new_int64(
            message.created_at_ms
        )
    );

    json_object_object_add(
        object.get(),
        "body",
        json_object_new_string_len(
            message.body.data(),
            static_cast<int>(message.body.size())
        )
    );

    json_object_object_add(
        object.get(),
        "origin",
        json_object_new_string(
            message.origin.c_str()
        )
    );

    return object;
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

    return serialize_json(root.get());
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
    }

    if (event_base_ != nullptr) {
        event_base_free(event_base_);
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
    } catch (const std::length_error&) {
        send_json(
            request,
            413,
            "Payload Too Large",
            R"({"error":"request_too_large"})"
        );
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
    const std::string path = request_path(request);

    if (path.empty()) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            R"({"error":"invalid_uri"})"
        );
        return;
    }

    const evhttp_cmd_type method =
        evhttp_request_get_command(request);

    if (method == EVHTTP_REQ_GET && path == "/") {
        send_web_asset(
            request,
            "index.html",
            "text/html; charset=utf-8"
        );
        return;
    }

    if (
        method == EVHTTP_REQ_GET &&
        path == "/style.css"
    ) {
        send_web_asset(
            request,
            "style.css",
            "text/css; charset=utf-8"
        );
        return;
    }

    if (
        method == EVHTTP_REQ_GET &&
        path == "/app.js"
    ) {
        send_web_asset(
            request,
            "app.js",
            "text/javascript; charset=utf-8"
        );
        return;
    }

    if (
        method == EVHTTP_REQ_GET &&
        path == "/healthz"
    ) {
        send_json(
            request,
            HTTP_OK,
            "OK",
            R"({"status":"ok","service":"les-chatd","version":"0.1.0"})"
        );
        return;
    }

    if (
        method == EVHTTP_REQ_GET &&
        path == "/api/v1/status"
    ) {
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

    if (path == "/api/v1/messages") {
        if (method == EVHTTP_REQ_GET) {
            get_messages(request);
            return;
        }

        if (method == EVHTTP_REQ_POST) {
            create_message(request);
            return;
        }

        send_json(
            request,
            HTTP_BADMETHOD,
            "Method Not Allowed",
            R"({"error":"method_not_allowed"})"
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

void Application::get_messages(
    evhttp_request* request
) {
    JsonObjectPtr root{json_object_new_object()};
    JsonObjectPtr items{json_object_new_array()};

    if (!root || !items) {
        throw std::runtime_error(
            "Unable to create messages JSON"
        );
    }

    for (const ChatMessage& message : messages_) {
        JsonObjectPtr item = message_to_json(message);

        json_object_array_add(
            items.get(),
            item.release()
        );
    }

    json_object_object_add(
        root.get(),
        "messages",
        items.release()
    );

    json_object_object_add(
        root.get(),
        "count",
        json_object_new_int64(
            static_cast<std::int64_t>(
                messages_.size()
            )
        )
    );

    send_json(
        request,
        HTTP_OK,
        "OK",
        serialize_json(root.get())
    );
}

void Application::create_message(
    evhttp_request* request
) {
    const char* content_type = evhttp_find_header(
        evhttp_request_get_input_headers(request),
        "Content-Type"
    );

    if (
        content_type == nullptr ||
        std::string_view{content_type}.find(
            "application/json"
        ) != 0
    ) {
        send_json(
            request,
            415,
            "Unsupported Media Type",
            R"({"error":"content_type_must_be_application_json"})"
        );
        return;
    }

    const std::string request_body =
        read_request_body(request);

    json_tokener* tokener = json_tokener_new();

    if (tokener == nullptr) {
        throw std::runtime_error(
            "Unable to create JSON parser"
        );
    }

    json_object* parsed_raw = json_tokener_parse_ex(
        tokener,
        request_body.data(),
        static_cast<int>(request_body.size())
    );

    const json_tokener_error parse_error =
        json_tokener_get_error(tokener);

    json_tokener_free(tokener);

    JsonObjectPtr parsed{parsed_raw};

    if (
        parse_error != json_tokener_success ||
        !parsed ||
        !json_object_is_type(
            parsed.get(),
            json_type_object
        )
    ) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            R"({"error":"invalid_json"})"
        );
        return;
    }

    json_object* body_value = nullptr;

    if (
        !json_object_object_get_ex(
            parsed.get(),
            "body",
            &body_value
        ) ||
        body_value == nullptr ||
        !json_object_is_type(
            body_value,
            json_type_string
        )
    ) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            R"({"error":"body_must_be_string"})"
        );
        return;
    }

    const char* body_data =
        json_object_get_string(body_value);

    const int body_length =
        json_object_get_string_len(body_value);

    if (body_data == nullptr || body_length <= 0) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            R"({"error":"message_empty"})"
        );
        return;
    }

    const std::string body{
        body_data,
        static_cast<std::size_t>(body_length)
    };

    if (body.size() > max_message_bytes) {
        send_json(
            request,
            413,
            "Payload Too Large",
            R"({"error":"message_too_large","max_bytes":2048})"
        );
        return;
    }

    if (!is_valid_utf8(body)) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            R"({"error":"message_must_be_valid_utf8"})"
        );
        return;
    }

    ChatMessage message{
        .sequence = next_sequence_,
        .message_id =
            identity_.node_id +
            ":" +
            std::to_string(next_sequence_),
        .origin = identity_.node_id,
        .callsign = identity_.callsign,
        .channel = "les-manet",
        .created_at_ms = current_time_ms(),
        .body = body
    };

    ++next_sequence_;

    messages_.push_back(message);

    if (messages_.size() > max_memory_messages) {
        messages_.erase(messages_.begin());
    }

    JsonObjectPtr response{json_object_new_object()};
    JsonObjectPtr message_json =
        message_to_json(message);

    if (!response || !message_json) {
        throw std::runtime_error(
            "Unable to create response JSON"
        );
    }

    json_object_object_add(
        response.get(),
        "message",
        message_json.release()
    );

    send_json(
        request,
        201,
        "Created",
        serialize_json(response.get())
    );
}

}  // namespace leschat