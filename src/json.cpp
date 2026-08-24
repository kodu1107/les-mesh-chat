#include "leschat/json.hpp"

#include <stdexcept>

namespace leschat {

JsonPtr make_json_object() {
    JsonPtr object{json_object_new_object()};
    if (!object) {
        throw std::runtime_error("Unable to create JSON object");
    }
    return object;
}

JsonPtr make_json_array() {
    JsonPtr object{json_object_new_array()};
    if (!object) {
        throw std::runtime_error("Unable to create JSON array");
    }
    return object;
}

std::string serialize_json(json_object* object) {
    const char* serialized = json_object_to_json_string_ext(
        object,
        JSON_C_TO_STRING_PLAIN
    );
    if (serialized == nullptr) {
        throw std::runtime_error("Unable to serialize JSON");
    }
    return serialized;
}

JsonParse parse_json(std::string_view text) {
    json_tokener* tokener = json_tokener_new();
    if (tokener == nullptr) {
        throw std::runtime_error("Unable to create JSON parser");
    }

    JsonParse parsed;
    parsed.object.reset(json_tokener_parse_ex(
        tokener,
        text.data(),
        static_cast<int>(text.size())
    ));
    parsed.error = json_tokener_get_error(tokener);
    parsed.parse_end = json_tokener_get_parse_end(tokener);
    json_tokener_free(tokener);
    return parsed;
}

bool json_is_object(json_object* object) noexcept {
    return object != nullptr &&
           json_object_is_type(object, json_type_object);
}

bool json_get_string(
    json_object* object,
    const char* name,
    std::string& value
) {
    json_object* member = nullptr;
    if (!json_object_object_get_ex(object, name, &member) ||
        !json_object_is_type(member, json_type_string)) {
        return false;
    }

    const char* text = json_object_get_string(member);
    const int length = json_object_get_string_len(member);
    if (text == nullptr || length < 0) {
        return false;
    }

    value.assign(text, static_cast<std::size_t>(length));
    return true;
}

}  // namespace leschat
