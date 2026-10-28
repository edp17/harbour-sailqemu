/*
    Copyright (C) 2026 edp17 and chatGPT

    This file is part of harbour-sailqemu.

    The harbour-sailqemu is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The harbour-sailqemu is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the harbour-sailqemu. If not, see <http://www.gnu.org/licenses/>.
*/
import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: guideContent.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        Column {
            id: guideContent
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: "User guide" }

            SectionHeader { text: "First run: QEMU installation" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "SailQEMU is a frontend, not the emulator itself. Install a compatible QEMU 10.2.4 package separately. The standard package normally lives under /opt/sailqemu/qemu-10.2.4/; the optional network-enabled build lives under /opt/sailqemu/qemu-10.2.4-net/. In Manage VMs, Edit a profile and check its QEMU executable path before starting a guest. Enable NAT only with a QEMU build that includes libslirp. Use Manage VMs > Diagnostics for read-only runtime checks and the recent QEMU log."
            }

            SectionHeader { text: "Start a virtual machine" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "Choose a VM from the Main page and tap Start VM for a live D-Bus guest display. If you need to add or edit one, open Manage VMs from the Main page pulley menu; long-press a VM and select Edit. Start VM boots immediately; Pause/Resume is for manually pausing a running VM. Expand Other start modes to use Start (SDL with console controls) or Start SDL only (legacy separate window)."
            }

            SectionHeader { text: "Runtime diagnostics" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "From Main, open Manage VMs and choose Diagnostics from its pulley menu. While a VM runs it measures QEMU and SailQEMU process CPU/RAM, the observed D-Bus framebuffer update rate, guest resolution, and checks for installed QEMU tools. Guest console > Info also links to Diagnostics. An update rate is not the actual Qt Quick rendered FPS; use these measurements to compare guests before changing display settings."
            }

            SectionHeader { text: "Return to the console" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "Back navigation from the Guest console does not stop the VM. Tap Return to running VM at the top of the Main page to reopen it."
            }

            SectionHeader { text: "Touch and keyboard" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "In landscape the four controls across the top are Keyboard, Pause/Resume, ACPI off and Info (i). The next row has Left click/tap (mouse L), Drag (hold left button and move) and Pan (move the zoomed view without clicking). The row after that has Right click (mouse R), Wheel up and Wheel down. Below those are Double click (2×, arm then tap a target) and Orientation lock (hold the current rotation). Tap-and-hold on the guest display also sends a right-click. Tap i for display diagnostics, cursor visibility and pointer calibration. The + / − controls zoom without changing guest resolution."
            }

            SectionHeader { text: "Mouse control: absolute vs PS/2" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "Windows XP with a supported USB tablet uses absolute input: tap directly on a button. Older guests such as Windows 95 usually use a relative PS/2 mouse. Disable 'USB tablet absolute pointer' in that VM's Hardware settings and RESTART it. In relative mode SailQEMU acts like a laptop touchpad: swipe to move the Windows cursor, then tap anywhere in the guest view to click at the cursor's current position. Drag holds the left button while you swipe; the R icon right-clicks at the cursor. The cursor cannot automatically jump underneath your fingertip without a guest driver supporting absolute positioning."
            }

            SectionHeader { text: "Controls — illustrated reference" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                text: "The icons below match the Guest console. Mouse actions require the D-Bus display and a guest pointing device."
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
            }
            Repeater {
                model: [
                    { iconName: "icon-m-keyboard", badge: "", description: "Keyboard — show or hide text entry and special keys." },
                    { iconName: "icon-m-pause", badge: "", description: "Pause / Resume — pause guest execution or continue." },
                    { iconName: "icon-m-stop", badge: "", description: "ACPI off — ask the guest to shut down cleanly; some DOS guests ignore it." },
                    { iconName: "icon-m-about", badge: "", description: "Information — connection status, cursor options, resync and calibration." },
                    { iconName: "icon-m-mouse", badge: "L", description: "Left click — in absolute mode tap the target; in relative PS/2 mode tap to click at the current pointer. Hold for right-click." },
                    { iconName: "icon-m-gesture", badge: "", description: "Drag — touch and move with the left button held; relative PS/2 mode moves the current pointer." },
                    { iconName: "icon-m-swipe", badge: "", description: "Pan — move around a magnified screen without clicking in the guest." },
                    { iconName: "icon-m-mouse", badge: "R", description: "Right click — send one right-click at the current guest cursor." },
                    { iconName: "icon-m-up", badge: "", description: "Wheel up — scroll the guest window upward." },
                    { iconName: "icon-m-down", badge: "", description: "Wheel down — scroll the guest window downward." },
                    { iconName: "icon-m-mouse", badge: "2×", description: "Double click — arm two clicks at the next touch (absolute mode: touch target; PS/2: current pointer)." },
                    { iconName: "icon-m-display", badge: "", description: "Orientation lock — lock the Guest console to its CURRENT portrait/landscape orientation; tap again to unlock. A locked screen shows a padlock." },
                    { iconName: "icon-m-remove", badge: "", description: "Zoom out — reduce magnification by one small step." },
                    { iconName: "icon-m-scale", badge: "", description: "Fit — reset zoom and pan to fit the guest display." },
                    { iconName: "icon-m-add", badge: "", description: "Zoom in — enlarge the guest display by one small step." },
                    { iconName: "icon-m-refresh", badge: "", description: "Resync — reconnect the D-Bus display listener, without restarting the VM." }
                ]
                delegate: Row {
                    x: Theme.horizontalPageMargin
                    width: guideContent.width - 2 * x
                    spacing: Theme.paddingMedium
                    height: Math.max(Theme.itemSizeSmall,
                                     actionMeaning.paintedHeight + 2 * Theme.paddingSmall)
                    Item {
                        width: Theme.itemSizeSmall
                        height: Theme.itemSizeSmall
                        Icon {
                            anchors.centerIn: parent
                            width: Theme.iconSizeMedium
                            height: Theme.iconSizeMedium
                            source: "image://theme/" + modelData.iconName
                        }
                        Label {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: Theme.paddingSmall / 2
                            visible: modelData.badge !== ""
                            text: modelData.badge
                            font.bold: true
                            font.pixelSize: Math.max(12, Theme.fontSizeExtraSmall * 0.70)
                        }
                    }
                    Label {
                        id: actionMeaning
                        width: parent.width - Theme.itemSizeSmall - parent.spacing
                        text: modelData.description
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.primaryColor
                        wrapMode: Text.Wrap
                    }
                }
            }

            SectionHeader { text: "Use landscape and diagnostics" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "In landscape the guest stays on the left with a vertical zoom/fit/refresh strip; the right control pane scrolls independently. The Info (i) icon expands connection details, cursor coordinates, pointer alignment and D-Bus resynchronisation. Resync reconnects the display listener without restarting the VM. In portrait the compact top toolbar contains Keyboard, Pause/Resume, ACPI off and Info."
            }

            SectionHeader { text: "Manage disk images" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "Open Disk tools from Manage VMs' pulley menu or from Edit VM. Inspect a disk using qemu-img, create sparse RAW/QCOW2 disks, or copy/convert an existing disk into an independent image (including legacy QCOW1). Image operations are disabled when a SailQEMU VM runs. An existing destination is never overwritten. A VM settings duplicate shares the SAME writable HDD: use Copy/convert first to give its clone independent storage. The resulting image may be attached explicitly to the selected VM. Keep source files and disk-image backups when experimenting."
            }

            SectionHeader { text: "Configure disk and boot media" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "On Manage VMs, open Edit to set storage and hardware. The Disks section lets you browse for a boot HDD and add up to two secondary HDDs; set each image format to raw, qcow (v1), qcow2, vmdk or vdi as appropriate. CD/DVD images normally use raw (ISO) and are mounted read-only. Floppy A: is also read-only. The Boot order section sets primary and secondary boot devices, without duplicates. Absent media cannot be selected. In Hardware, choose a memory preset or Custom to enter a whole number of MiB; The CPU model selector offers QEMU default and several legacy x86 models; virtual CPU count offers 1, 2 or 4 (use one for DOS/Windows 9x); and the video adapter defaults to Standard VGA. The emulator remains x86_64, irrespective of the selected emulated CPU model. Miscellaneous has the SDL-only fullscreen option. Full path fields stay hidden unless you expand manual editing."
            }

            SectionHeader { text: "Choose guest compatibility settings" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "In Manage VMs → Edit VM → Hardware, optionally apply a FreeDOS, Windows 95 or Windows XP hardware preset. Presets only adjust CPU model, virtual CPUs, RAM, VGA, ACPI and USB tablet; they do not replace the VM disk, change boot order or change networking. Windows XP: start with Default (QEMU), 1 CPU, 512 MiB RAM, Standard VGA, ACPI and USB tablet enabled. Windows 95: try Pentium, 1 CPU, 128 MiB, Cirrus VGA, ACPI disabled and PS/2 mouse (USB tablet disabled). Older guests may not implement ACPI shutdown."
            }
            SectionHeader { text: "Troubleshoot a Windows 95 DOS prompt" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "A DOS prompt alone does not prove a Windows 95 graphics failure. Check whether the VM booted the floppy instead of its installed hard disk; after installation set HDD as primary boot. At the appropriate DOS prompt check VER and DIR C:\\WINDOWS. If Windows 95 is installed, try C:\\WINDOWS\\WIN (or WIN from its Windows directory). If WIN.COM is absent, the image may contain DOS or Windows installation files rather than an installed Windows system. A guest that never initialises its VGA adapter may leave QEMU's initial display message in place; Resync cannot fix an uninitialised guest video driver."
            }

            SectionHeader { text: "Shut down and connect to a network" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "ACPI off requests a graceful guest shutdown. DOS and other guests without ACPI shutdown support may ignore it. Force off terminates QEMU after a cancellable warning and can lose unsaved guest data. Guest NAT networking requires QEMU compiled with libslirp. The original QEMU 10.2.4 RPM has no libslirp support; leave networking off with that executable. A separately built network-enabled QEMU RPM can support NAT once selected in the VM profile."
            }

            SectionHeader { text: "Troubleshoot the display" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                text: "A guest can switch video modes during startup. The D-Bus listener normally updates automatically; use Resync under Details if it stops updating. Some guest firmware shows a placeholder before the guest initialises video. If the guest draws no new pixels, SailQEMU cannot replace the placeholder. The old QMP screenshot preview is retained as a fallback."
            }
        }
    }
}
