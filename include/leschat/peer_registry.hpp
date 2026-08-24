#pragma once

#include "leschat/peer.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace leschat {

class PeerRegistry {
public:
    void update(Peer peer);
    [[nodiscard]] std::vector<Peer> active_peers(
        std::int64_t now_ms
    ) const;

private:
    std::unordered_map<std::string, Peer> peers_;
};

}  // namespace leschat
