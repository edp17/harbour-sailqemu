# M14r3 — QCOW v1 disk-image fix

Observed: `qemu-img info W95_c.img` reports **qcow** (version 1), 1 GiB virtual,
108 MiB allocated. FDISK inside a VM incorrectly configured with `format=raw`
sees only about 108 MiB and cannot find the DOS partition.

## Changes

- Detect QCOW v1 (`QFI\xfb` and big-endian version 1) and QCOW2 (versions 2/3)
  from the first eight bytes, regardless of `.img` filename.
- Add `qcow (v1)` to HDD, secondary HDD and optical image format selectors.
- Reject misconfigured QCOW images *before* QEMU starts. This is important:
  treating the QCOW container as raw and allowing guest writes can damage it.
- Show actual QCOW header version in the configured-media report.
- Replace missing Silica icon `icon-m-info` with `icon-m-about`.
- Avoid reading a QProcess that was never opened during listener teardown.

Existing profiles are not modified automatically when loaded. Select the correct
format in VM settings (or reselect the image using Browse), save, and inspect the
configured-media report. The runtime guard prevents the old misconfiguration from
being used again. A duplicate profile points to the **same HDD image**, so do not
run the original and duplicate simultaneously.

For diagnostic tests, use the installed QEMU 10.2.4 on the device, specifying
`-drive file=...,format=qcow,if=ide,index=0` and `-snapshot` to protect the source.
