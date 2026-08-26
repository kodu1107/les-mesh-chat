#pragma once

#include <cstddef>
#include <cstdint>

namespace leschat {

inline constexpr int protocol_version = 1;
inline constexpr const char* app_version = "0.1.17";
inline constexpr const char* service_name = "les-chatd";
inline constexpr const char* default_channel = "les-manet";

inline constexpr std::size_t max_message_bytes = 2048;
inline constexpr std::size_t max_request_bytes = 4096;
inline constexpr std::size_t max_sync_request_bytes = 128U * 1024U;
inline constexpr std::size_t max_datagram_bytes = 4096;
inline constexpr std::size_t max_sync_batch_messages = 50;
inline constexpr std::uint64_t max_database_bytes = 10U * 1024U * 1024U;

}  // namespace leschat
