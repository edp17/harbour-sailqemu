#!/bin/sh
# A detached, non-evaluating runner: preserves a real QEMU PID and logs stderr.
# Values are positional arguments; nothing is evaluated or passed to sh -c.
set -eu
LOG=$1
shift
export SDL_VIDEODRIVER=wayland
exec "$@" > "$LOG" 2>&1
