# SailQEMU 0.5.0-3 Sailjail experiment

**Not release approved.** Sailfish OS 5.1 SDK and on-device validation required.

## What changed

* Start QEMU directly using POSIX spawn, not `/bin/sh`.
* Preserve child PID and capture stdout/stderr to the existing `qemu.log`.
* Request access to `org.qemu` display D-Bus service, and make the display
  listener visible under Sailjail via application-specific permission.
* Keep the existing QEMU executable, disk, network and display configuration.

## Validation

1. Install a freshly built RPM from this tree. Confirm the new permission
   exists at `/etc/sailjail/permissions/SailQemuDbus.permission`.
2. Launch from the application icon and start FreeDOS or another disposable
   guest. Verify the QMP connection, D-Bus preview and keyboard/mouse.
3. Inspect `$HOME/.cache/org.edp17/SailQEMU/qemu.log` if launch fails.
4. If the permission is ignored, log out and back in before repeating.
5. Read `/etc/sailjail/permissions/SailQemuDbus.permission`, inspect the
   `firejail-*.log` traces and compare with an unsandboxed control launch.

## Caveats

* `org.qemu` is a global session D-Bus name. Only one QEMU D-Bus guest at a
  time is currently supported.
* Image files outside Sailjail's whitelisted paths still cannot be used.
* Custom application permissions are not necessarily allowed by Jolla's
  Harbour submission policy. If official Harbour distribution is required,
  this implementation must be redesigned (e.g. peer-to-peer D-Bus).
* `posix_spawn` retains the existing child PID and image arguments, but does
  not create a new process session as Qt's original detached launcher did.

## M16 Sailjail control-path follow-up (0.5.0-3)

* Allow visibility of `org.qemu` for GetConnectionUnixProcessID.
* Permit read-only D-Bus Properties calls to `/org/qemu/Display1` and descendants;
  mouse and console discovery use `org.freedesktop.DBus.Properties.Get`.
* Leave R/scroll icons visible (disabled until mouse discovery succeeds) and
  show QMP and D-Bus status when guest mouse input is unavailable.
* Explicit **Force off** now uses SIGTERM on a PID verified via `/proc` if QMP
  is disconnected. This is an unsafe termination, never an ACPI fallback.
* ACPI `system_powerdown` remains dependent on QMP **and guest ACPI support**.

**On-device test:** Launch from the icon, boot a guest with USB tablet,
check mouse status, then left/right/double-click, pause/resume and stop.
Run a second guest with ACPI disabled and use Force off, not ACPI shutdown.
In the Guest console Info panel, record D-Bus Status and mouse Status if
controls are disabled, and share Diagnostics report to distinguish QMP
from D-Bus problems. Reboot after updating Sailjail permission definitions
if the new rules are not reflected in the sandbox.


## 0.5.0-4: PID-namespace-safe D-Bus binding

Under Sailjail, QMP and process operations use the sandbox PID (for example 51),
while `org.freedesktop.DBus.GetConnectionUnixProcessID` reports the host PID
(for example 36810). Equality of those numbers is **not** a valid VM identity
check. 0.5.0-3 incorrectly refused the mouse service because of this.

At each new launch, SailQEMU generates a random operator-side QEMU name with
`-name sailqemu-<UUID>`, without changing the guest's hardware/SMBIOS UUID.
QMP `query-name` must return the expected name; only then may the frontend
compare `org.qemu.Display1.VM.Name` against the same name. If either check
fails, guest D-Bus controls remain unavailable. This still avoids attaching
to a different QEMU service. The name is stored alongside the running PID
for frontend restarts and cleared when the VM stops. Previously started VMs
without an identity must be shut down and restarted (QMP ACPI/Force off remains
available); there is no unsafe PID-namespace bypass.

Test from the Sailfish icon after installation, using a **newly started** VM:
check `QMP connected`, `Verified QMP/D-Bus identity`, `Mouse ready`, left,
right and double click, pause/resume, ACPI and Force off. The existing source
checks do **not** replace the on-device Sailjail test.

The standalone framebuffer listener used the same broken service-PID comparison
as the frontend and must also verify `org.qemu.Display1.VM.Name` against the
expected per-launch name (passed as its first argument). This keeps live frame
registration protected without depending on differing host/sandbox PIDs.
