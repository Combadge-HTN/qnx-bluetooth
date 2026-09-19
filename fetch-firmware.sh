#!/bin/sh
set -eu
cd "$(dirname "$0")"
expected=51c45e77ddad91a19e96dc8fb75295b2087c279940df2634b23baf71b6dea42c
tmp=$(mktemp ./firmware-download.XXXXXX)
trap 'rm -f "$tmp"' EXIT HUP INT TERM
curl --fail --location --output "$tmp" \
    https://raw.githubusercontent.com/RPi-Distro/bluez-firmware/master/broadcom/BCM4345C0.hcd
printf '%s  %s\n' "$expected" "$tmp" | sha256sum -c -
mv "$tmp" BCM4345C0.hcd
