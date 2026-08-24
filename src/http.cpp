#include "leschat/http.hpp"

#include <event2/buffer.h>
#include <event2/http.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef LESCHAT_WEB_ROOT
#define LESCHAT_WEB_ROOT "web"
#endif

namespace leschat {

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

namespace {

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

}  // namespace

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
    evhttp_request* request,
    std::size_t limit
) {
    evbuffer* input =
        evhttp_request_get_input_buffer(request);

    const std::size_t length =
        evbuffer_get_length(input);

    if (length > limit) {
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

bool has_json_content_type(evhttp_request* request) {
    const char* content_type = evhttp_find_header(
        evhttp_request_get_input_headers(request),
        "Content-Type"
    );

    return content_type != nullptr &&
           std::string_view{content_type}.find(
               "application/json"
           ) == 0;
}

}  // namespace leschat
