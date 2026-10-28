# SailQEMU 0.5.0-1 — public beta source preparation

## Status

Source-version reset from device-tested `0.14.3-1` to `0.5.0-1`. The VM engine,
D-Bus listener, display scheduling, mouse handling, disk tools and existing
profile format have **not** been changed. The 0.5.0 SDK build and on-device
installation are **not yet verified**. This is a candidate, not a published
GitHub release or an OBS build.

## Runtime layout and dependencies

* Frontend: `/usr/bin/harbour-sailqemu`
* Display listener: `/usr/bin/harbour-sailqemu-display-listener`
* Tested QEMU 10.2.4 executable with networking:
  `/opt/sailqemu/qemu-10.2.4-net/bin/qemu-system-x86_64`
* Companion `qemu-img` in the same QEMU `bin` directory
* Dynamic libslirp 4.9.5: `/opt/sailqemu/libslirp-4.9.5/lib64`

QEMU and libslirp are not included in the frontend RPM. A fresh installation
cannot run VMs until its separately packaged QEMU executable is installed.
Do not claim OBS reproducibility until both source packages have built and
passed guest tests on both architectures.

## Version reset and installation

`0.5.0` is **less** than installed development version `0.14.3` in RPM version
comparison. Consequently, ordinary `zypper update` will not perform this move.
Back up VM profiles and disk images first. Build 0.5.0 with your normal
SDK workflow; on a test device that already has 0.14.3, either explicitly
allow the RPM downgrade using the package manager's documented option or
uninstall and reinstall the frontend package while preserving user data.
Check the transaction summary before confirming: it must not remove your
QEMU or libslirp packages. Do not wipe the app's configuration directory.

## Release checklist

1. Confirm clean SDK builds for `aarch64`, then `armv7hl`.
2. Confirm first-run help and the two-action Diagnostics pulley menu.
3. Test existing FreeDOS, Windows 95 QCOW1, Windows XP and ReactOS profiles.
4. Test NAT, D-Bus framebuffer, mouse, keyboard, QMP shutdown and return.
5. Test RAW/QCOW2 creation, inspection and non-overwriting conversion.
6. Run `/usr/share/harbour-sailqemu/bin/m16-release-check.sh` after install.
7. Publish from the tested Git commit/tag, and only then label the beta stable.

## GitHub first push (new empty repository)

Create the **empty** repository `edp17/harbour-sailqemu` under your GitHub
account first; do not add a GitHub-generated README/license/gitignore, as
this source already contains them. From the extracted source directory:

```sh
git init -b main
git add .
git commit -m "SailQEMU 0.5.0 public beta source"
git remote add origin https://github.com/edp17/harbour-sailqemu.git
git push -u origin main
# After local SDK/build checks and installation tests:
git tag -a v0.5.0 -m "SailQEMU 0.5.0 beta"
git push origin v0.5.0
```

Do **not** tag prior to verification. A GitHub source repository is not proof
that QEMU/libslirp OBS dependencies are available.
