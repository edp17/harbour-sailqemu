# M15 — disk tools (0.13.0-2)

M15r2 fixes the `QTemporaryDir::filePath()` Qt 5.6 compilation failure. The staging path is now constructed using `QDir(m_stage->path()).filePath(...)`; behaviour is otherwise unchanged.

Independent `qemu-img` operations use the `qemu-img` binary next to the VM's configured QEMU executable. Tested target is Sailfish OS 5.1 / Qt 5.6, QEMU 10.2.4.

- Inspect image: asynchronous `qemu-img info --output=json`, reports format, virtual and allocated sizes, backing filename and VM profile references.
- Create: asynchronous `qemu-img create -f raw|qcow2`, capacity 16–131072 MiB.
- Copy/convert: asynchronous `qemu-img convert -p -O raw|qcow2` for an existing image. Works with QCOW v1 sources; conversion flattens backing chains.
- Both writers use a private temporary directory *on the target filesystem*. On success, publish by `QFile::rename`; never knowingly overwrite an existing file. Cancellation or failure removes the staging directory.
- While any SailQEMU VM is running, these operations are blocked. External QEMU processes are not detected: do not work with an image open in another emulator.
- The newly created/converted image is *not* attached automatically. The explicit Use-as-primary button updates the currently selected VM and saves it.
- No delete, in-place shrink, in-place conversion or snapshot modification is provided in this iteration.

## Device checks

1. Keep a backup of existing images. With VMs shut down, open Main > Disk tools.
2. Inspect an existing QCOW1 Windows 95 image: expect `format: qcow` and `virtual size: 1024 MiB`.
3. Create a 64 MiB QCOW2 in a new filename and inspect it. `qemu-img info` should report qcow2.
4. Convert a **copy** of a small image to QCOW2; inspect target size/format, then verify the source unchanged.
5. Try creating to the same target again: expect refusal without modifying it.
6. Duplicate a VM profile and inspect the primary HDD: the report should identify both references. Use Copy/convert to create independent storage and explicitly attach the new image only to the duplicate profile.
7. Start any guest, return to Disk tools; write actions should be disabled.

No Sailfish SDK compilation or on-device test was performed when preparing this source; verify before production use.
