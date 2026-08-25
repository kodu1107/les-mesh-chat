#!/bin/sh

set -eu

root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)

required_files='openwrt/package/les-chatd/Makefile
openwrt/package/les-chatd/files/etc/config/les-chat
openwrt/package/les-chatd/files/etc/init.d/les-chatd
openwrt/package/les-chatd/files/etc/uci-defaults/99-les-chat
openwrt/feed/install.sh.in
openwrt/feed/README.md
openwrt/offline/install.sh
openwrt/meshgate/setup.sh
openwrt/meshgate/les-chat-feed.init
openwrt/README.md
tools/build_openwrt_ipk.sh
tools/collect_openwrt_runtime_ipks.sh
tools/make_offline_bundle.sh
tools/make_meshgate_bundle.sh
tools/make_opkg_feed.sh
tools/windows/Install-LES-Chat.cmd
tools/windows/Install-LES-Chat.ps1
tools/windows/Sync-LES-Chat-MeshGate.cmd
tools/windows/Sync-LES-Chat-MeshGate.ps1
docs/OFFLINE_DISTRIBUTION.md
LICENSE
THIRD_PARTY_NOTICES.md'

for file in $required_files; do
	[ -f "$root/$file" ] || {
		echo "missing: $file" >&2
		exit 1
	}
done

grep -q 'opkg install' "$root/openwrt/README.md"
grep -q -- '--database' "$root/openwrt/package/les-chatd/files/etc/init.d/les-chatd"
grep -q '^PKG_LICENSE:=MIT$' "$root/openwrt/package/les-chatd/Makefile"
grep -q '^PKG_LICENSE_FILES:=LICENSE$' "$root/openwrt/package/les-chatd/Makefile"
grep -q '^PKG_BUILD_DEPENDS:=libevent2 libjson-c sqlite3$' \
	"$root/openwrt/package/les-chatd/Makefile"
grep -q '@FEED_BASE_URL@' "$root/openwrt/feed/install.sh.in"
grep -q '@OPENWRT_RELEASE@' "$root/openwrt/feed/install.sh.in"

sh -n \
	"$root/tools/build_openwrt_ipk.sh" \
	"$root/tools/collect_openwrt_runtime_ipks.sh" \
	"$root/tools/make_offline_bundle.sh" \
	"$root/tools/make_meshgate_bundle.sh" \
	"$root/tools/make_opkg_feed.sh" \
	"$root/openwrt/feed/install.sh.in" \
	"$root/openwrt/offline/install.sh" \
	"$root/openwrt/meshgate/setup.sh" \
	"$root/openwrt/meshgate/les-chat-feed.init" \
	"$root/openwrt/package/les-chatd/files/etc/init.d/les-chatd" \
	"$root/openwrt/package/les-chatd/files/etc/uci-defaults/99-les-chat"

echo "OpenWrt package files are structurally valid"
