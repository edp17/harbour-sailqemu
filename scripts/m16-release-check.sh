#!/bin/sh
# Read-only runtime/packaging checklist for Sailfish OS 5.1.
# Usage: m16-release-check.sh [path-to-qemu-system-x86_64]
# Does not start a guest, change an image, or modify profiles.
set -u
QEMU="${1:-/opt/sailqemu/qemu-10.2.4-net/bin/qemu-system-x86_64}"
if [ "$#" -eq 0 ] && [ ! -x "$QEMU" ] && \
   [ -x /opt/sailqemu/qemu-10.2.4/bin/qemu-system-x86_64 ]; then
    QEMU=/opt/sailqemu/qemu-10.2.4/bin/qemu-system-x86_64
fi
failed=0
check_file() {
    label=$1
    path=$2
    if [ -f "$path" ] && [ -x "$path" ]; then
        printf 'OK      %-19s %s\n' "$label" "$path"
    else
        printf 'MISSING %-19s %s\n' "$label" "$path"
        failed=1
    fi
}
check_readable() {
    label=$1
    path=$2
    if [ -f "$path" ] && [ -r "$path" ]; then
        printf 'OK      %-19s %s\n' "$label" "$path"
    else
        printf 'MISSING %-19s %s\n' "$label" "$path"
        failed=1
    fi
}
printf '%s\n' 'SailQEMU read-only release preflight'
check_file 'Frontend' /usr/bin/harbour-sailqemu
check_file 'D-Bus listener' /usr/bin/harbour-sailqemu-display-listener
check_file 'Launch script' /usr/share/harbour-sailqemu/bin/sailqemu-runner.sh
check_file 'QEMU executable' "$QEMU"
QEMUDIR=${QEMU%/*}
check_file 'qemu-img' "$QEMUDIR/qemu-img"
check_readable 'Desktop entry' /usr/share/applications/harbour-sailqemu.desktop
check_readable 'Application icon' /usr/share/icons/hicolor/86x86/apps/harbour-sailqemu.png
QMLROOT=/usr/share/harbour-sailqemu/qml
check_readable 'QML entrypoint' "$QMLROOT/main.qml"
for page in MainPage ProfilesPage VmEditPage GuestKeyboardPage DiskToolsPage \
            DiagnosticsPage SailQemuGuidePage AboutPage; do
    check_readable "QML $page" "$QMLROOT/pages/$page.qml"
done
if command -v rpm >/dev/null 2>&1; then
    rpm -q harbour-sailqemu || failed=1
fi
if [ "$failed" -eq 0 ]; then
    printf '%s\n' 'PASS: expected runtime files are present. This is not a VM functionality test.'
else
    printf '%s\n' 'FAIL: one or more expected files are missing; see the lines above.'
fi
exit "$failed"
