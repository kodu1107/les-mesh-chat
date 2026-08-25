#!/bin/sh

set -eu

usage() {
	echo "usage: $0 SDK_DIR APP_IPK_DIR OUTPUT_DIR ARCH" >&2
	exit 2
}

[ "$#" -eq 4 ] || usage

sdk_dir=$1
app_ipk_dir=$2
output_dir=$3
arch=$4

case "$arch" in
	aarch64_cortex-a72|aarch64_cortex-a76) ;;
	*)
		echo "unsupported package architecture: $arch" >&2
		exit 2
		;;
esac

[ -d "$sdk_dir/bin" ] || {
	echo "OpenWrt SDK package output is missing: $sdk_dir/bin" >&2
	exit 1
}

[ -d "$app_ipk_dir" ] || {
	echo "application IPK directory is missing: $app_ipk_dir" >&2
	exit 1
}

mkdir -p "$output_dir"

copy_one() {
	search_root=$1
	pattern=$2
	description=$3
	found=$(find "$search_root" -type f -name "$pattern" -print -quit)

	[ -n "$found" ] || {
		echo "$description package was not produced ($pattern)" >&2
		exit 1
	}

	cp "$found" "$output_dir/"
}

copy_one "$sdk_dir/bin" "libstdcpp6_*_${arch}.ipk" libstdcpp6
copy_one "$sdk_dir/bin" "zlib_*_${arch}.ipk" zlib
copy_one "$sdk_dir/bin" "libevent2-7_*_${arch}.ipk" libevent2-7
copy_one "$sdk_dir/bin" "libjson-c5_*_${arch}.ipk" libjson-c5
copy_one "$sdk_dir/bin" "libsqlite3-0_*_${arch}.ipk" libsqlite3-0
copy_one "$app_ipk_dir" "les-chatd_*_${arch}.ipk" les-chatd

echo "$output_dir"
