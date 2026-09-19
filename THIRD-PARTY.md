# Dependencies and licensing

This experimental project incorporates BlueKitchen BTstack. Its license
permits personal/noncommercial use subject to its conditions; see
[btstack-master/LICENSE](btstack-master/LICENSE). Do not assume this project
is available for commercial use or that every dependency uses the same license.
Individual source headers and bundled dependency notices remain authoritative.

The vendored BTstack subset comes from the master source archive obtained
during development, SHA-256:
`7956b1d9dc7401ed612cd606b361b9bb2aa4fa4867ae4ad327a0672004d9e13e`.
Source: https://github.com/bluekitchen/btstack
Unrelated platform ports, upstream tests and documentation are excluded.
QNX changes are in port/qnx-pi5, example/a2dp_source_demo.c, and the POSIX
portability edits described in README.md. The precise upstream commit of the
original archive was not recorded; the included source is the tested snapshot.

The libfdt parser in port/qnx-pi5/fdt-include comes from
https://github.com/dgibson/dtc and carries its original dual GPL-2.0-or-later
or BSD-2-Clause notices in each file.

Broadcom radio firmware is not committed. `sh fetch-firmware.sh` downloads
the tested blob from Raspberry Pi's bluez-firmware repository and verifies
its SHA-256. Review the upstream firmware terms; BCM-LEGAL.txt and
bluez-firmware-copyright are retained as supplied reference notices.

QNX headers, libraries and APKs are not redistributed. Build on the QNX
target with its licensed native development tools.
