#!/bin/sh
# Only for the inspected BCM2712 D0 board, initially unconfigured BT pins.
# Run from this directory as root. No boot files, clocks, or Wi-Fi pins changed.
set -eu
test "$(id -u)" = 0 || { echo 'Run as root' >&2; exit 1; }
test "$(uname -m)" = RaspberryPi5 || exit 1
pins=$(gpio-bcm get 24-29)
printf '%s\n' "$pins"
for pin in 24 25 26 27 29; do
    printf '%s\n' "$pins" | grep "^GPIO${pin}/" | grep 'pull=pd.*func=INPUT' >/dev/null || {
        echo "GPIO$pin differs from inspected baseline; refusing" >&2; exit 1;
    }
done
gpio-bcm funcs 24 | grep 'SD_CARD_PRES_B.*UART_RTS_0' >/dev/null || {
    echo 'Unsupported pinmux layout; refusing' >&2; exit 1;
}
restore() {
    gpio-bcm set 29 op dl pd
    gpio-bcm set 24-27 ip pd
    gpio-bcm set 29 ip pd
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP
gpio-bcm set 29 op dl pd
gpio-bcm set 24-27 a4
gpio-bcm set 24,26 pn
gpio-bcm set 25,27 pu
gpio-bcm set 29 op dh
sleep 1
gpio-bcm get 24-29
./uart_probe --hci-read-info
