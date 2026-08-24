#pragma once

#include "leschat/json.hpp"
#include "leschat/message.hpp"

namespace leschat {

bool parse_message(json_object* object, ChatMessage& message);
JsonPtr message_to_json(const ChatMessage& message);

}  // namespace leschat
