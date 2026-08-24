#include "leschat/message_service.hpp"

#include "leschat/clock.hpp"
#include "leschat/event_stream.hpp"
#include "leschat/json.hpp"
#include "leschat/message_codec.hpp"
#include "leschat/message_store.hpp"
#include "leschat/protocol.hpp"
#include "leschat/replication_service.hpp"

#include <utility>

namespace leschat {

MessageService::MessageService(
    NodeIdentity identity,
    MessageStore& store,
    ReplicationService& replication,
    EventStream& events
)
    : identity_(std::move(identity)),
      store_(store),
      replication_(replication),
      events_(events),
      next_sequence_(store_.next_sequence(identity_.node_id)) {}

void MessageService::publish(const ChatMessage& message) {
    JsonPtr message_json = message_to_json(message);
    events_.publish("message", serialize_json(message_json.get()));
}

void MessageService::replicate(const ChatMessage& message) {
    JsonPtr payload = message_to_json(message);
    replication_.broadcast(serialize_json(payload.get()));
}

ChatMessage MessageService::create_local(std::string body) {
    ChatMessage message{
        .sequence = next_sequence_,
        .message_id =
            identity_.node_id + ":" + std::to_string(next_sequence_),
        .origin = identity_.node_id,
        .callsign = identity_.callsign,
        .channel = default_channel,
        .created_at_ms = current_time_ms(),
        .body = std::move(body)
    };

    ++next_sequence_;
    static_cast<void>(store_.add(message));
    publish(message);
    replicate(message);
    return message;
}

bool MessageService::ingest_replica(const ChatMessage& message) {
    if (store_.contains(message.message_id)) {
        return false;
    }

    static_cast<void>(store_.add(message));
    publish(message);
    return true;
}

bool MessageService::ingest_sync(ChatMessage message) {
    if (!store_.add(message)) {
        return false;
    }

    publish(message);
    return true;
}

const std::vector<ChatMessage>& MessageService::messages() const noexcept {
    return store_.messages();
}

std::unordered_map<std::string, std::uint64_t>
MessageService::sequence_summary() const {
    return store_.sequence_summary();
}

}  // namespace leschat
