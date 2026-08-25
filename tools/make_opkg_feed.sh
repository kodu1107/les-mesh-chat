#!/bin/sh

set -eu

usage() {
	echo "usage: $0 SDK_DIR INPUT_DIR OUTPUT_DIR SIGNING_KEY [PUBLIC_KEY]" >&2
	exit 2
}

if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then
	usage
fi

sdk_dir=$1
input_dir=$2
output_dir=$3
signing_key=$4
public_key=${5:-}
indexer=$sdk_dir/scripts/ipkg-make-index.sh
usign=$sdk_dir/staging_dir/host/bin/usign
mkhash=$sdk_dir/staging_dir/host/bin/mkhash

[ -x "$indexer" ] || {
	echo "package indexer is missing: $indexer" >&2
	exit 1
}

[ -x "$usign" ] || {
	echo "usign is missing: $usign" >&2
	exit 1
}

[ -x "$mkhash" ] || {
	echo "mkhash is missing: $mkhash" >&2
	exit 1
}

[ -f "$signing_key" ] || {
	echo "signing key is missing: $signing_key" >&2
	exit 1
}

mkdir -p "$output_dir"

found_ipk=0
for ipk in "$input_dir"/*.ipk; do
	[ -f "$ipk" ] || continue
	cp "$ipk" "$output_dir/"
	found_ipk=1
done

[ "$found_ipk" -eq 1 ] || {
	echo "no IPK files found in $input_dir" >&2
	exit 1
}

(
	cd "$output_dir"
	MKHASH="$mkhash" "$indexer" . > Packages.manifest
	grep -vE \
		'^(Maintainer|LicenseFiles|Source|SourceName|Require|SourceDateEpoch):' \
		Packages.manifest > Packages
	case "$(((64 + $(stat -L -c%s Packages)) % 128))" in
		110|111)
			# OpenWrt uses this padding to avoid a historical usign SHA-512
			# boundary bug for package indexes of these exact lengths.
			printf '\n\n' >> Packages
			;;
	esac
	gzip -9nc Packages > Packages.gz
	"$usign" -S -m Packages -s "$signing_key" -x Packages.sig
)

if [ -n "$public_key" ]; then
	[ -f "$public_key" ] || {
		echo "public key is missing: $public_key" >&2
		exit 1
	}
	"$usign" -V -m "$output_dir/Packages" -p "$public_key" \
		-x "$output_dir/Packages.sig"
fi

echo "$output_dir"
