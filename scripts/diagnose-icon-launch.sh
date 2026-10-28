#!/bin/sh
# Read-only data collection for SailQEMU app-grid launch failures.
set -u
printf '%s\n' '===== SAILQEMU RPM ====='
rpm -q harbour-sailqemu 2>&1 || true
printf '\n%s\n' '===== INSTALLED DESKTOP ====='
if [ -f /usr/share/applications/harbour-sailqemu.desktop ]; then
    cat /usr/share/applications/harbour-sailqemu.desktop
else
    echo 'MISSING desktop file'
fi
printf '\n%s\n' '===== EXECUTABLE ====='
ls -l /usr/bin/harbour-sailqemu 2>&1 || true
printf '\n%s\n' '===== REQUIRED DYNAMIC LIBRARIES ====='
if command -v ldd >/dev/null 2>&1; then
    ldd /usr/bin/harbour-sailqemu 2>&1 || true
fi
printf '\n%s\n' '===== FRONTEND STARTUP TRACE ====='
for f in "$HOME"/.cache/org.edp17/SailQEMU/frontend-startup.log \
         "$HOME"/.cache/SailQEMU/frontend-startup.log \
         "$HOME"/.cache/harbour-sailqemu/frontend-startup.log; do
    if [ -f "$f" ]; then
        printf '%s\n' "File: $f"
        tail -n 60 "$f"
    fi
done
printf '\n%s\n' '===== MATCHING CACHE FILES ====='
find "$HOME/.cache" -maxdepth 5 -name frontend-startup.log -print 2>/dev/null | head -n 20 || true
printf '\n%s\n' '===== USER JOURNAL (RECENT MATCHES) ====='
if command -v journalctl >/dev/null 2>&1; then
    journalctl --user -b -n 1000 --no-pager 2>&1 |
        grep -iE 'sailqemu|sailjail|invoker|mapplauncher|booster|lipstick|QML.*error' |
        tail -n 120 || true
    printf '\n%s\n' '===== SYSTEM JOURNAL (RECENT MATCHES) ====='
    journalctl -b -n 1200 --no-pager 2>&1 |
        grep -iE 'sailqemu|sailjail|invoker|mapplauncher|booster|lipstick' |
        tail -n 120 || true
fi
printf '\nDone.\n'
