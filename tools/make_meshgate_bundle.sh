#!/bin/sh

set -eu

usage() {
	echo "usage: $0 USIGN FEEDS_DIR OUTPUT_DIR VERSION RELEASE SIGNING_KEY PUBLIC_KEY" >&2
	exit 2
}

[ "$#" -eq 7 ] || usage

usign=$1
feeds_dir=$2
output_dir=$3
version=$4
release=$5
signing_key=$6
public_key=$7
repo_root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
bundle_name=les-chat-meshgate-feed-${version}-r${release}
bundle_dir=$output_dir/$bundle_name
archive=$output_dir/$bundle_name.tar.gz

[ -x "$usign" ] || {
	echo "usign is missing: $usign" >&2
	exit 1
}

for arch in aarch64_cortex-a72 aarch64_cortex-a76; do
	[ -f "$feeds_dir/$arch/Packages" ] || {
		echo "signed feed is missing for $arch" >&2
		exit 1
	}
	[ -f "$feeds_dir/$arch/Packages.sig" ] || {
		echo "feed signature is missing for $arch" >&2
		exit 1
	}
done

if [ -e "$bundle_dir" ] || [ -e "$archive" ]; then
	echo "MeshGate bundle output already exists: $bundle_name" >&2
	exit 1
fi

mkdir -p "$bundle_dir/feed/24.10.2/stable" \
	"$bundle_dir/feed"
cp -a "$feeds_dir/aarch64_cortex-a72" \
	"$bundle_dir/feed/24.10.2/stable/"
cp -a "$feeds_dir/aarch64_cortex-a76" \
	"$bundle_dir/feed/24.10.2/stable/"
cp "$public_key" "$bundle_dir/opkg.pub"
cp "$public_key" "$bundle_dir/feed/opkg.pub"
cp "$repo_root/openwrt/meshgate/setup.sh" "$bundle_dir/setup.sh"
cp "$repo_root/openwrt/meshgate/les-chat-feed.init" \
	"$bundle_dir/les-chat-feed.init"
cp "$repo_root/openwrt/meshgate/les-chat-routing.init" \
	"$bundle_dir/les-chat-routing.init"
sed \
	-e 's|@FEED_BASE_URL@|http://127.0.0.1:8088|g' \
	-e 's|@OPENWRT_RELEASE@|24.10.2|g' \
	"$repo_root/openwrt/feed/install.sh.in" > "$bundle_dir/feed/install.sh"
chmod 0755 "$bundle_dir/setup.sh" "$bundle_dir/les-chat-feed.init" \
	"$bundle_dir/feed/install.sh"

(
	cd "$bundle_dir"
	find feed -type f -print | LC_ALL=C sort > .feed-files
	# shellcheck disable=SC2046
	sha256sum setup.sh les-chat-feed.init les-chat-routing.init opkg.pub $(cat .feed-files) \
		> SHA256SUMS
	rm .feed-files
)
"$usign" -S -m "$bundle_dir/SHA256SUMS" -s "$signing_key" \
	-x "$bundle_dir/SHA256SUMS.sig"
"$usign" -V -m "$bundle_dir/SHA256SUMS" -p "$public_key" \
	-x "$bundle_dir/SHA256SUMS.sig"

tar -C "$output_dir" -czf "$archive" "$bundle_name"
(
	cd "$output_dir"
	sha256sum "$bundle_name.tar.gz" > "$bundle_name.tar.gz.sha256"
)

echo "$archive"
