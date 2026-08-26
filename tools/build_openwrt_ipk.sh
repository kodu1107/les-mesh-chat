#!/bin/sh

set -eu

usage() {
	echo "usage: $0 SDK_DIR OUTPUT_DIR [VERSION [RELEASE]]" >&2
	exit 2
}

if [ "$#" -lt 2 ] || [ "$#" -gt 4 ]; then
	usage
fi

sdk_dir=$1
output_dir=$2
version=${3:-0.1.12}
release=${4:-1}
repo_root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)

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

mkdir -p "$output_dir" \
	"$sdk_dir/package/les-chatd" \
	"$sdk_dir/package/luci-app-les-chat"
cp -a "$repo_root/openwrt/package/les-chatd/." "$sdk_dir/package/les-chatd/"
cp -a "$repo_root/openwrt/package/luci-app-les-chat/." \
	"$sdk_dir/package/luci-app-les-chat/"

(
	cd "$sdk_dir"

	disable_package() {
		option=$1
		sed -i "/^${option}=/d;/^# ${option} is not set$/d" .config
		printf '# %s is not set\n' "$option" >> .config
	}

	if [ ! -e package/feeds/base/libevent2 ] ||
	   [ ! -e package/feeds/base/libjson-c ] ||
	   [ ! -e package/feeds/packages/sqlite3 ]; then
		./scripts/feeds update base packages
		./scripts/feeds install -p base libevent2 libjson-c
		./scripts/feeds install -p packages libsqlite3
	fi

	# A freshly extracted SDK has no .config. Without this initialization,
	# package targets fall back to interactive menuconfig and fail in CI.
	make defconfig
	for option in \
		CONFIG_PACKAGE_libedit \
		CONFIG_PACKAGE_libevent2-core \
		CONFIG_PACKAGE_libevent2-extra \
		CONFIG_PACKAGE_libevent2-mbedtls \
		CONFIG_PACKAGE_libevent2-openssl \
		CONFIG_PACKAGE_libevent2-pthreads \
		CONFIG_PACKAGE_sqlite3-cli
	do
		disable_package "$option"
	done
	make defconfig

	# Register the SDK-provided libc and libgcc before dependency checks run.
	make package/toolchain/compile NO_DEPS=1 V=s

	if ! find staging_dir -path '*/usr/lib/pkgconfig/zlib.pc' -print -quit | grep -q .; then
		make package/feeds/base/zlib/compile NO_DEPS=1 V=s
	fi

	if ! find staging_dir -path '*/usr/lib/pkgconfig/libevent.pc' -print -quit | grep -q .; then
		make package/feeds/base/libevent2/compile NO_DEPS=1 V=s
	fi

	if ! find staging_dir -path '*/usr/lib/pkgconfig/json-c.pc' -print -quit | grep -q .; then
		make package/feeds/base/libjson-c/compile NO_DEPS=1 V=s
	fi

	if ! find staging_dir -path '*/usr/lib/pkgconfig/sqlite3.pc' -print -quit | grep -q .; then
		make package/feeds/packages/sqlite3/compile NO_DEPS=1 V=s
	fi

	make package/les-chatd/clean
	make package/les-chatd/compile \
		LESCHAT_SOURCE_DIR="$repo_root" \
		LESCHAT_VERSION="$version" \
		LESCHAT_RELEASE="$release" \
		NO_DEPS=1 \
		V=s

	make package/luci-app-les-chat/clean
	make package/luci-app-les-chat/compile \
		LESCHAT_SOURCE_DIR="$repo_root" \
		LESCHAT_VERSION="$version" \
		LESCHAT_RELEASE="$release" \
		NO_DEPS=1 \
		V=s
)

copy_ipk() {
	description=$1
	shift
	ipk_path=
	for pattern in "$@"; do
		ipk_path=$(find "$sdk_dir/bin/packages" -type f \
			-name "$pattern" -print -quit)
		[ -z "$ipk_path" ] || break
	done
	[ -n "$ipk_path" ] || {
		echo "$description IPK was not produced" >&2
		exit 1
	}
	ipk_name=$(basename "$ipk_path")
	cp "$ipk_path" "$output_dir/"
	(
		cd "$output_dir"
		sha256sum "$ipk_name" > "$ipk_name.sha256"
	)
	echo "$output_dir/$ipk_name"
}

copy_ipk les-chatd \
	"les-chatd_${version}-r${release}_*.ipk" \
	"les-chatd_${version}-${release}_*.ipk"
copy_ipk luci-app-les-chat \
	"luci-app-les-chat_${version}-r${release}_all.ipk" \
	"luci-app-les-chat_${version}-${release}_all.ipk"
