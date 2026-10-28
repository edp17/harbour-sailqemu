# M16r4 — Diagnostics Silica navigation

Base: SailQEMU 0.14.2-1 (M16r3), verified on device. New version: 0.14.3-1.

* Keep **Refresh now** in the Diagnostics `PullDownMenu`.
* Move **Copy report** from a bottom-page `Button` to the same `PullDownMenu`.
* Remove the bottom-page **Refresh now** button; there are exactly two pulley items.
* Copy refreshes the report first, then copies it to the system clipboard.
* Preserve the automatic 2.5-second sampling and all runtime data.
* No changes to VM management, QEMU launch, storage, display or input.

## Device test

1. Navigate to Main > Manage VMs > Diagnostics; pull down to see two actions.
2. Tap **Refresh now** and confirm report updates normally.
3. Tap **Copy report**, paste into a text field and verify it begins with the
   read-only diagnostics heading and includes runtime checks.
4. Confirm no Refresh or Copy buttons appear below the report.
5. Verify that an existing guest still starts, can be controlled and shuts down.

Static source checks cannot replace the Sailfish Qt 5.6 SDK build or the device test.
