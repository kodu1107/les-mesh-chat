#pragma once

#include <json-c/json.h>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace leschat {

struct JsonDeleter {
    void operator()(json_object* object) const noexcept {
        if (object != nullptr) {
            json_object_put(object);
        }
    }
};

using JsonPtr = std::unique_ptr<json_object, JsonDeleter>;

struct JsonParse {
    JsonPtr object;
    json_tokener_error error{json_tokener_success};
    std::size_t parse_end{0};
};

JsonPtr make_json_object();
JsonPtr make_json_array();
std::string serialize_json(json_object* object);
JsonParse parse_json(std::string_view text);

bool json_is_object(json_object* object) noexcept;
bool json_get_string(
    json_object* object,
    const char* name,
    std::string& value
);

}  // namespace leschat
