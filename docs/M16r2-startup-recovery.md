# M16r2 — startup/recovery report (SailQEMU 0.14.1-1)

This is an incremental reliability release after the on-device M16 confirmation.

* When QEMU exits, distinguish guest shutdown (QMP SHUTDOWN), deliberate Force off,
  exit before QMP ever connected and exit without a shutdown event. No automatic
  termination or reboot was added.
* If the QEMU process is alive but the QMP control channel is still unavailable
  after 15 seconds, show a one-time status warning; retries continue.
* The Diagnostics page includes the last ~2.5 KiB of the QEMU process log,
  read from its tail without loading a large log into memory. `Copy report`
  copies the existing read-only diagnostic report to the Sailfish clipboard.
* Saving a VM profile no longer silently clears a previous startup/QMP error.
* A read-only installed-file preflight is packaged at
  `/usr/share/harbour-sailqemu/bin/m16-release-check.sh`. It accepts an
  optional explicit QEMU binary path.
* VM, disk, image format, guest input and D-Bus display paths are unchanged.

## Suggested tests

1. Start FreeDOS, Windows 95 or ReactOS; open Diagnostics and use Copy report.
2. Shut down a guest normally, check the status, and confirm the Main page returns.
3. In a disposable test VM, set an invalid/missing QEMU executable and confirm
   startup is rejected with an actionable error (do not change production VM).
4. If you can create an independently tested QEMU process that exits immediately,
   inspect the new last-log excerpt. This is optional; do not corrupt VM images.

Qt 5.6-compatible C++/QML is used; the source tests cannot replace an SDK build.
