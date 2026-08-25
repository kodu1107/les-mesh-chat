#!/bin/sh

set -eu

usage() {
	echo "usage: $0 SDK_DIR APP_IPK_DIR OUTPUT_DIR VERSION RELEASE ARCH SIGNING_KEY PUBLIC_KEY" >&2
	exit 2
}

[ "$#" -eq 8 ] || usage

sdk_dir=$1
app_ipk_dir=$2
output_dir=$3
version=$4
release=$5
arch=$6
signing_key=$7
public_key=$8
repo_root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
usign=$sdk_dir/staging_dir/host/bin/usign
bundle_name=les-chat-offline-${version}-r${release}-${arch}
bundle_dir=$output_dir/$bundle_name
archive=$output_dir/$bundle_name.tar.gz

[ -x "$usign" ] || {
	echo "usign is missing: $usign" >&2
	exit 1
}

[ -f "$signing_key" ] || {
	echo "signing key is missing: $signing_key" >&2
	exit 1
}

[ -f "$public_key" ] || {
	echo "public key is missing: $public_key" >&2
	exit 1
}

[ ! -e "$bundle_dir" ] && [ ! -e "$archive" ] || {
	echo "offline bundle output already exists: $bundle_name" >&2
	exit 1
}

mkdir -p "$bundle_dir/packages"
cp "$repo_root/openwrt/offline/install.sh" "$bundle_dir/install.sh"
cp "$public_key" "$bundle_dir/opkg.pub"
chmod 0755 "$bundle_dir/install.sh"

"$repo_root/tools/collect_openwrt_runtime_ipks.sh" \
	"$sdk_dir" "$app_ipk_dir" "$bundle_dir/packages" "$arch" >/dev/null

(
	cd "$bundle_dir"
	sha256sum install.sh opkg.pub packages/*.ipk > SHA256SUMS
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
