# LES Mesh Chat

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-brightgreen.svg)](CMakeLists.txt)
[![Target Platform](https://img.shields.io/badge/Platform-OpenMANET%20%7C%20OpenWrt%2024.10-orange.svg)](openwrt/README.md)
[![Release](https://img.shields.io/badge/Release-v0.1.16-informational.svg)](https://github.com/kodu1107/les-mesh-chat/releases/tag/v0.1.16)

**LES Mesh Chat** is an offline-first, decentralized P2P text communication system designed for **OpenMANET** and **OpenWrt** routers over **802.11ah HaLow** and **BATMAN-adv** mesh networks. It operates entirely without internet access, DNS, or central servers.

---

## 🌟 Key Features

* **🌐 Offline-First & Fully Decentralized:** Operates purely in ad-hoc mesh environments without reliance on external cloud or gateway infrastructure.
* **🔍 UDP Peer Discovery:** Automatically discovers neighboring mesh nodes via periodic UDP broadcasts (port 7777).
* **⚡ Instant Replication & Catch-up Sync:** Pushes new messages to active peers immediately via HTTP POST; recovers missed sequence ranges automatically upon node reconnection.
* **💾 Bounded SQLite Store:** Persistent message storage strictly bounded to 10 MiB with FIFO trimming and deduplication `(origin_node_id, sequence)`.
* **🖥️ Dual UI Modes:**
  * **Built-in Web UI:** Standalone browser-accessible dark UI hosted directly on `http://<node-ip>:7777/` (SSE-driven real-time updates).
  * **Native LuCI Integration:** Integrated OpenWrt admin dashboard (`luci-app-les-chat`) with Chat, Peers, Status, and Settings views.
* **⏱️ MeshGate Time Synchronization:** Synchronizes local clocks or adjusts message timestamp offsets against an internal MeshGate authority without NTP.
* **🔀 Overlapping IP & Mesh Routing Safety:** Outgoing HTTP replication and sync traffic are explicitly bound to the mesh interface (e.g. `br-ahwlan`), preventing packet loss in overlapping LAN/HaLow subnets.

---

## 🏗️ Architecture

```text
┌────────────────────────────────────────────────────────┐
│                      Client Device                     │
│  [Browser: http://<ip>:7777]  OR  [OpenWrt LuCI App]  │
└───────────────────────────┬────────────────────────────┘
                            │ HTTP API / SSE Events
┌───────────────────────────▼────────────────────────────┐
│                    les-chatd (C++20)                   │
│ ┌──────────────────────┐      ┌──────────────────────┐ │
│ │  HttpApi & Web UI    │      │  DiscoveryService    │ │
│ │  (libevent HTTP)     │      │  (UDP 7777 Announce) │ │
│ └──────────┬───────────┘      └──────────┬───────────┘ │
│            │                             │             │
│ ┌──────────▼───────────┐      ┌──────────▼───────────┐ │
│ │  MessageService      │◄────►│  PeerRegistry        │ │
│ │  & EventStream (SSE) │      │  (Node / Peer state) │ │
│ └──────────┬───────────┘      └──────────┬───────────┘ │
│            │                             │             │
│ ┌──────────▼───────────┐      ┌──────────▼───────────┐ │
│ │  MessageStore        │      │ Replication & Sync   │ │
│ │  (SQLite3 / 10MB)    │      │ (Outbound HTTP)      │ │
│ └──────────────────────┘      └──────────┬───────────┘ │
└──────────────────────────────────────────┼─────────────┘
                                           │ UDP / TCP 7777
┌──────────────────────────────────────────▼─────────────┐
│             BATMAN-adv / 802.11ah HaLow Mesh           │
│                 (Remote les-chatd Peers)               │
└────────────────────────────────────────────────────────┘
```

---

## 🚀 Quick Start (Native Build)

### Prerequisites

* C++20 compatible compiler (GCC 13+ or Clang 16+)
* CMake 3.20+
* Ninja build system
* `pkg-config`
* Libraries: `libevent`, `json-c`, `sqlite3`

On Debian/Ubuntu:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
    libevent-dev libjson-c-dev libsqlite3-dev
```

### Build & Run

```bash
# 1. Configure and build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel $(nproc)

# 2. Run unit tests
ctest --test-dir build --output-on-failure

# 3. Start a local node
./build/les-chatd \
    --node-id node-a \
    --callsign Alpha \
    --bind 0.0.0.0 \
    --port 7777 \
    --discovery-address 255.255.255.255 \
    --discovery-port 7777 \
    --database data/node-a.db
```

Open **http://127.0.0.1:7777/** in your web browser.

---

## 📦 OpenWrt / OpenMANET Deployment

Target hardware architectures:
* **Raspberry Pi 4:** `bcm27xx/bcm2711` (`aarch64_cortex-a72`)
* **Raspberry Pi 5:** `bcm27xx/bcm2712` (`aarch64_cortex-a76`)
* **LuCI App:** Architecture-independent (`all`)

### Installation Options

| Method | Target Environment | Documentation |
|---|---|---|
| **Online Opkg Feed** | Devices with internet access or GitHub Pages access | [docs/DISTRIBUTION.md](docs/DISTRIBUTION.md) |
| **Offline Windows Tool** | Isolated devices via SSH from a Windows laptop | [docs/OFFLINE_DISTRIBUTION.md](docs/OFFLINE_DISTRIBUTION.md) |
| **MeshGate Internal Feed** | Air-gapped mesh nodes via central MeshGate node (`:8088`) | [docs/OFFLINE_DISTRIBUTION.md](docs/OFFLINE_DISTRIBUTION.md) |
| **Manual SDK Build** | Building custom IPK packages directly from OpenWrt SDK | [openwrt/README.md](openwrt/README.md) |

#### One-Line Installation (Online Feed)
```bash
wget -qO- https://YOUR_ACCOUNT.github.io/les-mesh-chat/install.sh | sh
```

---

## ⚙️ Configuration

OpenWrt configuration is managed via UCI (`/etc/config/les-chat`):

```bash
# Set display callsign (nickname)
uci set les-chat.main.callsign='Bolt'

# Keep the generated ID, or set a unique manual Node ID
uci set les-chat.main.node_id='node-bolt'

# Bind explicit mesh egress interface (recommended for HaLow nodes)
uci set les-chat.main.discovery_interface='br-ahwlan'

# Persistent database storage path
uci set les-chat.main.database='/overlay/les-chat/messages.db'

# Commit and restart daemon
uci commit les-chat
/etc/init.d/les-chatd restart
```

---

## 🔌 REST & SSE API Reference

`les-chatd` exposes a lightweight REST API on port `7777`:

| Endpoint | Method | Description |
|---|---|---|
| `/healthz` | `GET` | Health check endpoint (`{"status":"ok"}`) |
| `/api/v1/status` | `GET` | Current node identity, version, uptime, and database size |
| `/api/v1/peers` | `GET` | List of currently active peers discovered via UDP |
| `/api/v1/messages` | `GET` | Retrieve stored messages (supports `limit` and `before_sequence`) |
| `/api/v1/messages` | `POST` | Send a new message (broadcasts to peers and notifies SSE) |
| `/api/v1/sync` | `GET` | Fetch missed messages by origin node ID and sequence range |
| `/api/v1/events` | `GET` | Server-Sent Events (SSE) stream for real-time messages & peer updates |

---

## 📚 Project Documentation

* [**System Architecture & Plan**](LES_MESH_CHAT_CPP_PROJECT_PLAN.md): In-depth C++20 architecture, lifecycle, and network design specs.
* [**OpenWrt Package & LuCI Guide**](openwrt/README.md): IPK compilation, LuCI app design, and firewall rules.
* [**GitHub & Opkg Feed Release Guide**](docs/DISTRIBUTION.md): Automated CI/CD pipeline, `usign` key management, and GitHub Pages release.
* [**Offline & MeshGate Distribution Guide**](docs/OFFLINE_DISTRIBUTION.md): Deployment workflow for air-gapped mesh fields using Windows tools or MeshGate internal feed.

---

## 📄 License

LES Mesh Chat is open source software licensed under the [MIT License](LICENSE).  
Third-party component notices and licenses are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
