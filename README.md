# QNX 8 / Raspberry Pi 5 Bluetooth audio bring-up

Status: **experimental Bluetooth transport, Python PCM input and A2DP speaker playback work;
system-wide QNX audio routing is not implemented.**

For a fresh checkout, run `sh fetch-firmware.sh` before starting the radio.
See [THIRD-PARTY.md](THIRD-PARTY.md) for dependency origins and license terms.

Python application playback is now available through `audio.pcm`. See
[PCM-PLAYBACK.md](PCM-PLAYBACK.md) for the two-terminal workflow and Python API.
The first Python tone test delivered all 132,300 frames to the PCM consumer.

On 2026-09-19 the target at `10.37.96.25` paired with the user's UT888,
advertised as `TWS Mini Speaker` (`75:89:73:F9:AA:6A`). The user confirmed
that the final quiet three-second tone was audible and steady. The final test
used SBC stereo at 44.1 kHz. Its trace contained 236 outgoing ACL packets and
236 controller completions, with no disconnect before intentional shutdown.
These are short bring-up tests, not evidence of long-duration reliability.

## Locations and changes

Target project: `/data/home/qnxuser/projects/qnx-bluetooth` (also `~/projects/qnx-bluetooth`).
Host copy: `/home/samuelm/Projects/qnx-bluetooth`.

- `btstack-master/port/qnx-pi5/btstack_uart_qnx_bcm.c`: new user-space
  BCM7271 UART backend for BTstack. Uses only the onboard Bluetooth UART at
  `0x107d50c000`, 32-bit accesses, 32-byte FIFO, and hardware RTS/CTS.
- `btstack-master/port/qnx-pi5/board_address.c`: reads the live device tree
  through a read-only mapping. Requires Raspberry Pi 5 Model B compatibility
  and reads the factory Bluetooth address, `2C:CF:67:D5:CD:08` on this board.
- `btstack-master/port/qnx-pi5/main.c`, `Makefile`, `btstack_config.h`:
  QNX application integration, firmware loading, pairing storage and trace.
- `btstack-master/example/a2dp_source_demo.c`: speaker-name filter,
  selected-peer pairing confirmation, silent connection, and a three-second
  tone with automatic local muting. The PCM amplitude is divided by 32.
- Two POSIX portability fixes: `<errno.h>` and QNX's `IHFLOW | OHFLOW` flags.
  The latter is for the generic serial backend; onboard mode uses the new
  MMIO backend instead.
- `run-radio.sh`: temporary foreground session with exclusive project lock,
  verified initial pin state, board-revision guard and GPIO rollback.
- `uart_probe.c`, `probe-radio.sh`: bounded low-level diagnostics.
- `BCM4345C0.hcd`: upstream Raspberry Pi firmware patch, loaded into the radio's
  volatile RAM. No EEPROM or flash programming.
- Target `packages/`: downloaded source archive and extracted QNX ALSA
  application headers. Nothing was installed using `apk add`.
- Target `successful-quiet-test.pklg`: preserved successful protocol trace.
  It and `pairings.tlv` are private, root-owned files; traces can contain
  pairing material, so do not publish them.

No boot image, boot configuration, startup service, Wi-Fi configuration,
system audio configuration or system library was changed. No reboot was done.
Bluetooth GPIO and UART changes were temporary and restored on exit. The final
checks found GPIO 24–27 and 29 back at input/pull-down and UART LCR/MCR at zero.
GPIO 28 (Wi-Fi enable) was not modified.

## Build and run

On the Pi:

```sh
cd ~/projects/qnx-bluetooth/btstack-master/port/qnx-pi5
make -j2
./a2dp_source_demo --help
sudo ./a2dp_source_demo --check-board   # read-only
cd ../../..
sudo sh ./run-radio.sh
```

This launcher is specific to the inspected BCM2712 D0 board and its original
idle pin state. It deliberately refuses other layouts or an already-owned UART.
Use the launcher rather than directly invoking `--onboard`.

Interactive keys:

- `a`: scan for `TWS Mini Speaker` and connect. Pairing mode may be required.
- `x`: play one quiet three-second 441 Hz tone; then automatically mute/pause.
- `p`: immediately mute the source and request pause.
- `f`: enable the PCM FIFO for a connected 44.1 kHz stream (see PCM-PLAYBACK.md).
- Ctrl-C: stop and restore the UART and GPIO baseline.

The speaker may play its own connection/disconnection chime independently of
the test's PCM volume. The launcher does not alter the speaker's stored volume.
Upstream demonstration keys are still present; only the keys above are part of
this bring-up procedure. The program is not a system audio output device.

## Verified hardware sequence

The onboard Bluetooth UART is BCM7271/16550-style, not the PL011 debug UART
at `/dev/ser10`. On this D0 board, GPIO 24–27 use ALT4; GPIO 29 is BT_REG_ON.
Initial baud is approximately 115200 (96 MHz / 16 / 52); operational baud is
3 Mbps (divisor 2). Wait for the radio to assert CTS before transmitting.
Sending the initial HCI reset before CTS was ready caused the early timeouts.

Using the controller's generic chip address was also corrected: the firmware's
live device tree supplies the board's unique address. The source reverses its
little-endian Bluetooth address representation for BTstack.

## Remaining work for all QNX applications

The target runs `io-snd` and QNX SALSA (`libasound.so.5`), currently with a USB
audio output. The available `qnx-alsa-dev-8.0.5-r0` APK supplies **application**
headers, not the `io-snd` driver interface. The installed library does not export
ALSA's `snd_pcm_ioplug_create`, so a Linux BlueALSA plugin cannot just be loaded.
QNX 6.x/7.x `io-audio` DDK examples are for a different audio interface.

To provide a normal system-wide output, obtain the QNX 8 `io-snd` audio driver
development headers, documentation and a compatible source driver example
(for example the Raspberry Pi audio samples distributed through QNX Software
Center). Confirm compatibility with this image's 8.0.5 audio runtime.
Those files can be staged inside this project; no system-wide SDK installation
is necessary for inspection.

Next implementation stages are a virtual PCM output for `io-snd`, bounded PCM
transport into the SBC/A2DP sender, underrun/disconnect handling, mixing/rate
conversion through the QNX audio framework, and application-level validation.
Only after those work should the preferred output or startup configuration be
changed. A sine-test success alone does not satisfy system-wide routing.

Other engineering work: sustained playback/load tests, reconnect tests, CPU
profiling, bounded/rotated tracing and a production scheduling strategy. The
current UART backend polls with a bounded one-millisecond work budget; it is a
prototype, not a production driver. One earlier RF connection suffered a link
supervision timeout; the final quiet test and pause completed normally.

## Sources and reproducibility

- QNX Pi 5 BSP: https://github.com/qnx/bsp_raspberrypi-bcm2712-rpi5
- Raspberry Pi hardware bindings and drivers:
  https://github.com/raspberrypi/linux/tree/rpi-6.12.y
- BTstack: https://github.com/bluekitchen/btstack
- Controller firmware: https://github.com/RPi-Distro/bluez-firmware
- Read-only FDT parser: https://github.com/dgibson/dtc/tree/main/libfdt
- QNX confirms Raspberry Pi audio source samples are distributed through QSC:
  https://qnx.software/en/blog/2026/building-a-qnx-powered-cd-player
- Historical official Bluetooth SDK targets QNX 6.6 and is EOL:
  https://www.qnx.com/download/group.html?programid=27609

BTstack archive SHA-256:
`7956b1d9dc7401ed612cd606b361b9bb2aa4fa4867ae4ad327a0672004d9e13e`

BCM4345C0.hcd SHA-256:
`51c45e77ddad91a19e96dc8fb75295b2087c279940df2634b23baf71b6dea42c`

Retain upstream notices. BTstack's free license restricts use to personal,
non-commercial purposes, matching the user's confirmed use. The FDT files
offer a BSD-2-Clause option. The downloaded Linux reference sources retain
their original licenses and are references rather than linked driver code.
