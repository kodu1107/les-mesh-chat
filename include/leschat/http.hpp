#pragma once

#include <cstddef>
#include <string>
#include <string_view>

struct evhttp_request;

namespace leschat {

void send_response(
    evhttp_request* request,
    int status,
    const char* reason,
    std::string_view content_type,
    std::string_view body
);

void send_json(
    evhttp_request* request,
    int status,
    const char* reason,
    std::string_view body
);

void send_web_asset(
    evhttp_request* request,
    std::string_view filename,
    std::string_view content_type
);

std::string request_path(evhttp_request* request);
std::string read_request_body(
    evhttp_request* request,
    std::size_t limit
);
bool has_json_content_type(evhttp_request* request);

}  // namespace leschat
