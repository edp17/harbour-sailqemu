# SailQEMU — native QEMU frontend for Sailfish OS

**Sailjail test candidate: 0.5.0-4 (Sailfish OS 5.1, Qt 5.6)**

SailQEMU is a native Sailfish Silica VM manager for QEMU 10.2.4, with a
D-Bus guest console, touch/mouse/keyboard interaction, persistent VM profiles,
QMP control, and disk-image management. The QEMU emulator and libslirp are
**separate upstream-based packages** and are not included in this repository.

This 0.5.0 version follows the device-tested 0.14.3-1 development snapshot.
The version number has intentionally been reset for the public beta.
**This is numerically lower than 0.14.3:** existing test devices will need an
explicit RPM downgrade or package replacement, not a normal automatic update.
Older experimental 0.5.0 development RPMs existed; use source tags, not just
RPM version numbers, to distinguish their code.

See [0.5.0 beta notes](docs/BETA-0.5.0.md) for prerequisites and rollout
instructions, and [source-build plan](docs/OBS-source-build-plan.md) for the
future QEMU/libslirp OBS packaging work.

## Building

Use Sailfish SDK 5.1 with an `aarch64` or `armv7hl` target, CMake, and the
requirements listed in `rpm/harbour-sailqemu.spec`. The source can be built
locally using the existing SDK/RPM workflow; no bundled QEMU source is needed.
Run source checks with `python3 tests/check-beta.py` and the other
`tests/check-*.py` scripts. Results are **not** a substitute for an SDK build.

## Change history (development series)

## Release 0.14.3-1 — M16r4 beta-readiness: Diagnostics menu

On the Diagnostics page, **Refresh now** remains a pulley action and **Copy report**
is now a second pulley action. The duplicate Refresh button and the old Copy
report button are removed. Copy refreshes the data before putting it on the
clipboard. Diagnostic sampling, QEMU launch, disk, display and mouse paths are
unchanged. See `docs/M16r4-diagnostics-pulley.md`.

The source also includes an **OBS source-build plan** for separately built
libslirp 4.9.5, QEMU 10.2.4 and the SailQEMU frontend on `armv7hl` and
`aarch64`. This is documentation only, not OBS-ready source RPM recipes:
see `docs/OBS-source-build-plan.md`. No binary dependencies are bundled or
installed by this source archive. Sailfish SDK 5.1 compilation and on-device
regression tests are still required before a beta tag.

## Release 0.14.2-1 — M16r3 navigation and release-readiness

Confirmed M16r2 is the tested baseline. The Main page now has only **three**
pulley actions (Manage VMs, User guide, About). **Manage VMs** now has
**three** pulley actions (Add new, Disk tools, Diagnostics), with no change to
existing direct links from the Guest console or Add/Edit VM. The User guide
reflects the new navigation and adds a first-run QEMU dependency check.
The installed read-only `m16-release-check.sh` now also verifies the app
icon and essential QML pages. QEMU executables and libslirp are **not** bundled
with the frontend. No VM profile, storage, input, display or lifecycle logic
was modified. See `docs/M16r3-navigation-release.md`.

## Release 0.14.1-1 — M16r2 startup recovery

Confirmed M16 runtime/display/storage paths are retained. This maintenance release
adds a one-time QMP startup warning, QMP-event-aware exit statuses, a bounded
recent QEMU process-log excerpt in Diagnostics, and Copy report to clipboard.
See `docs/M16r2-startup-recovery.md`. Requires Sailfish OS 5.1 Qt 5.6 SDK test.

# SailQEMU M15r2 (0.14.0-1) — Qt 5.6 build correction

Fixed the Qt 5.6 build error in `src/diskmanager.cpp`: `QTemporaryDir` has no `filePath()`; the staged image path now uses `QDir(m_stage->path()).filePath(...)`. No disk-image operations or VM settings were intentionally changed. This patch is against M15 `0.13.0-1`.

# SailQEMU M15 (0.13.0-1) — Safe VM disk tools

See [M15 disk tools](docs/M15-disk-tools.md) for new features and safeguards. The latest working M14r4 version was 0.12.0-4.

# SailQEMU M14r3 (0.12.0-3) — QCOW v1 support and media safety

Based on the G7 Power's confirmed working M13 (0.11.0-1), retaining
its D-Bus display, NAT networking, mouse/keyboard input, VM profiles,
automatic shutdown return, and previously tested virtual hardware.

## Corrections

- **Save VM** is horizontally centred on the Add/Edit VM page.
- **Manage VMs → long press a nonselected VM → Edit** now delegates the
  selection/navigation to the owning page after the context menu closes.
  It should enter Edit on the first tap; selecting another VM is still
  required because existing Add/Edit fields are bound to the active VM.

## Guest compatibility settings

- Add/Edit VM → Hardware: choose an optional **guest compatibility preset**.
  This changes CPU model, vCPU count, RAM, video adapter, ACPI and USB tablet,
  but **never** changes disk paths/formats, floppy/ISO/HDD boot order,
  networking, or QEMU executable. Changes take effect when you save and restart.
- **Windows XP (standard PC):** QEMU default CPU, 1 vCPU, 512 MiB, std VGA,
  ACPI enabled, USB tablet enabled. Recommended starting point for your XP image.
- **Windows 95 (legacy PC):** Pentium, 1 vCPU, 128 MiB, Cirrus VGA,
  ACPI off (`-machine pc,acpi=off`), no USB tablet (PS/2 relative input).
  It is a *test configuration*, not a guarantee that an arbitrary Win95
  image contains a bootable installed Windows 95 system.
- **FreeDOS:** QEMU default CPU, 1 vCPU, 64 MiB, std VGA, ACPI off and PS/2 mouse.
- ACPI and USB tablet can also be controlled independently under Hardware.
  Missing fields in older profiles both default to **enabled**, preserving
  previous command lines. Profile format version is 7; old profiles load
  without migration of their existing hardware settings.
- Disabling ACPI may make ACPI off ineffective: use the guest's own shutdown
  or confirmation-protected Force off when needed.
- User guide includes Windows 95 DOS prompt versus graphical boot diagnosis.

## Test on Sailfish OS 5.1 (aarch64)

1. Build using the same SDK command as M13. Expect
   `harbour-sailqemu-0.12.0-3.aarch64.rpm`.
2. Run source checks: `python3 tests/check-source.py`,
   `python3 tests/check-m12.py`, `python3 tests/check-m12r2.py`,
   `python3 tests/check-m13.py`, `python3 tests/check-m14.py`.
3. Stop any running guest. Select VM1 in Manage VMs, long-press VM3,
   choose **Edit** once. The editor must show VM3's name and media on
   the first attempt; verify that **Save VM** is centred.
4. Duplicate a test profile first (note: **Duplicate copies settings, not
   the writable HDD image**, so make a separate image backup for experiments).
   Apply Windows 95 preset, Save, reopen and confirm all fields persist.
   Check disk/ISO/floppy paths, boot order and networking are untouched.
5. With a Windows 95 test image, try the legacy preset, check boot media and
   enter `VER`, `DIR C:\\WINDOWS`, `C:\\WINDOWS\\WIN` in the appropriate
   DOS environment. Windows 95 compatibility may still require guest drivers,
   CPU/timing changes or proper installation media.
6. Apply Windows XP preset to a disposable profile and confirm it still boots
   with correct mouse positions, live display and NAT if enabled. Confirm
   FreeDOS still starts as before from its existing untouched profile.

**Validation boundary:** All supplied source tests are static and do not
compile the Sailfish Qt 5.6/Silica application or launch QEMU on your device.
The SDK build and real guest tests are still required.

---

# SailQEMU M13 (0.11.0-1) — VM hardware configuration and paused-state status

M13 builds on the **on-device-tested 0.10.1-1 (M12r2)** frontend and
preserves existing profiles, the working NAT-enabled QEMU 10.2.4 backend,
D-Bus display, mouse input, immediate guest boot, and automatic return after VM exit.

## Changes

- **Manage VMs** now distinguishes **Paused** from **Running** for the active
  VM, matching the Main page. Pausing does not stop the VM process.
- **CPU model** under Add/Edit VM → Hardware: Default (unchanged QEMU default),
  Intel 486, Pentium, Pentium II, Pentium III, QEMU 32-bit or QEMU 64-bit.
  Only explicit, non-default choices add `-cpu MODEL` to QEMU arguments.
  Choosing an older CPU model is an experiment, not a promise that an older OS boots.
- **Virtual CPUs**: 1, 2, or 4. The default 1 preserves old behaviour;
  other choices add `-smp cpus=2` or `-smp cpus=4`. Start with 1 for
  DOS and Windows 9x. Additional vCPUs do not guarantee higher performance
  with TCG emulation and increase guest compatibility risk.
- **Video adapter**: Standard VGA, Cirrus Logic VGA, or VMware SVGA.
  Existing profiles retain `-vga std`; select Cirrus only when testing older
  guests. Some adapters need guest-specific display drivers.
- CPU architecture remains **x86-compatible emulation on QEMU x86_64**;
  the CPU model selector does *not* enable ARM guest architectures.
- These new keys (`cpuModel`, `cpuCount`, `videoAdapter`) are optional when
  loading older profile files. Invalid values are reset to the old defaults.
  Profile schema version is now 6; previous profile keys are retained.

## Test procedure

1. Build with Sailfish SDK 5.1 aarch64, expected RPM
   `harbour-sailqemu-0.11.0-1.aarch64.rpm`.
2. Run `python3 tests/check-source.py`, `python3 tests/check-m12.py`,
   `python3 tests/check-m12r2.py`, and `python3 tests/check-m13.py`.
3. Boot existing FreeDOS/Windows XP profiles without editing them first.
   Confirm no change in behaviour and that NAT and D-Bus still work.
4. Pause a VM and open **Manage VMs**: selected VM should say **Paused**;
   resume and it should return to **Running**.
5. Stop the VM; create or duplicate a disposable test profile. Change the CPU
   model to Pentium III, virtual CPUs to 2, and display adapter to Cirrus.
   Save, reopen, and confirm the settings persist. Try booting the test guest.
6. Restore defaults in the test profile and repeat. Do not experiment first
   with irreplaceable writable Windows disk images; make a backup.

**Validation boundary:** The source/package regression checks do not compile
Sailfish Qt/Silica code or launch an actual guest; verify those steps on device.

---

## M12r2 historical release notes


This is a focused correction based on the G7 Power's M12 (0.10.0) test:
Windows XP appeared no faster and its boot felt slower after aggressive
frame-rate increases. 0.10.1 returns to the last confirmed frame schedule,
while preserving the M12 incremental scanout changes and diagnostic counter.

## VM lifecycle

- **Start VM** (integrated D-Bus) now starts executing the guest immediately.
  The legacy **SDL + guest controls** launch also starts immediately.
  Neither sends QEMU `-S`; users no longer need to press Play after Start.
- The Play/Pause action still uses QMP stop/cont when deliberately selected.
- The Guest console automatically pops to Main after the QEMU process exits,
  regardless of whether it is shut down by ACPI or Force off. It does not
  pop on the initial ACPI request, a QMP pause, or a temporary QMP disconnect.
  Process-exit detection is periodic, so allow around 1–2 seconds.
- The Main page retains its **Return to running VM** control.

## Display-performance regression mitigation

- Restore the proven listener interval of 120 ms, frontend sampling of 200 ms,
  and the former single-Image presentation, replacing the unproven M12
  90/75 ms dual asynchronous buffering.
- Keep M12's direct incremental framebuffer updates, unchanged-frame
  detection, tuned lossless PNG encoder, and runtime frame-update counter.
- Keep the private temporary frame directory to avoid frequent cache writes.
- This is **not** a claim of greater guest FPS; a later iteration should
  profile encoding, decode, UI presentation, and CPU emulation separately.

## Build and basic checks

Build using the Sailfish OS 5.1 aarch64 SDK; output:
`harbour-sailqemu-0.10.1-1.aarch64.rpm`.

Run `python3 tests/check-source.py` and
`python3 tests/check-m12.py` in the extracted source tree, then on the phone
`/usr/bin/harbour-sailqemu-display-listener --self-test`.

Device tests: launch Windows XP or FreeDOS and verify it boots without tapping
Play; use manual Pause/Resume; request ACPI shutdown and verify the app returns
to Main only after QEMU exits; repeat with Force off. Confirm VM profiles,
mouse alignment and network connectivity are unchanged.

Existing QEMU RPMs (both plain and NAT-enabled) are unaffected.

## Historical note: M12 0.10.0 (superseded)

Version 0.10.0 experimented with 90 ms frame encoding, 75 ms polling and
double-buffered asynchronous Qt Quick images. User testing did not demonstrate
better responsiveness and suggested slower Windows XP startup. These changes
are rolled back in 0.10.1. The direct incremental framebuffer-copy change,
lossless PNG encoder tuning, tmpfs files and Info update-rate counter remain.

To investigate performance further, measure QEMU TCG CPU usage, listener PNG
encoding time, Qt image decode/presentation timing and guest frame rate before
modifying the pipeline again.

---

## 0.9.4 M11r3 — Structured Add/Edit VM configuration

The Add/Edit VM page now has labelled Disks, Boot order, Hardware, and Miscellaneous areas, with small theme icons. Browse controls show the selected filename; full editable paths stay collapsed under **Edit media paths manually**. The emulator path is also collapsed by default.

* **Primary HDD** plus **up to two secondary IDE HDDs**. Secondary HDDs occupy IDE slots 1 and 3, while IDE slot 2 is reserved for CD/DVD. Each HDD has its own file format (raw, qcow2, vmdk, vdi). Adding an empty secondary slot requires browsing to an image before starting the VM; SailQEMU will refuse to launch an incomplete configuration.
* **CD/DVD** image format defaults to raw, corresponding to ordinary `.iso` files. Other supported QEMU block formats may be selected for converted optical images. An optical image is attached read-only to the VM. Floppy images remain read-only.
* **Primary and secondary boot** dropdowns control the actual BIOS `-boot` option. The secondary selector prevents selecting the primary device again; absent floppy/CD images cannot be selected. Existing profiles retain their original boot order until edited. The hard disk fallback remains available even when no HDD image is selected.
* **CPU**: M13 adds an optional CPU model selector plus 1, 2 or 4 virtual CPUs. The system emulator remains x86_64.
* **Memory** presets of 64, 128, 256, 512, 1024, 2048, 4096 MiB, plus Custom (16–32768 MiB). The custom TextField accepts only integers.
* **Network**: user-mode NAT when using the network-enabled QEMU RPM; old QEMU RPMs without libslirp still show a meaningful error.
* **Start SDL fullscreen** controls `-full-screen` for the *legacy SDL modes only*. The normal SailQEMU D-Bus Guest console uses the Sailfish page and ignores this option.

**Compatibility:** Per-VM `profiles.ini` remains in the same location. The profile schema advances from 4 to 5 without deleting old keys, and legacy boot switches remain stored to preserve old profiles. QEMU version 10.2.4 is unchanged. The frontend RPM is `harbour-sailqemu-0.9.4-1.aarch64.rpm`.

**Validation:** Source assertions and packaging checks are run on the assistant side; the actual Sailfish OS 5.1 SDK compile and device testing still need to be performed.

---

## 0.9.3 Main-page redesign

- Main page: active guest, VM chooser, primary D-Bus Start VM, and a collapsible Other start modes section.
- Manage VMs: select, add, edit, duplicate, or delete profiles. Editing is disabled while a VM is running.
- Add/Edit VM: identity, emulator path, media, boot priority, memory, networking and fullscreen.
- User guide and About are on the Main page pulley menu.
- The Guest console orientation-unlocked icon is now `icon-m-display` to distinguish it from Fit.
- Cover page, QMP control, D-Bus display listener, guest input and existing profiles are unchanged.

# SailQEMU M11 (0.9.2-1) — Guest input and guide

* One-shot **Double click (2×)** action: choose the 2× mouse icon, then tap the
  intended target; two D-Bus left-clicks are sent 110 ms apart, then normal
  Tap mode automatically resumes. The icon does not click at a previously
  selected position.
* **Orientation lock**: freezes the current Guest console orientation, not
  the whole app. Tap the rotate icon again to unlock. The locked icon uses
  the Sailfish screen-lock glyph.
* **SailQEMU guide** now presents the actual themed icons with descriptions,
  including the L/R/2× mouse markings, all four zoom/fit/resync controls, and
  ACPI shutdown behaviour.
* Existing D-Bus render/input, QMP, SDL, NAT detection and VM profile code
  is unchanged.
* Not yet compiled on the Sailfish SDK or tested on device.

# SailQEMU M10r2 (0.9.1-1)

This frontend-only follow-up removes the landscape/portrait Focus/squeeze controls,
keeps the stable right-hand pane width, puts VM status above a fixed 4-icon
action row, and uses 3+3 mouse icon rows with L/R markings. Diagnostics are
available from the Info icon. No network backend or QEMU RPM changes.

---

# SailQEMU M10 (0.9.0-1)

M10 keeps the tested QEMU 10.2.4 D-Bus framebuffer, input, cursor and ACPI behaviour.
The landscape console moves the four zoom actions to a vertical strip beside
the guest display. Zoom increments are 15% instead of 50%. Portrait control
icons follow a notch-aware Sailfish PageHeader, while landscape uses
`CutoutMode.AvoidLandscapeCutout`. The guest framebuffer retains its stable
parent and existing pointer mapping.

**Networking:** the separate M10 networking toolkit builds libslirp and a
second QEMU 10.2.4 RPM, without replacing the proven QEMU installation.
Networking is not enabled by updating this frontend alone.

---

# M9 (0.8.0-1) — Landscape guest panel, compact controls, power-off and display resync

- In landscape the existing live guest frame moves to the fixed **left pane**,
  while the existing Silica control list becomes a narrow, scrollable **right pane**.
  No second framebuffer or input mapping is introduced; portrait keeps the normal flow.
- Main actions, mouse modes, zoom, refresh, and wheel use themed `IconButton`s;
  special guest keys have compact touch targets. The original QMP input path remains.
- `ACPI off` issues QMP `system_powerdown` and explains when a guest ignores it.
  It **never** forcibly quits the VM. `Force off` remains available with Remorse.
- `Resync guest display` tears down only the D-Bus listener, rediscovers the
  console, and requests a fresh scanout; it does not restart QEMU.
- The text `Guest has not initialized the display (yet)` is painted by QEMU
  into its startup placeholder framebuffer; it is **not** a SailQEMU text overlay.
  If the guest stays on that message after display resync, investigate whether
  guest video/console actually initialised (e.g. VGA mode and selected console).

**Known limitation:** QEMU's own placeholder may remain visible when a guest
never initialises the graphical console. Do not assume `CLS` will remove it.

# SailQEMU M8r2 — 0.7.0-2 (mouse/guest cursor alignment)

M8 source-level fix responding to Windows XP testing where the touch crosshair
appeared correctly on the guest image but the QEMU/Windows mouse cursor landed
elsewhere, with larger apparent displacement in Focus display mode.

* Treat **QEMU Console.Width / Console.Height** as the authoritative input
  coordinate space for `org.qemu.Display1.Mouse.SetAbsPosition`. The D-Bus
  display PNG can temporarily represent a different geometry during guest
  video mode changes. Re-query console dimensions and the current Mouse.IsAbsolute mode during the VM's lifetime.
* The mouse touch mapping and separate QEMU cursor overlay now use one inverse
  image-to-console / console-to-image transform, independent of Focus mode,
  zoom and pan. Cursor hotspot is subtracted in guest console pixels.
* `Hide QEMU cursor overlay` allows testing whether a second pointer is already
  being drawn into the Windows guest framebuffer. The original overlay can
  be re-enabled without restarting the VM.
* On the normal Guest console page, a diagnostic label shows PNG frame size,
  QEMU console input size, last cursor coordinates and last sent mouse
  coordinates. Asynchronous D-Bus mouse errors are now reported rather than
  silently treated as successful clicks.
* Per-VM pointer calibration remains available. Start by choosing **Reset
  pointer offset** to remove any calibration from M8; use offsets only after
  checking the native mapping.
* Retains all M8 features: Focus, keyboard shortcuts, D-Bus renderer,
  QMP screenshot fallback, SDL mode and existing saved profiles. This changes
  only the SailQEMU frontend; QEMU 10.2.4 RPM remains untouched.

**Status:** Source-level candidate, not compiled against Sailfish OS SDK or
on-device tested in this environment. If Windows still clicks elsewhere,
please report the diagnostic line and whether hiding the QEMU cursor overlay
removes a duplicate/misaligned cursor. In particular compare the frame size,
QEMU console size and cursor position after a tap at the screen centre.

Build using the same Sailfish OS 5.1 aarch64 SDK as M8. Expected RPM:
`harbour-sailqemu-0.7.0-2.aarch64.rpm`.

---

# SailQEMU M8 — 0.7.0-1 (Sailfish OS 5.1 / Qt 5.6)

Based on M7 (0.6.0-1), **confirmed on the G7 Power to boot Windows XP
with live D-Bus rendering and functional mouse/keyboard controls**. M8 is
a device-test candidate; it has not been compiled in the Sailfish SDK here.

## New in M8

* **Touch alignment**: touch-to-guest coordinate conversion now explicitly
  uses image-local coordinates and the displayed image's native size.
  An 800ms crosshair shows the actual detected touch position, independently
  of QEMU's guest mouse pointer and its cursor hotspot.
* **Saved per-VM calibration**: `Adjust pointer alignment` (only for absolute
  mouse mode) provides X/Y ±4 guest-pixel adjustments and reset. If QEMU's
  pointer lands right of your finger, press `X −`; likewise for other axes.
  The offsets are persisted per profile in `profiles.ini` and automatically
  applied in native guest coordinates after zoom/pan conversion. Calibrations
  are deliberately limited to ±128 guest pixels. If the native mapping is
  already correct, leave offsets at zero.
* **Focus display**: expands the guest viewport, hides verbose diagnostic
  labels and optionally hides the full keyboard. The Pause/Boot controls
  remain accessible as required; `Normal view` returns to the original layout.
* **Guest shortcuts**: Windows Start menu (Ctrl+Esc), Alt+Tab, Alt+F4 on
  a compact row when the keyboard is shown. QMP send-key remains the same
  known-good mechanism; Win95/XP may respond differently in setup screens.
* Preserves SDL fallback, live D-Bus framebuffer, QMP screenshot fallback,
  Tap/Drag/Pan, mouse wheel, guest cursor, boot/controls, and profiles.
* **Networking remains disabled in the supplied QEMU 10.2.4 RPM**. The
  explicit no-SLIRP diagnostic stays in place; this frontend update does
  not rebuild QEMU or change networking.

## Build and test

Build using the same Sailfish OS 5.1 aarch64 SDK target and package command
as M7. No additional build dependencies. Expected output:
`harbour-sailqemu-0.7.0-1.aarch64.rpm`.

1. Boot Windows XP using D-Bus mode and verify Tap at 1× and 1.5× zoom,
   including after changing orientation or panning. The crosshair should
   appear directly under the contact point; compare the guest cursor position.
2. If there is an offset, expand `Adjust pointer alignment`; adjust X/Y a few
   guest pixels at a time. Stop once guest clicks land under the crosshair.
3. Navigate back to SailQEMU and reopen the Guest console; offsets should
   remain saved. Restart SailQEMU and confirm calibration persists.
4. Toggle `Focus display` / `Normal view` and `Show/Hide keyboard` in portrait
   and landscape. Test Start, Alt+Tab, Alt+F4 inside Windows XP.
5. Boot FreeDOS using D-Bus and check Enter, DIR/CLS, arrows, and live display.
6. Verify the existing SDL fallback still starts separately.

If the cursor crosshair is correctly placed but the Windows cursor offset
persists even after calibration, report whether `Mouse ready: absolute` is
shown and the direction/approximate distance of the mismatch at the top,
center and bottom of the display. A displacement increasing toward the edges
indicates a scale/resolution issue rather than a constant offset.

---

# SailQEMU M7 — 0.6.0-1 (Sailfish OS 5.1 / Qt 5.6)

M6r2 (0.5.0-2) is the known-good live D-Bus framebuffer release tested on the
G7 Power. This M7 build is a **device-test candidate**, not yet SDK-compiled
or runtime-validated by its author.

## New in M7

* **Return to guest console** appears near the top of the main page whenever
  a VM is running. Navigating back from the console leaves the VM running.
* In the D-Bus console, choose **Tap**, **Drag**, or **Pan**. Tap sends a left
  mouse click; press-and-hold sends a right click. Drag presses the left button,
  moves the pointer at most ~25 times/s, then releases it. Pan lets you scroll
  a magnified guest display without injecting mouse events. Extra buttons send
  right click and wheel up/down at the guest's existing cursor position.
* Mouse events go via QEMU 10.2.4's `org.qemu.Display1.Mouse` interface on
  the graphical console and are bound to the QEMU instance already verified
  by `servicePid`. The existing `-usb -device usb-tablet` supplies absolute
  positioning for most Windows guests. Relative mice are supported as well,
  though initial pointer position is unspecified until movement begins.
* QEMU `CursorDefine` and `MouseSet` messages are rendered as a **separate
  cursor overlay** above the D-Bus PNG framebuffer when available. On guests
  that do not transmit a cursor image, the overlay is omitted; mouse events
  still work. Input and cursor support are experimental until device-tested.
* The **Enable guest networking** option now probes `-netdev help` before
  launching QEMU and reports a clear error if user-mode NAT/SLIRP is absent.
  **This does not add networking** to the previously packaged QEMU 10.2.4 RPM.
* The original SDL buttons, QMP keyboard, QMP screenshot fallback, VM profiles,
  disk/floppy/ISO handling and M6 live framebuffer remain available.

## Build

Build exactly as M6r2 with your Sailfish OS 5.1 aarch64 SDK. No new
BuildRequires. Expected RPM: `harbour-sailqemu-0.6.0-1.aarch64.rpm`.
QEMU stays separately installed at
`/opt/sailqemu/qemu-10.2.4/bin/qemu-system-x86_64`.

## M7 on-device acceptance tests

1. Confirm the display listener self-test:
   `/usr/bin/harbour-sailqemu-display-listener --self-test`.
2. Boot FreeDOS with **Start D-Bus + controls (experimental)**, then
   **Boot / Resume**. Confirm live display and text input still work.
3. Use Sailfish Back to reach the main page. Tap **Return to guest console**;
   confirm the same running VM resumes in the console, without a second boot.
4. If you have a functioning Windows XP image, start it in D-Bus mode. Check
   that `Mouse ready: absolute (USB tablet)` appears; then test Tap, right-click,
   Drag (e.g. moving a window), Wheel, Pan and different zoom levels.
5. Look for the Windows cursor on the guest image. Depending on the emulated
   video hardware and guest driver QEMU may not publish a CursorDefine;
   absence of the overlay alone does not mean input is broken.
6. With the VM stopped, enable guest networking and try to start with the
   existing QEMU 10.2.4 binary: expect a clear missing-SLIRP error instead of
   a silent VM exit. Turn networking off again and verify FreeDOS boots.
7. Test **Start SDL only (legacy)** once for regression. This mode still has
   no integrated D-Bus mouse; the separate SDL window manages its own input.

If the SDK build fails, report compiler output. If mouse support fails,
report `D-Bus` and `Mouse ready` labels, and the QEMU log file path shown in
SailQEMU's Diagnostics section. All guest disk images are untouched by this
upgrade, but make a backup before testing Windows guest disks as usual.

---

## M6r2 — Sailfish OS 5.1 build compatibility (0.5.0-2)

The M6r1 Sailfish aarch64 build failed only in `src/dbusdisplayhelper.cpp`:

* Qt5's `signals` macro expanded GLib's `GDBusSignalInfo::signals` member.
  The listener now compiles with `QT_NO_KEYWORDS` **only for the helper target**,
  leaving the existing Qt application and its `signals:` sections unchanged.
* The helper used unsupported `Qt::hex` and `Qt::dec` debug manipulators.
  These now use `QString::number(fmt, 16)`.

No QEMU recompilation is required. Build this source using the existing
Sailfish SDK 5.1 aarch64 target. Expected RPM: `harbour-sailqemu-0.5.0-2.aarch64.rpm`.
The listener self-test after installation is:

```sh
/usr/bin/harbour-sailqemu-display-listener --self-test
```

Expected output: `M6 SELF-TEST PASS: pixman RGB32 to Qt PNG`.
Then launch FreeDOS with **Start D-Bus + controls (experimental)** and
**Boot / Resume**. The live D-Bus display path still requires on-device
validation; the existing QMP screenshot fallback is preserved.

---

# SailQEMU 0.5.0-2 — M6 live QEMU D-Bus framebuffer (device-test candidate)

**Build target:** Sailfish OS 5.1 / Qt5 / Silica, aarch64. **QEMU** is a separate RPM:
`/opt/sailqemu/qemu-10.2.4/bin/qemu-system-x86_64`.

## Changes from confirmed-working 0.4.0-1 (M5)

The experimental **Start D-Bus + controls** mode now launches a dedicated
unprivileged process `/usr/bin/harbour-sailqemu-display-listener` after the
frontend has validated that the session-bus `org.qemu` service belongs to the
QEMU process started by SailQEMU. The helper repeats the PID check, registers
as QEMU's peer-to-peer D-Bus listener by passing an AF_UNIX socket FD through
`Console.RegisterListener`, then receives `Scanout` and `Update` frames.

Frames are copied from the D-Bus message, bounds-checked, converted from
common 32-bit pixman layouts to QImage RGB32 and written atomically as PNG
at most ~8 times per second. The Guest console displays these frames with
its existing zoom and pan. QMP **send-key**/boot/pause/resume work unchanged.
The first frame may take a moment; until a supported D-Bus frame arrives,
the existing QMP screendump preview remains the fallback.

- D-Bus listener does **not** request shared-memory mapping/DMABUF; those
  performance paths are future work. This software renderer is deliberately
  limited to 4096×4096 RGBA 32-bit frames and 64 MiB per frame.
- SDL launch remains unchanged and opens a separate window.
- The session `org.qemu` bus name is still global (only one M5/M6 D-Bus guest
  per user session). The *listener* connection is peer-to-peer, not the main
  VM connection. Multi-instance QMP FD-handoff has **not** been implemented.
- The live cursor overlay, mouse/touch injection and clipboard integration
  are not yet implemented.
- The PNG path is a transitional bridge. A direct in-memory Qt framebuffer
  provider will avoid temporary PNGs once the protocol and source pixel
  formats have been tested on the device.
- No new persistent profile keys and no changes to existing guest media.

### SDK dependencies

This release uses Qt5 Core/Gui/DBus (already used in M5) plus GLib/GIO,
GIO-Unix, and Pixman development headers, packaged as
`pkgconfig(gio-2.0)`, `pkgconfig(gio-unix-2.0)`, and `pkgconfig(pixman-1)`.
The helper is compiled from source in the **same SailQEMU RPM**; no additional
runtime service needs to be configured. The QEMU RPM is untouched.

### On-device test sequence

1. Stop the running VM. Back up 0.4.0-1 and your existing `profiles.ini`.
2. Build/install 0.5.0-2 on SFOS 5.1 aarch64.
3. Run `/usr/bin/harbour-sailqemu-display-listener --self-test`.
   It must print `M6 SELF-TEST PASS` before proceeding.
4. Confirm **Start SDL only (legacy)** still boots FreeDOS in a separate
   window; stop QEMU.
5. Confirm guest networking is disabled, and the QEMU profile executable is
   the tested `/opt/sailqemu/qemu-10.2.4/bin/qemu-system-x86_64`.
6. Choose **Start D-Bus + controls (experimental)** and press **Boot / Resume**.
   Watch the `Live QEMU D-Bus frame` indicator on Guest console.
7. Use DOS commands `DIR`, `CLS`, `VER` and test boot-menu arrow keys. If the
   image refreshes much faster than the old 1-second screenshot interval,
   the actual framebuffer listener is operating. Zoom, pan and rotate.
8. Stop and reopen the application; verify no helper stays running after VM
   shutdown and the saved profile is still available.

If the image continues updating through the **QMP fallback** instead of
`Live QEMU D-Bus frame`, send the Guest console status and these outputs:

```sh
ps -ef | grep -E 'sailqemu-display-listener|qemu-system' | grep -v grep
ls -l /usr/bin/harbour-sailqemu-display-listener
rpm -q harbour-sailqemu
```

The source-only checks (`python3 tests/check-source.py`) passed here. There
is **no Sailfish SDK compiler or real QEMU D-Bus listener service in this
workspace**, so compilation, authentication, framebuffer transfer and
refresh rate require your device validation. Do not replace the tested 0.4.0
RPM archive before M6 is confirmed.

QEMU D-Bus protocol: https://www.qemu.org/docs/master/interop/dbus-display

---

# M5 (0.4.0-1) — First QtDBus display discovery for QEMU 10.2.4

- M4 is verified: installed `/opt/sailqemu/qemu-10.2.4/bin/qemu-system-x86_64` boots FreeDOS using SDL; `-display dbus,p2p=yes` passed the QMP smoke test.
- M5 adds **Start D-Bus + controls (experimental)**. The guest starts paused (`-S`) with `-display dbus,gl=off` and *no SDL window*. The existing console page still uses QMP `screendump` and `send-key`; press Boot / Resume there to start FreeDOS.
- The native QtDBus connection discovers the `org.qemu` session-bus service, checks the **D-Bus service PID matches the QMP-launched QEMU PID** (avoiding incorrect attachment), reads the VM `Name`, `ConsoleIDs`, and console `Type`, `Label`, `Width`, `Height` properties and displays them on the main page. Press **Refresh D-Bus consoles** to update.
- This deliberately uses the QEMU **session-bus mode**, not `p2p=yes`; the latter requires QMP Unix FD passing. It is only a temporary discovery phase. Other QEMU instances holding `org.qemu` will prevent M5 D-Bus mode from starting. The M6 framebuffer/listener phase will use a private peer-to-peer QMP socket-FD hand-off to avoid conflicts.
- SDL starts, QMP control and periodic screenshot preview from 0.3.3-2 remain unchanged. Existing VM profile files are not migrated or overwritten. New default profiles point to the packaged QEMU 10.2.4 executable; previously saved paths stay unchanged.
- M5 D-Bus mode requires networking off: installed QEMU 10.2.4 lacks libslirp. Fullscreen setting applies only to SDL.
- The QtDbus client has **no live framebuffer yet**, nor D-Bus keyboard/touch integration. That is for M6. Nothing modifies a guest disk outside normal QEMU guest operations.

## M5 test procedure

1. Stop running VMs. Keep the known-good `harbour-sailqemu-0.3.3-2.aarch64.rpm` for fallback; back up your profiles.ini.
2. Build `harbour-sailqemu-0.4.0-1` in the Sailfish OS 5.1 aarch64 SDK (needs `Qt5DBus` development dependency in target).
3. Confirm regular **Start + controls** still starts the SDL window and FreeDOS boots; stop it.
4. Ensure QEMU path is `/opt/sailqemu/qemu-10.2.4/bin/qemu-system-x86_64`, **Enable guest networking** is off. Tap **Start D-Bus + controls (experimental)**, then **Boot / Resume** on guest-console page.
5. Return to main page. Observe `D-Bus: Connected to ... (PID ...); ... console(s).` and at least `Console 0` with dimensions. Tap **Refresh D-Bus consoles**.
6. Verify QMP keyboard works via guest console, pause/resume works, and the QEMU process stops cleanly via Force off (Remorse). Repeat startup and inspect log if D-Bus metadata fails.
7. Confirm reopening the app while a D-Bus VM runs recovers QMP, its D-Bus mode, and console metadata.
8. Regress the regular SDL mode after finishing.

M5 source is statically checked only. No Sailfish SDK compilation or on-device D-Bus metadata test has been executed in this environment.

Official reference: https://www.qemu.org/docs/master/interop/dbus-display

---

## M3r4r2 (0.3.3-2) — Guest console QML loading fix

- Fixes `GuestKeyboardPage.qml: Cannot assign to non-existent property "onAccepted"`.
- Sailfish Silica `TextField` has no `accepted` signal. Use `EnterKey.onClicked` for the software keyboard's Enter key and physical Return/Enter.
- Does not change QEMU, profile storage, guest disks or QMP functionality.
- Rebuild to install `harbour-sailqemu-0.3.3-2.aarch64.rpm` and retest **Start + controls**, **Open guest console**, and the virtual keyboard's Enter action.
- This source archive has static checks only; SDK compilation and on-device behavior still need validation.

## M3r4 (0.3.3) — Console-first FreeDOS boot and zoom

- **Start + controls** starts QEMU with `-S` (CPU initially paused) and immediately opens the guest console; tap **Boot / Resume** after the console connects. This avoids missing short FreeDOS boot-menu timeouts while navigating to keyboard controls.
- **Start SDL only (legacy)** retains the normal M3r3 startup behaviour.
- Guest console now presents QMP screenshot, zoom controls (1×–4×), drag-to-pan, immediate boot-menu arrows, Enter and Esc, and a Sailfish text entry field in one place. Portrait and landscape orientations supported.
- Screenshot refresh is approximately every 1 second **and is not a real-time renderer**; the separate SDL window remains active. Zoom changes the screenshot preview only, not the SDL window or FreeDOS resolution.
- No VM profile migration, QEMU reinstall or disk image modifications.
- Caution: if a guest boots through a short text boot menu, wait for QMP to connect, tap Boot / Resume, then use navigation controls. Preview may show a menu up to ~1 second late.

# SailQEMU 0.3.2 / M3r3 — Guest keyboard and preview

This release adds a **Guest keyboard / preview** page to control FreeDOS using
QEMU 4.1's QMP `send-key` command, and to view the guest screen via QMP
`screendump` still frames. There is no VNC connection. **The preview is not
real-time**; the native SDL/Wayland guest window is still the main display.

To try it, start the FreeDOS VM as before, wait for QMP to connect, then tap
**Guest keyboard / preview**. Enter `VER` and tap **Send + Enter**. The
on-screen keyboard enters text into the Sailfish field; pressing Send pushes
physical US PC key codes into the guest. Use the special-key buttons for
Enter, Escape, cursor keys, function keys and Ctrl+C. The preview updates
about every 2 seconds; tap **Refresh preview** if it is stale.

QMP's `send-key` is asynchronous. SailQEMU spaces keystrokes by 130 ms and
holds each for 65 ms to reduce dropped keys. Only US keyboard ASCII is
supported; non-ASCII characters are rejected without transmitting a partial
command. This isn't Unicode text injection and is unsuitable for pasting
passwords or sensitive text. It does not change the guest disks or store the
commands in the VM profile. The temporary screenshot is in the app cache.

The OS is still displayed by **QEMU SDL as a separate Sailfish window**.
The next substantial milestone is modern QEMU with D-Bus display, which
will permit the keyboard and guest display in the same application window.

---

# Previous milestone — M3r2 floppy support (0.3.1-1)

Target: Sailfish OS 5.1, Qt 5 / Silica, aarch64. Existing QEMU 4.1 built with SDL2/Wayland remains external and untouched. Source candidate: requires compilation and on-device verification.

## M3r2 fix — FreeDOS boot floppy support

* An `.fd` or `.ima` is a raw **floppy** image, not a CD-ROM ISO. M3 mistakenly accepted it in the ISO field and passed `-cdrom`, causing SeaBIOS CD boot failure.
* Added **Floppy image (A:)**, Browse button, and **Boot from floppy first** preference, independently stored for each VM profile.
* Existing M3 profile values ending in `.fd` or `.ima` in the ISO field are **automatically migrated** to the new floppy field (without editing the actual image). `.img` is left untouched because it may be a hard disk or a floppy.
* The boot order now includes drive `a`: e.g., `ac` for floppy plus HDD, or `adc` for floppy plus ISO plus HDD. Turning off **Boot from floppy first** puts floppy last.
* Boot floppy images use QEMU `-drive file=...,if=floppy,index=0,format=raw,readonly=on`, to avoid modifying your boot diskette. QEMU will still write to the separate HDD image if provided.
* `.fd`/`.ima` entered directly in ISO field now produce a clear validation error.
* Do not treat a nonbootable/blank HDD `.img` as an install failure: a DOS installer must partition, format and install boot files before HDD boot works.

## Fixes since M2r2

* **Explicit Choose / manage VMs button** on the main page instead of a hidden clickable profile-name row.
* **Per-profile fullscreen toggle** uses `TextSwitch.automaticCheck: false` and only writes the C++ property in the user `onClicked` handler. This avoids a broken `checked` binding and prevents toggling one profile from altering the control's state for another.
* Existing M2 profiles remain in place, with no destructive migration. `profiles.ini` schema adds two optional booleans; missing values default to old behaviour.

## New in M3

* **Browse disk image / Browse boot ISO** via Sailfish's native file picker (loaded only when requested). Paths can still be entered manually. Recognized disk extensions `.img`, `.raw`, `.qcow2`, `.vmdk`, `.vdi` select an appropriate default disk format; verify the actual format before starting a guest.
* **Boot ISO first** preference, per profile. When ISO is mounted, `-boot order=dc` (on) or `-boot order=cd` (off). When no ISO is mounted, use `-boot order=c`.
* **Enable guest networking** preference, per profile; off by default. When on, QEMU 4.1 user-mode NAT/SLIRP and emulated RTL8139 NIC are configured. No incoming ports are forwarded. Guest drivers are still required.
* `scripts/qemu-packaging-inventory.sh`: read-only information gathering for a future independently built QEMU RPM. **M3 does not package QEMU.**

## Build

Unpack and build with your **Sailfish OS 5.1 aarch64** target as in M2:

```sh
cd harbour-sailqemu-0.3.2
mkdir -p build && cd build
sfdk build ..
```

Expected RPM: `harbour-sailqemu-0.3.2-1.aarch64.rpm`. If your environment uses the `dhd/helpers/build_packages.sh` wrapper, use the same command you used for M2.

Install the new RPM normally. Do not change or remove any QEMU or existing VM disk files. Shut down any running guest before upgrading. Make a backup of your existing `profiles.ini` as usual.

## Recommended tests

1. From icon, open SailQEMU. **Choose / manage VMs** must be clearly visible.
2. With VM stopped, select Default VM, turn Start fullscreen **on**, Save profile. Select DOS, confirm **off**, then switch back and confirm **on**. Close and reopen and repeat. This is the regression test for the reported issue.
3. Toggle **Enable guest networking** independently between the two profiles; repeat selection/restart test.
4. Attach a boot ISO, toggle ISO boot priority per profile, and verify it is remembered. Do not assume BIOS can actually boot a non-bootable ISO.
5. Test **Browse disk image / Browse boot ISO / Browse floppy image**. Picker may be slow on devices with large file indexes. Path fields remain usable without picker.
6. Start diskless test and confirm Start/Pause/Resume/Remorse Force off still work, including on the home screen after minimizing. QEMU display still uses separate native SDL window, no VNC.
7. For FreeDOS boot, attach your `freedos.fd` via **Floppy image (A:)**, leave ISO empty unless you have a real `.iso`, and enable **Boot from floppy first**. If you previously used `freedos.fd` as ISO in M3, check that it migrated automatically.
8. If starting a real guest disk, make a backup of the disk first. Neither the picker nor the profile manager modifies a disk, but QEMU guest writes do.

### RPM preparation

On G7 Power, from the extracted M3 source:

```sh
QEMU_BIN="$HOME/Downloads/edp17Apps/sailQemu/qemu-4.1.0-sdl-poc-build/x86_64-softmmu/qemu-system-x86_64" \
  ./scripts/qemu-packaging-inventory.sh > qemu-inventory.txt
```

This doesn't construct an RPM; it records the QEMU version, binary locations, firmware, dependencies and toolchain information. We should package a newer maintained QEMU version in an independently installable RPM after confirming what can be built on the Sailfish 5.1 device.

## Known limitations

* QEMU 4.1 supports no D-Bus display backend. Guest appears in a **separate SDL/Wayland window**, not yet embedded in the Silica page.
* `Sailfish.Pickers` is a separate QML module; the picker page is only loaded on demand. File selection and UX require device testing.
* Launcher keeps `X-Sailfish-ApplicationType=Silica`, Sailjail disabled for development.
* Only x86_64/PC guest, and one running VM at a time. ACPI shutdown doesn't work with diskless SeaBIOS.
* Existing `.fd`/`.ima` ISO field values are migrated to floppy on first launch. Other profile fields remain intact.


## M9r2 (0.8.1) — Layout stabilisation

The Guest console has a permanent display host in both orientations; only its
size changes. In portrait a compact fixed toolbar and guest pane sit above
independently scrolling controls. In landscape the sidebar is at least 340 px
wide (up to 460 px). Connection diagnostics and pointer calibration are under
**Details**. The Main page has a **SailQEMU guide** pull-down menu and
collapsed VM configuration and diagnostics sections. D-Bus input geometry,
QMP control and ACPI shutdown logic are unchanged from M9.


## M10r2 (0.9.1) guest-console controls

The landscape sidebar has four predictable rows: VM status; Keyboard,
Pause/Resume, ACPI off, Info; Tap/left click (L), Drag, Pan; Right click
(R), Wheel up, Wheel down. The previous Focus/squeeze-controls toggle is
removed in both orientations. All detailed explanations live in the
SailQEMU guide; the Info icon retains the on-device diagnostics. No changes
to the QEMU 10.2.4 RPM, network probes or D-Bus input mapping.


### M14r2 validation and media inspection

New QML editor saves the name and configuration together. The configured
media inspection action reports exact file paths/formats/sizes/IDE slots;
when running, Guest console → Info → Inspect QEMU attached drives invokes
QMP `query-block` to show which images QEMU actually mounted.

See `docs/M14r2-media-inspection.md` for test procedure. These commands
never write to guest disk images or reformat partitions.


### M14r3: QCOW v1 image support and guard

QEMU can attach older QCOW version 1 images (`format=qcow`), which are
**not** QCOW2. Some files have a `.img` suffix; the picker now checks the
first eight bytes before assuming an image is raw. The configured-media
inspector reports version mismatches, and VM startup refuses to open a
recognised QCOW1/2 image with the wrong declared format.

The missing `icon-m-info` images now use Sailfish `icon-m-about` instead.
See `docs/M14r3-qcow-v1-fix.md` for a read-only command-line test.


## M14r4 — Legacy relative PS/2 pointer fix (0.12.0-4)

The D-Bus mouse interface can be **absolute** (supported USB tablet) or
**relative** (PS/2). Direct on-screen tap-to-position only works for an
absolute device with a functional guest driver. The old relative mouse path
incorrectly interpreted differences between successive tap coordinates as
relative PS/2 movements; Windows 95 could consequently click far from the
visible cursor.

For relative mode, SailQEMU now treats the guest display like a laptop
touchpad: swipe to move the existing Windows pointer, tap to click at its
current position, long-press for right-click, and use Drag mode for click-and-hold
movement. Absolute USB-tablet input (tested with Windows XP) is unchanged.

For Windows 95, disable **USB tablet absolute pointer** in the VM settings,
fully power off the guest, and boot it again. This selects QEMU's conventional
PS/2 relative mouse. A Windows 95 installation without USB tablet/HID drivers
will not necessarily use QEMU's absolute device even if the emulator advertises
`IsAbsolute=true`. This change does not install guest drivers.

See `docs/M14r4-relative-pointer.md` for the test checklist.


## M16 — reliability and measurable performance (0.14.0-1)

Disk tools now shows results near the top and scrolls there on completion;
cancellation is asynchronous. The new read-only Diagnostics page reports
QEMU and frontend CPU (one-core normalisation), RSS memory and observed
D-Bus framebuffer file updates, along with presence checks for QEMU,
qemu-img, the display listener and launch script. This does **not** replace
benchmarking guest-side performance or count Qt Quick presentation FPS.

This frontend depends on a separately installed QEMU 10.2.4 package; it
does not bundle or upgrade QEMU. The selected VM's configured executable
determines which qemu-img is inspected/used. Keep important disk images
backed up before experimenting with guest OS installation or cloning.
