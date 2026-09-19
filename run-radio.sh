#!/bin/sh
# Temporary session for the inspected BCM2712 D0 board only.
set -eu
cd "$(dirname "$0")"
test "$(id -u)" = 0 || { echo 'Run as root' >&2; exit 1; }
test "$(uname -m)" = RaspberryPi5 || exit 1
test -f BCM4345C0.hcd
test -x btstack-master/port/qnx-pi5/a2dp_source_demo
mkdir .radio-lock || { echo 'Another radio session may be active' >&2; exit 1; }
child=
pins_changed=0
restore() {
    if test -n "$child"; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    if test "$pins_changed" = 1; then
        gpio-bcm set 29 op dl pd
        gpio-bcm set 24-27 ip pd
        gpio-bcm set 29 ip pd
    fi
    rmdir .radio-lock
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP
pins=$(gpio-bcm get 24-29)
for pin in 24 25 26 27 29; do
    printf '%s\n' "$pins" | grep "^GPIO${pin}/" | grep 'pull=pd.*func=INPUT' >/dev/null || {
        echo "GPIO$pin differs from inspected baseline; refusing" >&2; exit 1;
    }
done
gpio-bcm funcs 24 | grep 'SD_CARD_PRES_B.*UART_RTS_0' >/dev/null || exit 1
pins_changed=1
gpio-bcm set 29 op dl pd
gpio-bcm set 24-27 a4
gpio-bcm set 24,26 pn
gpio-bcm set 25,27 pu
sleep 1
gpio-bcm set 29 op dh
sleep 1
# Explicit stdin redirection keeps the interactive menu connected in background.
./btstack-master/port/qnx-pi5/a2dp_source_demo --onboard "$PWD/BCM4345C0.hcd" <&0 &
child=$!
status=0
wait "$child" || status=$?
child=
exit "$status"
