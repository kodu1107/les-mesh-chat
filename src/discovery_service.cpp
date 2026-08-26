#include "leschat/discovery_service.hpp"

#include "leschat/clock.hpp"
#include "leschat/json.hpp"
#include "leschat/peer.hpp"
#include "leschat/peer_registry.hpp"
#include "leschat/protocol.hpp"

#include <arpa/inet.h>
#include <event2/event.h>
#include <event2/util.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace leschat {

DiscoveryService::DiscoveryService(
    event_base* event_base,
    PeerRegistry& registry,
    NodeIdentity identity,
    std::uint16_t http_port,
    std::string discovery_address,
    std::string discovery_interface,
    std::uint16_t discovery_port,
    TimeSyncMode time_sync_mode,
    std::string time_authority_id
)
    : registry_(registry),
      identity_(std::move(identity)),
      http_port_(http_port),
      discovery_address_(std::move(discovery_address)),
      discovery_interface_(std::move(discovery_interface)),
      discovery_port_(discovery_port),
      time_sync_mode_(time_sync_mode),
      time_authority_id_(std::move(time_authority_id)) {
    try {
        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_ < 0) {
            throw std::runtime_error("Failed to create discovery socket");
        }

        const int enabled = 1;
        if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                       &enabled, sizeof(enabled)) != 0 ||
            setsockopt(socket_, SOL_SOCKET, SO_BROADCAST,
                       &enabled, sizeof(enabled)) != 0 ||
            evutil_make_socket_nonblocking(socket_) != 0) {
            throw std::runtime_error("Failed to configure discovery socket");
        }

        sockaddr_in bind_address{};
        bind_address.sin_family = AF_INET;
        bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
        bind_address.sin_port = htons(discovery_port_);
        if (bind(socket_, reinterpret_cast<sockaddr*>(&bind_address),
                 sizeof(bind_address)) != 0) {
            throw std::runtime_error("Failed to bind discovery UDP port " +
                                     std::to_string(discovery_port_));
        }

        in_addr parsed_address{};
        if (inet_pton(AF_INET, discovery_address_.c_str(),
                      &parsed_address) != 1) {
            throw std::runtime_error("Invalid discovery IPv4 address: " +
                                     discovery_address_);
        }

        receive_event_ = event_new(
            event_base, socket_, EV_READ | EV_PERSIST,
            &DiscoveryService::receive_callback, this
        );
        announce_event_ = event_new(
            event_base, -1, EV_PERSIST,
            &DiscoveryService::announce_callback, this
        );
        if (receive_event_ == nullptr || announce_event_ == nullptr) {
            throw std::runtime_error("Failed to create discovery events");
        }

        const timeval interval{5, 0};
        if (event_add(receive_event_, nullptr) != 0 ||
            event_add(announce_event_, &interval) != 0) {
            throw std::runtime_error("Failed to activate discovery events");
        }
        announce();
    } catch (...) {
        close_resources();
        throw;
    }
}

DiscoveryService::~DiscoveryService() {
    close_resources();
}

void DiscoveryService::close_resources() noexcept {
    if (announce_event_ != nullptr) {
        event_free(announce_event_);
        announce_event_ = nullptr;
    }
    if (receive_event_ != nullptr) {
        event_free(receive_event_);
        receive_event_ = nullptr;
    }
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
}

void DiscoveryService::receive_callback(
    int, short, void* context
) noexcept {
    try {
        static_cast<DiscoveryService*>(context)->receive_announcements();
    } catch (const std::exception& error) {
        std::cerr << "Discovery receive error: " << error.what() << '\n';
    }
}

void DiscoveryService::announce_callback(
    int, short, void* context
) noexcept {
    static_cast<DiscoveryService*>(context)->announce();
}

void DiscoveryService::announce() noexcept {
    try {
        send_announce();
        if (announce_failed_) {
            std::cerr << "Discovery announce recovered\n";
        }
        announce_failed_ = false;
    } catch (const std::exception& error) {
        if (!announce_failed_) {
            std::cerr
                << "Discovery announce error: "
                << error.what()
                << '\n';
        }
        announce_failed_ = true;
    }
}

void DiscoveryService::receive_announcements() {
    std::array<char, max_datagram_bytes + 1U> buffer{};

    for (;;) {
        sockaddr_in sender{};
        socklen_t sender_length = sizeof(sender);
        const ssize_t received = recvfrom(
            socket_, buffer.data(), buffer.size(), 0,
            reinterpret_cast<sockaddr*>(&sender), &sender_length
        );
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            throw std::runtime_error(
                std::string{"recvfrom failed: "} + std::strerror(errno)
            );
        }
        if (static_cast<std::size_t>(received) > max_datagram_bytes) {
            continue;
        }

        JsonParse parsed = parse_json(
            std::string_view{buffer.data(), static_cast<std::size_t>(received)}
        );
        if (parsed.error != json_tokener_success ||
            parsed.parse_end != static_cast<std::size_t>(received) ||
            !json_is_object(parsed.object.get())) {
            continue;
        }

        json_object* protocol = nullptr;
        json_object* port = nullptr;
        json_object* authority = nullptr;
        json_object* authority_time = nullptr;
        std::string type;
        Peer peer{};
        if (!json_object_object_get_ex(parsed.object.get(), "protocol",
                                       &protocol) ||
            !json_object_is_type(protocol, json_type_int) ||
            json_object_get_int(protocol) != protocol_version ||
            !json_get_string(parsed.object.get(), "type", type) ||
            type != "announce" ||
            !json_get_string(parsed.object.get(), "node_id", peer.node_id) ||
            peer.node_id.empty() ||
            !json_get_string(parsed.object.get(), "callsign", peer.callsign) ||
            !json_get_string(
                parsed.object.get(), "app_version", peer.app_version
            ) ||
            !json_object_object_get_ex(
                parsed.object.get(), "http_port", &port
            ) ||
            !json_object_is_type(port, json_type_int)) {
            continue;
        }
        const std::int64_t port_value = json_object_get_int64(port);
        if (port_value < 1 || port_value > 65535 ||
            peer.node_id == identity_.node_id) {
            continue;
        }

        bool is_time_authority = false;
        std::int64_t authority_time_ms = 0;
        if (json_object_object_get_ex(
                parsed.object.get(), "time_authority", &authority
            ) && json_object_is_type(authority, json_type_boolean)) {
            is_time_authority = json_object_get_boolean(authority) != 0;
            if (is_time_authority &&
                (!json_object_object_get_ex(
                    parsed.object.get(), "time_ms", &authority_time
                ) || !json_object_is_type(
                    authority_time, json_type_int
                ))) {
                continue;
            }
            if (is_time_authority) {
                authority_time_ms = json_object_get_int64(authority_time);
            }
        }

        if (time_sync_mode_ == TimeSyncMode::Client &&
            is_time_authority &&
            (time_authority_id_.empty() ||
             time_authority_id_ == peer.node_id)) {
            synchronize_time(peer.node_id, authority_time_ms);
        }

        std::array<char, INET_ADDRSTRLEN> address_text{};
        if (inet_ntop(AF_INET, &sender.sin_addr, address_text.data(),
                      address_text.size()) == nullptr) {
            continue;
        }
        peer.address = address_text.data();
        peer.http_port = static_cast<std::uint16_t>(port_value);
        peer.last_seen_ms = current_time_ms();
        registry_.update(std::move(peer));
    }
}

void DiscoveryService::send_announce() {
    JsonPtr root = make_json_object();
    json_object_object_add(root.get(), "protocol",
                           json_object_new_int(protocol_version));
    json_object_object_add(root.get(), "type",
                           json_object_new_string("announce"));
    json_object_object_add(root.get(), "node_id",
                           json_object_new_string(identity_.node_id.c_str()));
    json_object_object_add(root.get(), "callsign",
                           json_object_new_string(identity_.callsign.c_str()));
    json_object_object_add(root.get(), "http_port",
                           json_object_new_int(static_cast<int>(http_port_)));
    json_object_object_add(root.get(), "app_version",
                           json_object_new_string(app_version));
    if (time_sync_mode_ == TimeSyncMode::Authority) {
        json_object_object_add(
            root.get(),
            "time_authority",
            json_object_new_boolean(1)
        );
        json_object_object_add(
            root.get(),
            "time_ms",
            json_object_new_int64(current_time_ms())
        );
    }

    const std::string payload = serialize_json(root.get());

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(discovery_port_);
    if (inet_pton(AF_INET, discovery_address_.c_str(),
                  &destination.sin_addr) != 1) {
        throw std::runtime_error("Invalid discovery IPv4 address");
    }
    ssize_t sent{-1};
    if (discovery_interface_.empty()) {
        sent = sendto(
            socket_, payload.data(), payload.size(), 0,
            reinterpret_cast<sockaddr*>(&destination), sizeof(destination)
        );
    } else {
        const unsigned int interface_index =
            if_nametoindex(discovery_interface_.c_str());
        if (interface_index == 0U) {
            throw std::runtime_error(
                "Discovery interface is unavailable: " +
                discovery_interface_
            );
        }
        if (interface_index > static_cast<unsigned int>(
                std::numeric_limits<int>::max()
            )) {
            throw std::runtime_error(
                "Discovery interface index is out of range"
            );
        }

        iovec payload_buffer{};
        payload_buffer.iov_base = const_cast<char*>(payload.data());
        payload_buffer.iov_len = payload.size();

        alignas(cmsghdr)
        std::array<unsigned char, CMSG_SPACE(sizeof(in_pktinfo))>
            control_buffer{};
        msghdr message{};
        message.msg_name = &destination;
        message.msg_namelen = sizeof(destination);
        message.msg_iov = &payload_buffer;
        message.msg_iovlen = 1;
        message.msg_control = control_buffer.data();
        message.msg_controllen = control_buffer.size();

        cmsghdr* control_message = CMSG_FIRSTHDR(&message);
        if (control_message == nullptr) {
            throw std::runtime_error(
                "Failed to configure discovery interface"
            );
        }
        control_message->cmsg_level = IPPROTO_IP;
        control_message->cmsg_type = IP_PKTINFO;
        control_message->cmsg_len = CMSG_LEN(sizeof(in_pktinfo));

        in_pktinfo packet_info{};
        packet_info.ipi_ifindex = static_cast<int>(interface_index);
        std::memcpy(
            CMSG_DATA(control_message),
            &packet_info,
            sizeof(packet_info)
        );

        sent = sendmsg(socket_, &message, 0);
    }
    if (sent < 0) {
        throw std::runtime_error(
            std::string{"Discovery datagram send failed: "} +
            std::strerror(errno)
        );
    }
    if (static_cast<std::size_t>(sent) != payload.size()) {
        throw std::runtime_error(
            "Discovery datagram was not sent completely"
        );
    }
}

void DiscoveryService::synchronize_time(
    std::string_view authority_id,
    std::int64_t authority_time_ms
) {
    if (authority_time_ms <= 0) {
        return;
    }

    const std::int64_t measured_offset =
        authority_time_ms - system_time_ms();
    const std::int64_t current_offset = current_time_offset_ms();
    const std::int64_t offset_delta = measured_offset - current_offset;
    if (offset_delta > -250 && offset_delta < 250) {
        return;
    }

    if (set_system_time_ms(authority_time_ms)) {
        std::cerr
            << "Time synchronized from MeshGate "
            << authority_id
            << " (offset "
            << measured_offset
            << " ms)\n";
        return;
    }

    set_time_offset_ms(measured_offset);
    std::cerr
        << "System clock update unavailable; using MeshGate time offset "
        << measured_offset
        << " ms from "
        << authority_id
        << '\n';
}

}  // namespace leschat
