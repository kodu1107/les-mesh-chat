#!/bin/sh

set -eu

usage() {
	echo "usage: $0 [PORT [FIREWALL_ZONE]]" >&2
	exit 2
}

[ "$#" -le 2 ] || usage

port=${1:-8088}
firewall_zone=${2:-ahwlan}
bundle_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
install_root=/srv/les-chat-feed
stage_root=/srv/les-chat-feed.new.$$
trusted_key_id=9db1776b78018b98

case "$port" in
	''|*[!0-9]*) usage ;;
esac
[ "$port" -ge 1 ] && [ "$port" -le 65535 ] || usage

case "$firewall_zone" in
	''|*[!A-Za-z0-9_-]*)
		echo "invalid firewall zone: $firewall_zone" >&2
		exit 2
		;;
esac

[ -r /etc/openwrt_release ] || {
	echo "this installer must run on OpenWrt" >&2
	exit 1
}

if [ ! -x /usr/sbin/uhttpd ] && \
	! /bin/busybox --list 2>/dev/null | grep -qx httpd; then
	echo "MeshGate needs uhttpd or the BusyBox httpd applet" >&2
	exit 1
fi

key_id=$(usign -F -p "$bundle_dir/opkg.pub")
[ "$key_id" = "$trusted_key_id" ] || {
	echo "untrusted LES Mesh Chat signing key: ${key_id:-unreadable}" >&2
	exit 1
}

usign -V -m "$bundle_dir/SHA256SUMS" -p "$bundle_dir/opkg.pub" \
	-x "$bundle_dir/SHA256SUMS.sig"
(
	cd "$bundle_dir"
	sha256sum -c SHA256SUMS
)

trap 'rm -rf "$stage_root"' EXIT INT TERM
mkdir -p "$stage_root"
cp -a "$bundle_dir/feed/." "$stage_root/"

rm -rf "$install_root.old"
if [ -d "$install_root" ]; then
	mv "$install_root" "$install_root.old"
fi
mv "$stage_root" "$install_root"
rm -rf "$install_root.old"
trap - EXIT INT TERM

cp "$bundle_dir/les-chat-feed.init" /etc/init.d/les-chat-feed
chmod 0755 /etc/init.d/les-chat-feed

touch /etc/config/les-chat-feed
uci -q delete les-chat-feed.main || true
uci set les-chat-feed.main=service
uci set les-chat-feed.main.root="$install_root"
uci set les-chat-feed.main.port="$port"
uci commit les-chat-feed

uci -q delete firewall.les_chat_feed || true
uci set firewall.les_chat_feed=rule
uci set firewall.les_chat_feed.name='LES Chat package feed'
uci set firewall.les_chat_feed.src="$firewall_zone"
uci set firewall.les_chat_feed.proto=tcp
uci set firewall.les_chat_feed.dest_port="$port"
uci set firewall.les_chat_feed.target=ACCEPT
uci commit firewall
/etc/init.d/firewall reload

/etc/init.d/les-chat-feed enable
/etc/init.d/les-chat-feed restart
sleep 1
wget -qO- "http://127.0.0.1:$port/opkg.pub" >/dev/null

echo "LES Mesh Chat feed is running on TCP port $port."
echo "Only firewall zone $firewall_zone can reach it."
