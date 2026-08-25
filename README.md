# LES Mesh Chat

LES Mesh Chat is an offline-first distributed chat service for OpenMANET and
OpenWrt nodes. Peers discover each other over UDP, replicate messages over
HTTP, recover missed messages after reconnecting, and keep a bounded SQLite
history.

## Native build

Required development packages are CMake, a C++20 compiler, libevent, json-c,
SQLite 3, and pkg-config.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Run a node:

```bash
./build/les-chatd \
    --node-id node-a \
    --callsign Alpha \
    --bind 0.0.0.0 \
    --port 7777 \
    --discovery-address 255.255.255.255 \
    --discovery-port 7777 \
    --database data/node-a.db
```

On an OpenMANET node with overlapping LAN and HaLow routes, add
`--discovery-interface br-ahwlan` to select the mesh interface explicitly.

Open <http://127.0.0.1:7777/> in a browser.

## OpenWrt packages

The package definition, SDK build instructions, service configuration, and
firewall notes are in [openwrt/README.md](openwrt/README.md). Raspberry Pi 4
and Raspberry Pi 5 require separate IPKs:

- Raspberry Pi 4: `bcm27xx/bcm2711`, `aarch64_cortex-a72`
- Raspberry Pi 5: `bcm27xx/bcm2712`, `aarch64_cortex-a76`

Tagged releases are intended to publish both IPKs and update the signed opkg
feed. The one-time GitHub and signing-key setup is documented in
[docs/DISTRIBUTION.md](docs/DISTRIBUTION.md). Development builds should not be
installed on unattended nodes.

## License

LES Mesh Chat is licensed under the [MIT License](LICENSE). Dependency notices
are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
