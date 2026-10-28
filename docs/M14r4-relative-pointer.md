# M14r4 — Windows 95 PS/2 pointer test

1. Duplicate your working Windows 95 **profile** for testing. Cloning a profile does not copy a writable disk image; do not use the same writable image in two running VMs.
2. In Hardware, disable **USB tablet absolute pointer**. Keep the working QCOW v1 format, boot order, CPU and video settings. Fully stop QEMU, then start Windows 95.
3. Open the Guest console's **Info** section: the mouse status should say **relative movement**, not **absolute (USB tablet)**.
4. In Tap mode, **swipe your finger on the guest image**. The Windows cursor should move in the same general direction. Release and **tap anywhere** to click at the cursor's current position. This does NOT mean direct screen-coordinate clicking.
5. Test the R button, long-press right click, Drag mode, and double-click at the cursor position. Make sure Windows XP with the USB tablet ON still supports accurate direct tapping.
6. If the Win95 cursor does not move with swipes, check whether the guest has a working PS/2 mouse driver. Capture the Info mouse status and any D-Bus error.

Do not adjust absolute-pointer calibration for PS/2 relative input. In QEMU D-Bus, `SetAbsPosition()` requires `IsAbsolute=true`; `RelMotion()` requires `IsAbsolute=false`.
