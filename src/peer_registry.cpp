#include "leschat/peer_registry.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace leschat {

namespace {
constexpr std::int64_t peer_timeout_ms =
    std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::seconds{20}
    ).count();
}

void PeerRegistry::update(Peer peer) {
    peers_.insert_or_assign(peer.node_id, std::move(peer));
}

std::vector<Peer> PeerRegistry::active_peers(
    std::int64_t now_ms
) const {
    std::vector<Peer> result;

    for (const auto& [node_id, peer] : peers_) {
        static_cast<void>(node_id);
        const std::int64_t age_ms = now_ms - peer.last_seen_ms;

        if (age_ms >= 0 && age_ms <= peer_timeout_ms) {
            result.push_back(peer);
        }
    }

    std::ranges::sort(
        result,
        {},
        &Peer::node_id
    );
    return result;
}

}  // namespace leschat
