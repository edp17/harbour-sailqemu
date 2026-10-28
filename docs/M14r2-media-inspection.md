# M14r2 — Read-only media diagnostics and cloned-name correction

## Verify an active VM

1. In Add/Edit VM, select **Inspect configured media**. The report shows
   resolved filenames, file sizes, declared formats and assigned slots.
   This only describes the settings about to be launched.
2. Start the VM, open **Guest console** → **Info** → **Inspect QEMU attached drives**.
   The report uses QMP `query-block` to list *actual* inserted devices,
   filesystem paths, QEMU image drivers and read-only/writable status.
   This is read-only; no disk or partition changes occur.
3. If there is no IDE hard disk in the QMP report, capture the report and
   the QEMU launch log. If IDE disk exists, inspect the image itself.

## Image inspection on the phone

```
QIMG=/opt/sailqemu/qemu-10.2.4-net/bin/qemu-img
[ -x "$QIMG" ] || QIMG=/opt/sailqemu/qemu-10.2.4/bin/qemu-img
"$QIMG" info "/path/to/your/windows95.img"
"$QIMG" info "/path/to/your/ReactOS-0.4.16-i386.iso"
```

Only inspect original disk images when no VM is using them. Ensure QEMU's
reported image format matches the format selected in the VM configuration.
The `.img` suffix does NOT guarantee a raw format.

## Windows 95

If the A: boot floppy starts DOS, it does not prove the hard disk is readable.
QEMU boot order (`-boot order=...`) chooses where BIOS looks for boot code;
DOS assigns C:/D:/R: based on its own drivers, partitions and filesystems.
The Windows 95 screenshot shows an optical driver mounted as **R:**.

Check from the DOS prompt, without modifying partitions:

```
FDISK /STATUS
DIR C:\
DIR R:\
```

**Do not** run `FDISK` without `/STATUS`, `FORMAT`, `SYS`, or filesystem repair
against an image whose contents are valuable. If partitions aren't DOS-readable,
check for a missing partition table, NTFS, an incompatible image format, or an
image that is not a complete hard disk.

## ReactOS

A ReactOS splash or stop screen indicates the firmware reached guest boot code.
It doesn't demonstrate an attached HDD problem by itself. Try a known suitable
ReactOS image using 1 vCPU, standard VGA, 256–512 MiB RAM, default CPU; then
record the full STOP code if it crashes. Boot ISO should use the correct raw
format and be selected as primary boot.

## Profile cloning

Cloning duplicates only settings, NOT the underlying writable HDD. Two profiles
pointing to the same HDD do not create independent installations. Don't run or
modify a cloned installation assuming it is a separate copy.
