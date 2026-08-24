#pragma once

#include "leschat/message.hpp"
#include "leschat/node_identity.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace leschat {

class EventStream;
class MessageStore;
class ReplicationService;

class MessageService {
public:
    MessageService(
        NodeIdentity identity,
        MessageStore& store,
        ReplicationService& replication,
        EventStream& events
    );

    ChatMessage create_local(std::string body);
    [[nodiscard]] bool ingest_replica(const ChatMessage& message);
    [[nodiscard]] bool ingest_sync(ChatMessage message);
    [[nodiscard]] const std::vector<ChatMessage>& messages() const noexcept;
    [[nodiscard]] std::unordered_map<std::string, std::uint64_t>
    sequence_summary() const;

private:
    void publish(const ChatMessage& message);
    void replicate(const ChatMessage& message);

    NodeIdentity identity_;
    MessageStore& store_;
    ReplicationService& replication_;
    EventStream& events_;
    std::uint64_t next_sequence_{1};
};

}  // namespace leschat
