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

// M11r3: grouped VM configuration. File paths stay behind Browse buttons;
// existing profiles remain compatible with earlier versions of SailQEMU.
Page {
    id: page
    allowedOrientations: Orientation.All
    property bool createMode: false
    property bool manualQemuPath: false
    property bool manualMediaPaths: false
    property bool useCustomMemory: false
    property int appliedPresetIndex: 0
    property string editingProfileId: ""
    property string mediaReport: ""

    Component.onCompleted: {
        editingProfileId = page.createMode ? "" : vm.activeProfileId
        nameField.text = page.createMode ? "" : vm.activeProfileName
    }

    function hasUnselectedSecondary() {
        for (var i = 0; i < vm.secondaryDisks.length; i++) {
            if (!vm.secondaryDisks[i].path || vm.secondaryDisks[i].path.trim().length === 0)
                return true
        }
        return false
    }
    function filename(path) {
        if (!path || !path.length) return "Not selected"
        var slash = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"))
        return path.substring(slash + 1)
    }

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: form.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        Column {
            id: form
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: page.createMode ? "Add VM" : "Edit VM" }

            SectionHeader { text: "Identity" }
            TextField {
                id: nameField
                width: parent.width
                label: "VM name"
                placeholderText: "e.g. Windows XP"
                maximumLength: 80
                enabled: !vm.running
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            }
            Button {
                x: Theme.horizontalPageMargin
                visible: page.createMode
                text: "Create VM and configure"
                enabled: !vm.running && nameField.text.trim().length > 0
                onClicked: {
                    if (vm.createProfile(nameField.text)) {
                        page.createMode = false
                        page.editingProfileId = vm.activeProfileId
                        nameField.text = vm.activeProfileName
                    }
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                visible: page.createMode
                text: "Create the VM first. It receives its own settings without changing any existing VM."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }

            Column {
                id: settings
                width: parent.width
                spacing: Theme.paddingMedium
                visible: !page.createMode

                // DISKS: firmware-visible IDE slots; CD/DVD always reserves IDE index 2.
                Row {
                    x: Theme.horizontalPageMargin
                    spacing: Theme.paddingMedium
                    Icon { source: "image://theme/icon-m-file-other"; width: Theme.iconSizeMedium; height: Theme.iconSizeMedium }
                    Label { text: "Disks"; font.pixelSize: Theme.fontSizeLarge; color: Theme.highlightColor }
                }
                BackgroundItem {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    height: Theme.itemSizeMedium
                    enabled: !vm.running
                    onClicked: pageStack.push(Qt.resolvedUrl("DiskToolsPage.qml"))
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.paddingMedium
                        Icon { source: "image://theme/icon-m-file-other"; width: Theme.iconSizeMedium; height: Theme.iconSizeMedium }
                        Label { text: "Create / inspect / copy disk images"; color: Theme.highlightColor; anchors.verticalCenter: parent.verticalCenter }
                    }
                }
                SectionHeader { text: "HDD" }
                Column {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    spacing: Theme.paddingSmall
                    Label { text: "Boot / primary storage"; color: Theme.primaryColor; width: parent.width }
                    Row {
                        width: parent.width
                        spacing: Theme.paddingMedium
                        Label {
                            width: parent.width - browseHdd.width - parent.spacing
                            anchors.verticalCenter: browseHdd.verticalCenter
                            text: page.filename(vm.diskPath)
                            color: Theme.secondaryColor
                            truncationMode: TruncationMode.Fade
                        }
                        Button {
                            id: browseHdd
                            text: "Browse"
                            enabled: !vm.running
                            onClicked: pageStack.push(Qt.resolvedUrl("VmFilePickerPage.qml"))
                        }
                    }
                }
                ComboBox {
                    id: diskFormatBox
                    width: parent.width
                    enabled: !vm.running
                    label: "HDD image format"
                    currentIndex: vm.diskFormat === "qcow" ? 1 : vm.diskFormat === "qcow2" ? 2 : vm.diskFormat === "vmdk" ? 3 : vm.diskFormat === "vdi" ? 4 : 0
                    menu: ContextMenu {
                        MenuItem { text: "raw"; onClicked: vm.diskFormat = "raw" }
                        MenuItem { text: "qcow (v1)"; onClicked: vm.diskFormat = "qcow" }
                        MenuItem { text: "qcow2"; onClicked: vm.diskFormat = "qcow2" }
                        MenuItem { text: "vmdk"; onClicked: vm.diskFormat = "vmdk" }
                        MenuItem { text: "vdi"; onClicked: vm.diskFormat = "vdi" }
                    }
                }
                Repeater {
                    model: vm.secondaryDisks
                    delegate: Column {
                        width: settings.width
                        spacing: Theme.paddingSmall
                        property int diskIndex: modelData.index
                        Label {
                            x: Theme.horizontalPageMargin
                            width: parent.width - 2*x
                            text: "Secondary storage " + (diskIndex + 1)
                            color: Theme.primaryColor
                        }
                        Row {
                            x: Theme.horizontalPageMargin
                            width: parent.width - 2*x
                            spacing: Theme.paddingSmall
                            Label {
                                width: parent.width - browseExtra.width - removeExtra.width - 2*parent.spacing
                                text: page.filename(modelData.path)
                                color: Theme.secondaryColor
                                truncationMode: TruncationMode.Fade
                                anchors.verticalCenter: browseExtra.verticalCenter
                            }
                            Button {
                                id: browseExtra
                                text: "Browse"
                                enabled: !vm.running
                                onClicked: pageStack.push(Qt.resolvedUrl("VmFilePickerPage.qml"),
                                                          {selectSecondary: true, secondaryIndex: diskIndex})
                            }
                            IconButton {
                                id: removeExtra
                                icon.source: "image://theme/icon-m-remove"
                                enabled: !vm.running
                                onClicked: vm.removeSecondaryDisk(diskIndex)
                            }
                        }
                        ComboBox {
                            width: parent.width
                            enabled: !vm.running
                            label: "Secondary image format"
                            currentIndex: modelData.format === "qcow" ? 1 : modelData.format === "qcow2" ? 2 : modelData.format === "vmdk" ? 3 : modelData.format === "vdi" ? 4 : 0
                            menu: ContextMenu {
                                MenuItem { text: "raw"; onClicked: vm.setSecondaryDiskFormat(diskIndex, "raw") }
                                MenuItem { text: "qcow (v1)"; onClicked: vm.setSecondaryDiskFormat(diskIndex, "qcow") }
                                MenuItem { text: "qcow2"; onClicked: vm.setSecondaryDiskFormat(diskIndex, "qcow2") }
                                MenuItem { text: "vmdk"; onClicked: vm.setSecondaryDiskFormat(diskIndex, "vmdk") }
                                MenuItem { text: "vdi"; onClicked: vm.setSecondaryDiskFormat(diskIndex, "vdi") }
                            }
                        }
                    }
                }
                Button {
                    x: Theme.horizontalPageMargin
                    text: "+ Add secondary storage"
                    enabled: !vm.running && vm.secondaryDisks.length < 2
                    onClicked: vm.addSecondaryDisk()
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    text: "Up to two secondary IDE disks (PC-compatible slots)."
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    wrapMode: Text.Wrap
                }
                SectionHeader { text: "ISO / CD / DVD" }
                Row {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    spacing: Theme.paddingMedium
                    Label {
                        width: parent.width - browseIso.width - parent.spacing
                        text: page.filename(vm.isoPath)
                        color: Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                        anchors.verticalCenter: browseIso.verticalCenter
                    }
                    Button {
                        id: browseIso
                        text: "Browse"
                        enabled: !vm.running
                        onClicked: pageStack.push(Qt.resolvedUrl("VmFilePickerPage.qml"), {selectIso: true})
                    }
                }
                ComboBox {
                    width: parent.width
                    enabled: !vm.running
                    label: "CD/DVD image format"
                    currentIndex: vm.isoFormat === "qcow" ? 1 : vm.isoFormat === "qcow2" ? 2 : vm.isoFormat === "vmdk" ? 3 : vm.isoFormat === "vdi" ? 4 : 0
                    menu: ContextMenu {
                        MenuItem { text: "raw (ISO)"; onClicked: vm.isoFormat = "raw" }
                        MenuItem { text: "qcow (v1)"; onClicked: vm.isoFormat = "qcow" }
                        MenuItem { text: "qcow2"; onClicked: vm.isoFormat = "qcow2" }
                        MenuItem { text: "vmdk"; onClicked: vm.isoFormat = "vmdk" }
                        MenuItem { text: "vdi"; onClicked: vm.isoFormat = "vdi" }
                    }
                }
                SectionHeader { text: "Floppy" }
                Row {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    spacing: Theme.paddingMedium
                    Label {
                        width: parent.width - browseFloppy.width - parent.spacing
                        text: page.filename(vm.floppyPath)
                        color: Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                        anchors.verticalCenter: browseFloppy.verticalCenter
                    }
                    Button {
                        id: browseFloppy
                        text: "Browse"
                        enabled: !vm.running
                        onClicked: pageStack.push(Qt.resolvedUrl("VmFilePickerPage.qml"), {selectFloppy: true})
                    }
                }
                // Paths remain editable when a file picker cannot reach a location.
                BackgroundItem {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    height: Theme.itemSizeSmall
                    onClicked: page.manualMediaPaths = !page.manualMediaPaths
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.manualMediaPaths ? "Hide manual paths  ▲" : "Edit media paths manually  ▼"
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
                Column {
                    width: parent.width
                    visible: page.manualMediaPaths
                    TextField {
                        width: parent.width
                        label: "Primary HDD path"
                        text: vm.diskPath
                        inputMethodHints: Qt.ImhNoPredictiveText
                        enabled: !vm.running
                        onTextChanged: if (activeFocus) vm.diskPath = text
                    }
                    TextField {
                        width: parent.width
                        label: "CD/DVD path"
                        text: vm.isoPath
                        inputMethodHints: Qt.ImhNoPredictiveText
                        enabled: !vm.running
                        onTextChanged: if (activeFocus) vm.isoPath = text
                    }
                    TextField {
                        width: parent.width
                        label: "Floppy path"
                        text: vm.floppyPath
                        inputMethodHints: Qt.ImhNoPredictiveText
                        enabled: !vm.running
                        onTextChanged: if (activeFocus) vm.floppyPath = text
                    }
                }

                // No writes to images: show the exact paths/IDE slots and
                // declared format before starting the guest.
                BackgroundItem {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    height: Theme.itemSizeMedium
                    onClicked: page.mediaReport = page.mediaReport.length ? "" : vm.mediaConfigurationReport()
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.paddingMedium
                        Icon { source: "image://theme/icon-m-about" }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: page.mediaReport.length ? "Hide configured media" : "Inspect configured media"
                            color: Theme.highlightColor
                        }
                    }
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    visible: page.mediaReport.length > 0
                    text: page.mediaReport
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }

                Row {
                    x: Theme.horizontalPageMargin
                    spacing: Theme.paddingMedium
                    Icon { source: "image://theme/icon-m-play"; width: Theme.iconSizeMedium; height: Theme.iconSizeMedium }
                    Label { text: "Boot order"; font.pixelSize: Theme.fontSizeLarge; color: Theme.highlightColor }
                }
                ComboBox {
                    width: parent.width
                    label: "Primary boot"
                    enabled: !vm.running
                    currentIndex: vm.bootPrimary === "floppy" ? 0 : vm.bootPrimary === "cdrom" ? 1 : 2
                    menu: ContextMenu {
                        MenuItem {
                            text: "Floppy"
                            enabled: vm.floppyPath.trim().length > 0
                            onClicked: vm.bootPrimary = "floppy"
                        }
                        MenuItem {
                            text: "ISO / CD / DVD"
                            enabled: vm.isoPath.trim().length > 0
                            onClicked: vm.bootPrimary = "cdrom"
                        }
                        MenuItem { text: "HDD"; onClicked: vm.bootPrimary = "hdd" }
                    }
                }
                ComboBox {
                    width: parent.width
                    label: "Secondary boot"
                    enabled: !vm.running
                    currentIndex: vm.bootSecondary === "floppy" ? 0 : vm.bootSecondary === "cdrom" ? 1 : vm.bootSecondary === "hdd" ? 2 : 3
                    menu: ContextMenu {
                        MenuItem {
                            text: "Floppy"
                            enabled: vm.floppyPath.trim().length > 0 && vm.bootPrimary !== "floppy"
                            onClicked: vm.bootSecondary = "floppy"
                        }
                        MenuItem {
                            text: "ISO / CD / DVD"
                            enabled: vm.isoPath.trim().length > 0 && vm.bootPrimary !== "cdrom"
                            onClicked: vm.bootSecondary = "cdrom"
                        }
                        MenuItem {
                            text: "HDD"
                            enabled: vm.bootPrimary !== "hdd"
                            onClicked: vm.bootSecondary = "hdd"
                        }
                        MenuItem { text: "None"; onClicked: vm.bootSecondary = "none" }
                    }
                }

                Row {
                    x: Theme.horizontalPageMargin
                    spacing: Theme.paddingMedium
                    Icon { source: "image://theme/icon-m-display"; width: Theme.iconSizeMedium; height: Theme.iconSizeMedium }
                    Label { text: "Hardware"; font.pixelSize: Theme.fontSizeLarge; color: Theme.highlightColor }
                }
                // Compatibility presets change virtual hardware, never disk
                // attachment, boot order, emulator selection or networking.
                ComboBox {
                    id: compatibilityBox
                    width: parent.width
                    label: "Apply guest compatibility preset (optional)"
                    description: "Applies CPU, memory, VGA, ACPI and pointer recommendations. You can customise them below."
                    currentIndex: page.appliedPresetIndex
                    menu: ContextMenu {
                        MenuItem { text: "Choose preset"; onClicked: page.appliedPresetIndex = 0 }
                        MenuItem { text: "FreeDOS (BIOS PC)"; onClicked: { if (vm.applyHardwarePreset("freedos")) page.appliedPresetIndex = 1 } }
                        MenuItem { text: "Windows 95 (legacy PC)"; onClicked: { if (vm.applyHardwarePreset("win95")) page.appliedPresetIndex = 2 } }
                        MenuItem { text: "Windows XP (standard PC)"; onClicked: { if (vm.applyHardwarePreset("winxp")) page.appliedPresetIndex = 3 } }
                    }
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    visible: page.appliedPresetIndex === 2
                    text: "Windows 95 tip: disconnect a DOS boot floppy and boot from the HDD after installation. If the guest reaches C:\\>, check that Windows is installed before trying WIN."
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    wrapMode: Text.Wrap
                }
                ComboBox {
                    width: parent.width
                    label: "CPU model (x86 PC)"
                    description: "Default preserves existing guest compatibility. Legacy models can help older operating systems."
                    enabled: !vm.running
                    currentIndex: vm.cpuModel === "486" ? 1 :
                                  vm.cpuModel === "pentium" ? 2 :
                                  vm.cpuModel === "pentium2" ? 3 :
                                  vm.cpuModel === "pentium3" ? 4 :
                                  vm.cpuModel === "qemu32" ? 5 :
                                  vm.cpuModel === "qemu64" ? 6 : 0
                    menu: ContextMenu {
                        MenuItem { text: "Default (QEMU)"; onClicked: vm.cpuModel = "default" }
                        MenuItem { text: "Intel 486"; onClicked: vm.cpuModel = "486" }
                        MenuItem { text: "Pentium"; onClicked: vm.cpuModel = "pentium" }
                        MenuItem { text: "Pentium II"; onClicked: vm.cpuModel = "pentium2" }
                        MenuItem { text: "Pentium III"; onClicked: vm.cpuModel = "pentium3" }
                        MenuItem { text: "QEMU 32-bit"; onClicked: vm.cpuModel = "qemu32" }
                        MenuItem { text: "QEMU 64-bit"; onClicked: vm.cpuModel = "qemu64" }
                    }
                }
                ComboBox {
                    width: parent.width
                    label: "Virtual CPUs"
                    description: "Older guests usually work best with one. More CPUs may add overhead on a phone."
                    enabled: !vm.running
                    currentIndex: vm.cpuCount === 2 ? 1 : vm.cpuCount === 4 ? 2 : 0
                    menu: ContextMenu {
                        MenuItem { text: "1 CPU (recommended for DOS / Windows 9x)"; onClicked: vm.cpuCount = 1 }
                        MenuItem { text: "2 CPUs"; onClicked: vm.cpuCount = 2 }
                        MenuItem { text: "4 CPUs"; onClicked: vm.cpuCount = 4 }
                    }
                }
                ComboBox {
                    width: parent.width
                    label: "Video adapter"
                    description: "Standard VGA remains the default. Cirrus is available for older guests; VMware SVGA may need guest drivers."
                    enabled: !vm.running
                    currentIndex: vm.videoAdapter === "cirrus" ? 1 : vm.videoAdapter === "vmware" ? 2 : 0
                    menu: ContextMenu {
                        MenuItem { text: "Standard VGA (recommended)"; onClicked: vm.videoAdapter = "std" }
                        MenuItem { text: "Cirrus Logic VGA"; onClicked: vm.videoAdapter = "cirrus" }
                        MenuItem { text: "VMware SVGA"; onClicked: vm.videoAdapter = "vmware" }
                    }
                }
                ComboBox {
                    id: memoryBox
                    width: parent.width
                    label: "Memory (MiB)"
                    enabled: !vm.running
                    currentIndex: {
                        if (page.useCustomMemory) return 7
                        var sizes = [64,128,256,512,1024,2048,4096]
                        var i = sizes.indexOf(vm.memoryMiB)
                        return i >= 0 ? i : 7
                    }
                    menu: ContextMenu {
                        MenuItem { text: "64 MiB"; onClicked: { page.useCustomMemory = false; vm.memoryMiB = 64 } }
                        MenuItem { text: "128 MiB"; onClicked: { page.useCustomMemory = false; vm.memoryMiB = 128 } }
                        MenuItem { text: "256 MiB"; onClicked: { page.useCustomMemory = false; vm.memoryMiB = 256 } }
                        MenuItem { text: "512 MiB"; onClicked: { page.useCustomMemory = false; vm.memoryMiB = 512 } }
                        MenuItem { text: "1024 MiB (1 GiB)"; onClicked: { page.useCustomMemory = false; vm.memoryMiB = 1024 } }
                        MenuItem { text: "2048 MiB (2 GiB)"; onClicked: { page.useCustomMemory = false; vm.memoryMiB = 2048 } }
                        MenuItem { text: "4096 MiB (4 GiB)"; onClicked: { page.useCustomMemory = false; vm.memoryMiB = 4096 } }
                        MenuItem { text: "Custom"; onClicked: { page.useCustomMemory = true; customMemory.forceActiveFocus(); customMemory.selectAll() } }
                    }
                }
                TextField {
                    id: customMemory
                    width: parent.width
                    visible: memoryBox.currentIndex === 7
                    label: "Custom memory (MiB)"
                    placeholderText: "16–32768 (whole numbers)"
                    enabled: !vm.running
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 16; top: 32768 }
                    text: vm.memoryMiB.toString()
                    EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                    onTextChanged: {
                        if (activeFocus && acceptableInput)
                            vm.memoryMiB = parseInt(text, 10)
                    }
                }
                TextSwitch {
                    text: "Enable ACPI"
                    description: "Turn off for older guests such as Windows 95. ACPI-off guests may ignore the ACPI shutdown action."
                    enabled: !vm.running
                    automaticCheck: false
                    checked: vm.acpiEnabled
                    onClicked: if (!vm.running) vm.acpiEnabled = !vm.acpiEnabled
                }
                TextSwitch {
                    text: "USB tablet (absolute pointer)"
                    description: "Recommended for Windows XP. Turn off for a legacy PS/2 mouse in Windows 95."
                    enabled: !vm.running
                    automaticCheck: false
                    checked: vm.usbTabletEnabled
                    onClicked: if (!vm.running) vm.usbTabletEnabled = !vm.usbTabletEnabled
                }
                TextSwitch {
                    text: "Enable guest networking"
                    description: "Requires QEMU with libslirp (user-mode NAT)"
                    enabled: !vm.running
                    automaticCheck: false
                    checked: vm.networkEnabled
                    onClicked: if (!vm.running) vm.networkEnabled = !vm.networkEnabled
                }

                Row {
                    x: Theme.horizontalPageMargin
                    spacing: Theme.paddingMedium
                    Icon { source: "image://theme/icon-m-about"; width: Theme.iconSizeMedium; height: Theme.iconSizeMedium }
                    Label { text: "Miscellaneous"; font.pixelSize: Theme.fontSizeLarge; color: Theme.highlightColor }
                }
                TextSwitch {
                    text: "Start SDL fullscreen"
                    description: "Legacy SDL modes only; the normal D-Bus Guest console uses the Sailfish page instead."
                    enabled: !vm.running
                    automaticCheck: false
                    checked: vm.fullscreen
                    onClicked: if (!vm.running) vm.fullscreen = !vm.fullscreen
                }
                BackgroundItem {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    height: Theme.itemSizeSmall
                    onClicked: page.manualQemuPath = !page.manualQemuPath
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "QEMU: " + page.filename(vm.qemuPath) + (page.manualQemuPath ? "  ▲" : "  ▼")
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
                TextField {
                    width: parent.width
                    visible: page.manualQemuPath
                    label: "QEMU executable"
                    text: vm.qemuPath
                    enabled: !vm.running
                    inputMethodHints: Qt.ImhNoPredictiveText
                    onTextChanged: if (activeFocus) vm.qemuPath = text
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    text: "Select an image for each secondary HDD, or remove the empty slot, before saving."
                    visible: page.hasUnselectedSecondary()
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Save VM"
                    enabled: !vm.running && nameField.text.trim().length > 0 && !page.hasUnselectedSecondary()
                    onClicked: {
                        if (vm.saveProfileEdits(page.editingProfileId, nameField.text))
                            pageStack.pop()
                    }
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                visible: vm.lastError.length > 0
                text: vm.lastError
                wrapMode: Text.Wrap
                color: Theme.highlightColor
            }
        }
    }
}
