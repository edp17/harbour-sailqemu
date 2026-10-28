#!/bin/sh
# Read-only probe: check that the chosen QEMU executable advertises the
# CPU/video models offered in M13. Does not launch a virtual machine.
set -eu
QEMU=${1:-/opt/sailqemu/qemu-10.2.4-net/bin/qemu-system-x86_64}
if [ ! -x "$QEMU" ]; then
    echo "QEMU executable not found or not executable: $QEMU" >&2
    exit 1
fi
printf '===== QEMU version =====\n'
"$QEMU" --version | head -n 1
printf '\n===== Selected CPU models (-cpu help) =====\n'
"$QEMU" -cpu help 2>&1 | grep -Ei '(^|[[:space:]])(qemu32|qemu64|486|pentium|pentium2|pentium3)([[:space:]]|$)' || true
printf '\n===== Video-adapter options (-vga help) =====\n'
"$QEMU" -vga help 2>&1 | grep -Ei '(std|cirrus|vmware)' || true
printf '\nM13 probe finished. This does not confirm that a guest will boot with every model.\n'
