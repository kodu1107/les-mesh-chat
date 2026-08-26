#include "leschat/http_api.hpp"

#include "leschat/clock.hpp"
#include "leschat/event_stream.hpp"
#include "leschat/http.hpp"
#include "leschat/json.hpp"
#include "leschat/message.hpp"
#include "leschat/message_codec.hpp"
#include "leschat/message_service.hpp"
#include "leschat/peer.hpp"
#include "leschat/peer_registry.hpp"
#include "leschat/protocol.hpp"
#include "leschat/utf8.hpp"

#include <event2/http.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace leschat {

void EvhttpDeleter::operator()(evhttp* server) const noexcept {
    if (server != nullptr) {
        evhttp_free(server);
    }
}

namespace {

struct Route {
    evhttp_cmd_type method;
    std::string_view path;
    void (HttpApi::*handler)(evhttp_request*);
};

void send_unsupported_media_type(evhttp_request* request) {
    send_json(
        request,
        415,
        "Unsupported Media Type",
        R"({"error":"content_type_must_be_application_json"})"
    );
}

JsonPtr read_json_object(
    evhttp_request* request,
    std::size_t limit,
    bool require_complete,
    std::string_view invalid_json
) {
    if (!has_json_content_type(request)) {
        send_unsupported_media_type(request);
        return {};
    }

    const std::string body = read_request_body(request, limit);
    JsonParse parsed = parse_json(body);
    if (parsed.error != json_tokener_success ||
        !json_is_object(parsed.object.get()) ||
        (require_complete && parsed.parse_end != body.size())) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            invalid_json
        );
        return {};
    }

    return std::move(parsed.object);
}

std::string make_status_json(
    const NodeIdentity& identity,
    const std::string& bind_address,
    std::uint16_t port
) {
    JsonPtr root = make_json_object();

    json_object_object_add(
        root.get(),
        "status",
        json_object_new_string("ok")
    );
    json_object_object_add(
        root.get(),
        "service",
        json_object_new_string(service_name)
    );
    json_object_object_add(
        root.get(),
        "version",
        json_object_new_string(app_version)
    );
    json_object_object_add(
        root.get(),
        "node_id",
        json_object_new_string(identity.node_id.c_str())
    );
    json_object_object_add(
        root.get(),
        "callsign",
        json_object_new_string(identity.callsign.c_str())
    );
    json_object_object_add(
        root.get(),
        "bind_address",
        json_object_new_string(bind_address.c_str())
    );
    json_object_object_add(
        root.get(),
        "port",
        json_object_new_int(static_cast<int>(port))
    );
    json_object_object_add(
        root.get(),
        "time_offset_ms",
        json_object_new_int64(current_time_offset_ms())
    );

    return serialize_json(root.get());
}

}  // namespace

HttpApi::HttpApi(
    event_base* event_base,
    NodeIdentity identity,
    std::string bind_address,
    std::uint16_t port,
    PeerRegistry& peers,
    MessageService& messages,
    EventStream& events
)
    : identity_(std::move(identity)),
      bind_address_(std::move(bind_address)),
      port_(port),
      peers_(peers),
      messages_(messages),
      events_(events) {
    http_server_.reset(evhttp_new(event_base));
    if (!http_server_) {
        throw std::runtime_error("Failed to create HTTP server");
    }

    evhttp_set_gencb(
        http_server_.get(),
        &HttpApi::handle_request,
        this
    );

    if (evhttp_bind_socket(
            http_server_.get(),
            bind_address_.c_str(),
            port_
        ) != 0) {
        throw std::runtime_error(
            "Failed to bind HTTP server to " +
            bind_address_ + ":" +
            std::to_string(port_)
        );
    }
}

HttpApi::~HttpApi() = default;

void HttpApi::handle_request(
    evhttp_request* request,
    void* context
) {
    auto* api = static_cast<HttpApi*>(context);

    try {
        api->process_request(request);
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

void HttpApi::process_request(evhttp_request* request) {
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

    static const Route routes[] = {
        {EVHTTP_REQ_GET, "/", &HttpApi::serve_index},
        {EVHTTP_REQ_GET, "/style.css", &HttpApi::serve_stylesheet},
        {EVHTTP_REQ_GET, "/app.js", &HttpApi::serve_script},
        {EVHTTP_REQ_GET, "/healthz", &HttpApi::get_healthz},
        {EVHTTP_REQ_GET, "/api/v1/status", &HttpApi::get_status},
        {EVHTTP_REQ_GET, "/api/v1/messages", &HttpApi::get_messages},
        {EVHTTP_REQ_POST, "/api/v1/messages", &HttpApi::create_message},
        {EVHTTP_REQ_GET, "/api/v1/peers", &HttpApi::get_peers},
        {EVHTTP_REQ_POST, "/api/v1/replicate", &HttpApi::replicate_message},
        {EVHTTP_REQ_POST, "/api/v1/sync/summary", &HttpApi::sync_summary},
        {EVHTTP_REQ_POST, "/api/v1/sync/messages", &HttpApi::sync_messages},
        {EVHTTP_REQ_GET, "/api/v1/events", &HttpApi::open_event_stream},
    };

    bool path_exists = false;
    for (const Route& route : routes) {
        if (route.path != path) {
            continue;
        }

        path_exists = true;
        if (route.method == method) {
            (this->*route.handler)(request);
            return;
        }
    }

    if (path_exists) {
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

void HttpApi::serve_index(evhttp_request* request) {
    send_web_asset(request, "index.html", "text/html; charset=utf-8");
}

void HttpApi::serve_stylesheet(evhttp_request* request) {
    send_web_asset(request, "style.css", "text/css; charset=utf-8");
}

void HttpApi::serve_script(evhttp_request* request) {
    send_web_asset(request, "app.js", "text/javascript; charset=utf-8");
}

void HttpApi::get_healthz(evhttp_request* request) {
    const std::string body =
        R"({"status":"ok","service":"les-chatd","version":")" +
        std::string{app_version} + R"("})";
    send_json(
        request,
        HTTP_OK,
        "OK",
        body
    );
}

void HttpApi::get_status(evhttp_request* request) {
    send_json(
        request,
        HTTP_OK,
        "OK",
        make_status_json(identity_, bind_address_, port_)
    );
}

void HttpApi::get_messages(evhttp_request* request) {
    JsonPtr root = make_json_object();
    JsonPtr items = make_json_array();
    const std::vector<ChatMessage>& messages = messages_.messages();
    std::vector<ChatMessage> sorted_messages = messages;
    std::stable_sort(
        sorted_messages.begin(),
        sorted_messages.end(),
        [](const ChatMessage& left, const ChatMessage& right) {
            if (left.created_at_ms != right.created_at_ms) {
                return left.created_at_ms < right.created_at_ms;
            }
            if (left.origin != right.origin) {
                return left.origin < right.origin;
            }
            if (left.sequence != right.sequence) {
                return left.sequence < right.sequence;
            }
            return left.message_id < right.message_id;
        }
    );

    for (const ChatMessage& message : sorted_messages) {
        JsonPtr item = message_to_json(message);
        json_object_array_add(items.get(), item.release());
    }

    json_object_object_add(root.get(), "messages", items.release());
    json_object_object_add(
        root.get(),
        "count",
        json_object_new_int64(static_cast<std::int64_t>(sorted_messages.size()))
    );
    send_json(request, HTTP_OK, "OK", serialize_json(root.get()));
}

void HttpApi::create_message(evhttp_request* request) {
    JsonPtr parsed = read_json_object(
        request,
        max_request_bytes,
        false,
        R"({"error":"invalid_json"})"
    );
    if (!parsed) {
        return;
    }

    json_object* body_value = nullptr;
    if (!json_object_object_get_ex(parsed.get(), "body", &body_value) ||
        body_value == nullptr ||
        !json_object_is_type(body_value, json_type_string)) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            R"({"error":"body_must_be_string"})"
        );
        return;
    }

    const char* body_data = json_object_get_string(body_value);
    const int body_length = json_object_get_string_len(body_value);
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

    const ChatMessage message = messages_.create_local(body);
    JsonPtr response = make_json_object();
    JsonPtr message_json = message_to_json(message);
    json_object_object_add(response.get(), "message", message_json.release());
    send_json(request, 201, "Created", serialize_json(response.get()));
}

void HttpApi::get_peers(evhttp_request* request) {
    const std::vector<Peer> peers =
        peers_.active_peers(current_time_ms());
    JsonPtr root = make_json_object();
    JsonPtr items = make_json_array();

    for (const Peer& peer : peers) {
        JsonPtr item = make_json_object();
        json_object_object_add(
            item.get(),
            "node_id",
            json_object_new_string(peer.node_id.c_str())
        );
        json_object_object_add(
            item.get(),
            "callsign",
            json_object_new_string(peer.callsign.c_str())
        );
        json_object_object_add(
            item.get(),
            "address",
            json_object_new_string(peer.address.c_str())
        );
        json_object_object_add(
            item.get(),
            "http_port",
            json_object_new_int(static_cast<int>(peer.http_port))
        );
        json_object_object_add(
            item.get(),
            "app_version",
            json_object_new_string(peer.app_version.c_str())
        );
        json_object_object_add(
            item.get(),
            "last_seen_ms",
            json_object_new_int64(peer.last_seen_ms)
        );
        json_object_array_add(items.get(), item.release());
    }

    json_object_object_add(root.get(), "peers", items.release());
    json_object_object_add(
        root.get(),
        "count",
        json_object_new_int64(static_cast<std::int64_t>(peers.size()))
    );
    send_json(request, HTTP_OK, "OK", serialize_json(root.get()));
}

void HttpApi::replicate_message(evhttp_request* request) {
    JsonPtr parsed = read_json_object(
        request,
        max_request_bytes,
        true,
        R"({"error":"invalid_message"})"
    );
    if (!parsed) {
        return;
    }

    ChatMessage message{};
    if (!parse_message(parsed.get(), message)) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            R"({"error":"invalid_message"})"
        );
        return;
    }

    const bool accepted = messages_.ingest_replica(message);
    send_json(
        request,
        HTTP_OK,
        "OK",
        accepted ? R"({"status":"accepted"})"
                 : R"({"status":"duplicate"})"
    );
}

void HttpApi::sync_summary(evhttp_request* request) {
    JsonPtr parsed = read_json_object(
        request,
        max_request_bytes,
        true,
        R"({"error":"invalid_sync_summary"})"
    );
    if (!parsed) {
        return;
    }

    json_object* protocol = nullptr;
    json_object* type = nullptr;
    json_object* origins = nullptr;
    if (!json_object_object_get_ex(parsed.get(), "protocol", &protocol) ||
        !json_object_is_type(protocol, json_type_int) ||
        json_object_get_int(protocol) != protocol_version ||
        !json_object_object_get_ex(parsed.get(), "type", &type) ||
        !json_object_is_type(type, json_type_string) ||
        std::string_view{json_object_get_string(type)} != "sync_summary" ||
        !json_object_object_get_ex(parsed.get(), "origins", &origins) ||
        !json_object_is_type(origins, json_type_object)) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            R"({"error":"invalid_sync_summary"})"
        );
        return;
    }

    JsonPtr response = make_json_object();
    JsonPtr local_origins = make_json_object();
    for (const auto& [origin, sequence] : messages_.sequence_summary()) {
        json_object_object_add(
            local_origins.get(),
            origin.c_str(),
            json_object_new_int64(static_cast<std::int64_t>(sequence))
        );
    }
    json_object_object_add(
        response.get(),
        "protocol",
        json_object_new_int(protocol_version)
    );
    json_object_object_add(
        response.get(),
        "type",
        json_object_new_string("sync_summary")
    );
    json_object_object_add(
        response.get(),
        "origins",
        local_origins.release()
    );
    send_json(request, HTTP_OK, "OK", serialize_json(response.get()));
}

void HttpApi::sync_messages(evhttp_request* request) {
    JsonPtr parsed = read_json_object(
        request,
        max_sync_request_bytes,
        true,
        R"({"error":"invalid_sync_messages"})"
    );
    if (!parsed) {
        return;
    }

    json_object* protocol = nullptr;
    json_object* type = nullptr;
    json_object* messages = nullptr;
    if (!json_object_object_get_ex(parsed.get(), "protocol", &protocol) ||
        !json_object_is_type(protocol, json_type_int) ||
        json_object_get_int(protocol) != protocol_version ||
        !json_object_object_get_ex(parsed.get(), "type", &type) ||
        !json_object_is_type(type, json_type_string) ||
        std::string_view{json_object_get_string(type)} != "sync_messages" ||
        !json_object_object_get_ex(parsed.get(), "messages", &messages) ||
        !json_object_is_type(messages, json_type_array) ||
        json_object_array_length(messages) >
            static_cast<int>(max_sync_batch_messages)) {
        send_json(
            request,
            HTTP_BADREQUEST,
            "Bad Request",
            R"({"error":"invalid_sync_messages"})"
        );
        return;
    }

    std::size_t accepted = 0;
    const std::size_t count =
        static_cast<std::size_t>(json_object_array_length(messages));
    for (std::size_t index = 0; index < count; ++index) {
        ChatMessage message{};
        if (!parse_message(
                json_object_array_get_idx(
                    messages,
                    static_cast<int>(index)
                ),
                message
            )) {
            send_json(
                request,
                HTTP_BADREQUEST,
                "Bad Request",
                R"({"error":"invalid_sync_message"})"
            );
            return;
        }
        if (messages_.ingest_sync(std::move(message))) {
            ++accepted;
        }
    }

    send_json(
        request,
        HTTP_OK,
        "OK",
        "{\"status\":\"accepted\",\"count\":" +
            std::to_string(accepted) + "}"
    );
}

void HttpApi::open_event_stream(evhttp_request* request) {
    events_.subscribe(request);
}

}  // namespace leschat
