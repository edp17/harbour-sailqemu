#!/bin/bash
# Read-only inventory for preparing a reproducible QEMU RPM later.
set -u
qemu="${QEMU_BIN:-$HOME/Downloads/edp17Apps/sailQemu/qemu-4.1.0-sdl-poc-build/x86_64-softmmu/qemu-system-x86_64}"
echo '===== QEMU VERSION ====='
"$qemu" --version 2>&1 | head -n 3

echo '===== INSTALLED EXECUTABLES ====='
find /usr/local/bin -maxdepth 1 \( -name 'qemu-*' -o -name 'virtiofsd' \) -type f -print 2>/dev/null | sort

echo '===== FIRMWARE/DATA FILES ====='
for dir in /usr/local/share/qemu /usr/share/qemu; do
    if [[ -d "$dir" ]]; then
        find "$dir" -maxdepth 2 -type f -print 2>/dev/null | sort
    fi
done

echo '===== SHARED LIBRARIES ====='
ldd "$qemu" 2>&1 || true

echo '===== BUILD AND TOOL INFORMATION ====='
for cmd in gcc meson ninja pkg-config make; do
    command -v "$cmd" || true
done
printf 'CPU: '; uname -m
printf 'Sailfish: '; grep '^VERSION=' /etc/sailfish-release 2>/dev/null || true

cat <<'END_NOTE'

NOTE: This only inventories files and runtime dependencies. Building a
redistributable RPM additionally requires the original source tree, its
configure options, firmware/ROM data, build dependencies and licensing.
END_NOTE
