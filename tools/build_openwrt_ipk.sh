#!/bin/sh

set -eu

usage() {
	echo "usage: $0 SDK_DIR OUTPUT_DIR [VERSION [RELEASE]]" >&2
	exit 2
}

[ "$#" -ge 2 ] && [ "$#" -le 4 ] || usage

sdk_dir=$1
output_dir=$2
version=${3:-0.1.0}
release=${4:-2}
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

case "$version" in
	''|*[!0-9A-Za-z.+~-]*)
		echo "invalid package version: $version" >&2
		exit 2
		;;
esac

case "$release" in
	''|*[!0-9]*)
		echo "invalid package release: $release" >&2
		exit 2
		;;
esac

[ -f "$sdk_dir/Makefile" ] || {
	echo "not an OpenWrt SDK directory: $sdk_dir" >&2
	exit 1
}

[ -x "$sdk_dir/scripts/feeds" ] || {
	echo "OpenWrt feeds helper is missing: $sdk_dir/scripts/feeds" >&2
	exit 1
}

mkdir -p "$output_dir" "$sdk_dir/package/les-chatd"
cp -a "$repo_root/openwrt/package/les-chatd/." "$sdk_dir/package/les-chatd/"

(
	cd "$sdk_dir"

	if [ ! -e package/feeds/base/libevent2 ] ||
	   [ ! -e package/feeds/base/libjson-c ] ||
	   [ ! -e package/feeds/packages/sqlite3 ]; then
		./scripts/feeds update base packages
		./scripts/feeds install -p base libevent2 libjson-c
		./scripts/feeds install -p packages libsqlite3
	fi

	if ! find staging_dir -path '*/usr/lib/pkgconfig/libevent.pc' -print -quit | grep -q .; then
		make package/feeds/base/libevent2/compile V=s
	fi

	if ! find staging_dir -path '*/usr/lib/pkgconfig/json-c.pc' -print -quit | grep -q .; then
		make package/feeds/base/libjson-c/compile V=s
	fi

	if ! find staging_dir -path '*/usr/lib/pkgconfig/sqlite3.pc' -print -quit | grep -q .; then
		make package/feeds/packages/sqlite3/compile \
			CONFIG_PACKAGE_libedit=n \
			CONFIG_PACKAGE_sqlite3-cli=n \
			V=s
	fi

	make package/les-chatd/clean
	make package/les-chatd/compile \
		LESCHAT_SOURCE_DIR="$repo_root" \
		LESCHAT_VERSION="$version" \
		LESCHAT_RELEASE="$release" \
		CONFIG_PACKAGE_libedit=n \
		CONFIG_PACKAGE_sqlite3-cli=n \
		V=s
)

ipk_path=$(find "$sdk_dir/bin/packages" -type f \
	-name "les-chatd_${version}-r${release}_*.ipk" -print -quit)

[ -n "$ipk_path" ] || {
	echo "les-chatd IPK was not produced" >&2
	exit 1
}

ipk_name=$(basename "$ipk_path")
cp "$ipk_path" "$output_dir/"
(
	cd "$output_dir"
	sha256sum "$ipk_name" > "$ipk_name.sha256"
)

echo "$output_dir/$ipk_name"
