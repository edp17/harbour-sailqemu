# M16 — Reliability, Diagnostics and Packaging (0.14.0-1)

Built on the confirmed 0.13.0-2 (M15r2) frontend and the independently
installed QEMU 10.2.4 binaries. No VM storage schema change; existing profiles
and guest images stay where they are. The QMP, D-Bus, display and input paths
retain their known-good behaviour.

## Disk tools result visibility

- The operation status and report now appear near the **top** of Disk tools.
- On completion, cancellation or a rejected input path, Disk tools scrolls to
  the top so the result is visible without manual searching.
- Cancel is asynchronous; the UI is not blocked waiting for qemu-img to exit.
- A failed or rejected operation clears any previously successful "Use as
  selected VM's primary HDD" result. An existing output image is not replaced.

## Runtime diagnostics

Access via **Main > Diagnostics** or **Guest console > Info > Performance and
runtime diagnostics**. This is read-only and refreshes about every 2.5 seconds.

Measurements:

- QEMU CPU percentage of **one** CPU core (can exceed 100% when QEMU threads use
  more than one core); SailQEMU frontend CPU, separately.
- Resident set size (RSS) in MiB for the two processes.
- Observed D-Bus framebuffer file changes per second, plus staleness detection.
  **Not** QEMU guest framerate or the frame presentation rate of Qt Quick.
- Guest console dimensions, if reported by QEMU; QMP connection state; SDL vs
  D-Bus display state.
- Read-only existence/executability checks for selected QEMU, adjacent
  qemu-img, installed display listener, and launch script.
- Location of QEMU's log file and latest frontend error.

Sampling uses `/proc/PID/stat` fields 14 and 15, and `/proc/PID/statm`. No
commands are launched, no guest input is sent, and no disk image is read by
this diagnostics page. The first CPU figure reads "sampling..." until two
samples of the same process are available.

## Suggested on-device checks

1. Open Disk tools. Inspect a disposable image; verify that the report is
   visible near the top and the screen automatically returns to it.
2. Re-run Create with an existing destination filename. Confirm refusal and
   that no previous image remains available under "Use as...".
3. Copy a disposable image then cancel while busy; verify cancellation does
   not lock the UI and no destination is published.
4. Open Diagnostics with no VM running. Check that QEMU path/qemu-img checks
   reflect your selected profile.
5. Start ReactOS or Windows 95, open Diagnostics and observe CPU/RSS and
   framebuffer update rate. Compare with Windows XP under similar guest load.
6. Shut down through ACPI as before. Confirm auto-return to Main and that
   diagnostics shows QEMU stopped.

## Boundaries

The host may run a separately launched QEMU instance that SailQEMU does not
track, so disk work still requires stopping all external writers. The QEMU
process and Qt renderer cannot be benchmarked solely through observed frame
updates. QEMU and libslirp remain separate runtime dependencies and must be
installed independently.
