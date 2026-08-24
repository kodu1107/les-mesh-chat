#include "leschat/message_codec.hpp"

#include "leschat/protocol.hpp"
#include "leschat/utf8.hpp"

#include <cstdint>

namespace leschat {

bool parse_message(json_object* object, ChatMessage& message) {
    json_object* sequence = nullptr;
    json_object* created_at = nullptr;
    if (!json_object_is_type(object, json_type_object) ||
        !json_get_string(object, "id", message.message_id) ||
        !json_get_string(object, "origin", message.origin) ||
        !json_get_string(object, "callsign", message.callsign) ||
        !json_get_string(object, "channel", message.channel) ||
        !json_get_string(object, "body", message.body) ||
        !json_object_object_get_ex(object, "sequence", &sequence) ||
        !json_object_is_type(sequence, json_type_int) ||
        !json_object_object_get_ex(object, "created_at_ms", &created_at) ||
        !json_object_is_type(created_at, json_type_int)) {
        return false;
    }

    const std::int64_t sequence_value = json_object_get_int64(sequence);
    if (sequence_value < 1 || message.message_id.empty() ||
        message.message_id.size() > 160U || message.origin.empty() ||
        message.origin.size() > 64U || message.callsign.empty() ||
        message.callsign.size() > 64U ||
        message.channel != default_channel ||
        message.body.empty() || message.body.size() > max_message_bytes ||
        !is_valid_utf8(message.body)) {
        return false;
    }

    message.sequence = static_cast<std::uint64_t>(sequence_value);
    message.created_at_ms = json_object_get_int64(created_at);
    return true;
}

JsonPtr message_to_json(const ChatMessage& message) {
    JsonPtr object = make_json_object();

    json_object_object_add(
        object.get(),
        "sequence",
        json_object_new_int64(
            static_cast<std::int64_t>(message.sequence)
        )
    );
    json_object_object_add(
        object.get(),
        "id",
        json_object_new_string(message.message_id.c_str())
    );
    json_object_object_add(
        object.get(),
        "callsign",
        json_object_new_string(message.callsign.c_str())
    );
    json_object_object_add(
        object.get(),
        "channel",
        json_object_new_string(message.channel.c_str())
    );
    json_object_object_add(
        object.get(),
        "created_at_ms",
        json_object_new_int64(message.created_at_ms)
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
        json_object_new_string(message.origin.c_str())
    );

    return object;
}

}  // namespace leschat
