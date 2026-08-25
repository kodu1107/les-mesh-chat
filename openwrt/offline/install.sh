#!/bin/sh

set -eu

supported_release=24.10.2
supported_revision=r28739-d9340319c6
trusted_key_id=9db1776b78018b98

[ "$#" -eq 0 ] || {
	echo "usage: $0" >&2
	exit 2
}

bundle_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
package_dir=$bundle_dir/packages

[ -r /etc/openwrt_release ] || {
	echo "this installer must run on OpenWrt" >&2
	exit 1
}

# shellcheck disable=SC1091
. /etc/openwrt_release

release_is_supported=0
if [ "${DISTRIB_RELEASE:-}" = "$supported_release" ]; then
	release_is_supported=1
elif [ "${DISTRIB_RELEASE:-}" = 24.10 ] && \
	[ "${DISTRIB_REVISION:-}" = "$supported_revision" ]; then
	release_is_supported=1
fi

[ "$release_is_supported" -eq 1 ] || {
	echo "unsupported OpenWrt release: ${DISTRIB_RELEASE:-unknown}" >&2
	echo "revision: ${DISTRIB_REVISION:-unknown}" >&2
	echo "expected: $supported_release ($supported_revision)" >&2
	exit 1
}

package_arch=$(
	opkg print-architecture | awk '
		$2 == "aarch64_cortex-a72" || $2 == "aarch64_cortex-a76" {
			selected = $2
		}
		END { print selected }
	'
)

[ -n "$package_arch" ] || {
	echo "this bundle supports Raspberry Pi 4 and Raspberry Pi 5 only" >&2
	exit 1
}

case "$(basename "$bundle_dir")" in
	*"-$package_arch") ;;
	*)
		echo "bundle architecture does not match this device: $package_arch" >&2
		exit 1
		;;
esac

[ -x /usr/bin/usign ] || [ -x /bin/usign ] || command -v usign >/dev/null 2>&1 || {
	echo "usign is required to verify this bundle" >&2
	exit 1
}

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

set -- \
	"$package_dir"/libgcc1_*_"$package_arch".ipk \
	"$package_dir"/libstdcpp6_*_"$package_arch".ipk \
	"$package_dir"/zlib_*_"$package_arch".ipk \
	"$package_dir"/libevent2-7_*_"$package_arch".ipk \
	"$package_dir"/libjson-c5_*_"$package_arch".ipk \
	"$package_dir"/libsqlite3-0_*_"$package_arch".ipk \
	"$package_dir"/les-chatd_*_"$package_arch".ipk

for package in "$@"; do
	[ -f "$package" ] || {
		echo "bundle package is missing: $package" >&2
		exit 1
	}
done

# Supplying every local package in one transaction lets opkg resolve the
# dependency graph without contacting an external package feed.
opkg install "$@"

if [ -x /etc/uci-defaults/99-les-chat ]; then
	/etc/uci-defaults/99-les-chat
fi

/etc/init.d/les-chatd enable
/etc/init.d/les-chatd restart

sleep 1
wget -qO- http://127.0.0.1:7777/healthz
echo
echo "LES Mesh Chat is available on port 7777."
