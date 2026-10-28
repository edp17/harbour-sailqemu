# M16r3 — navigation and first-run packaging checks (0.14.2-1)

The preceding M16r2 (0.14.1-1) was confirmed working on-device. This
increment does not change the QEMU command line, VM profiles, disk manager,
display listener, mouse input, QMP, or shutdown behaviour.

## Sailfish pulley menus

* **Main**: Manage VMs, User guide, About (3 items).
* **Manage VMs**: Add new, Disk tools, Diagnostics (3 items).
* Direct links from Add/Edit VM to Disk tools and Guest console Info to
  Diagnostics are retained.
* The guide now points to Manage VMs rather than Main for both tools.
* A brief first-run guide section identifies the separately installed QEMU
  executable and distinction between network-enabled and standard packages.

## Read-only release check

After installing the frontend and a compatible QEMU 10.2.4 package, run:

```sh
/usr/share/harbour-sailqemu/bin/m16-release-check.sh
```

You may optionally specify an explicit QEMU binary as the first argument.
The script checks expected installed binaries, launcher, desktop entry, icon,
and important QML pages. It **does not** launch VMs, inspect or change disks,
modify profiles, or download dependencies. The check confirms file
presence/executability/readability, **not** a successful VM boot or full
runtime dependency compatibility. The QEMU RPM is independent of the
frontend RPM and must be installed separately.

## On-device check list

1. Verify Main has exactly three pulley actions; Manage VMs has exactly three.
2. Open both moved actions, and confirm Direct links from Edit and Guest
   console still navigate correctly.
3. Run the preflight command and check PASS (or report any missing components).
4. Start and shut down a previously working guest to check for regressions.

No SDK build or device test is represented by the static regression suite.
