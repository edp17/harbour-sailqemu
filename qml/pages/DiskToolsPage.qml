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

// All writes are performed by qemu-img into a staging folder. The original
// source image is never modified and an existing destination is not replaced.
Page {
    id: page
    allowedOrientations: Orientation.All
    function basename(path) {
        if (!path || !path.length) return "No image selected"
        return path.substring(path.lastIndexOf("/") + 1)
    }
    // Disk operations are asynchronous. Move the finished report into view
    // instead of leaving it unnoticed after the long configuration form.
    Connections {
        target: disks
        onOperationCompleted: diskFlick.contentY = 0
    }
    SilicaFlickable {
        id: diskFlick
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge
        VerticalScrollDecorator {}
        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium
            PageHeader { title: "Disk tools" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: "Create, inspect or independently copy a virtual disk. Existing images are never overwritten. Stop your VM before using disk tools."
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                visible: vm.running
                text: "Stop the VM first. Disk operations are disabled while a VM is running."
            }
            SectionHeader { text: "Operation" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                color: disks.busy ? Theme.highlightColor : Theme.primaryColor
                wrapMode: Text.Wrap
                text: disks.status
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                visible: disks.report.length > 0
                text: disks.report
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                wrapMode: Text.WrapAnywhere
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Cancel operation"
                visible: disks.busy
                onClicked: disks.cancel()
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Use as selected VM's primary HDD"
                visible: disks.resultPath.length > 0 && !disks.busy
                enabled: !vm.running
                onClicked: {
                    vm.diskPath = disks.resultPath
                    vm.diskFormat = disks.resultFormat
                    vm.save()
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                visible: disks.resultPath.length > 0
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: "The button above changes only the SELECTED VM's primary HDD. The original disk is not deleted. Review the VM's boot order after attaching."
            }
            SectionHeader { text: "Inspect existing image" }
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                spacing: Theme.paddingMedium
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - sourceBrowse.width - parent.spacing
                    truncationMode: TruncationMode.Fade
                    text: page.basename(disks.sourcePath)
                }
                Button {
                    id: sourceBrowse
                    text: "Browse"
                    enabled: !disks.busy && !vm.running
                    onClicked: pageStack.push(Qt.resolvedUrl("DiskSourcePickerPage.qml"))
                }
            }
            TextField {
                id: sourceManual
                width: parent.width
                label: "Image path (optional manual entry)"
                text: disks.sourcePath
                inputMethodHints: Qt.ImhNoPredictiveText
                enabled: !disks.busy && !vm.running
                onTextChanged: if (activeFocus) disks.sourcePath = text
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Inspect image"
                enabled: !vm.running && !disks.busy && disks.sourcePath.length > 0
                onClicked: disks.inspectImage(vm.qemuPath, disks.sourcePath)
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WrapAnywhere
                visible: disks.sourcePath.length > 0
                text: vm.storageReferences(disks.sourcePath)
            }
            SectionHeader { text: "Create disk" }
            TextField {
                id: createPath
                width: parent.width
                label: "New disk filename (full path)"
                text: disks.suggestedDirectory + "/new-disk.qcow2"
                inputMethodHints: Qt.ImhNoPredictiveText
                enabled: !disks.busy && !vm.running
            }
            ComboBox {
                id: createFormat
                width: parent.width
                property string selectedFormat: "qcow2"
                label: "New disk format"
                enabled: !disks.busy && !vm.running
                menu: ContextMenu {
                    MenuItem { text: "QCOW2 (sparse)"; onClicked: createFormat.selectedFormat = "qcow2" }
                    MenuItem { text: "RAW (sparse)"; onClicked: createFormat.selectedFormat = "raw" }
                }
            }
            TextField {
                id: createSize
                width: parent.width
                label: "Capacity in MiB (e.g. 4096 = 4 GiB)"
                text: "4096"
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 16; top: 131072 }
                enabled: !disks.busy && !vm.running
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Create new disk"
                enabled: !disks.busy && !vm.running && createPath.text.trim().length > 0
                         && createSize.acceptableInput
                onClicked: disks.createImage(vm.qemuPath, createPath.text,
                                             createFormat.selectedFormat, parseInt(createSize.text))
            }
            SectionHeader { text: "Copy / convert existing image" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                text: "Source: " + page.basename(disks.sourcePath)
                wrapMode: Text.WrapAnywhere
                color: Theme.secondaryColor
            }
            TextField {
                id: copyPath
                width: parent.width
                label: "Destination filename (full path)"
                text: disks.suggestedDirectory + "/disk-copy.qcow2"
                inputMethodHints: Qt.ImhNoPredictiveText
                enabled: !disks.busy && !vm.running
            }
            ComboBox {
                id: copyFormat
                width: parent.width
                property string selectedFormat: "qcow2"
                label: "Destination format"
                enabled: !disks.busy && !vm.running
                menu: ContextMenu {
                    MenuItem { text: "QCOW2 (recommended)"; onClicked: copyFormat.selectedFormat = "qcow2" }
                    MenuItem { text: "RAW"; onClicked: copyFormat.selectedFormat = "raw" }
                }
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Copy / convert"
                enabled: !disks.busy && !vm.running && disks.sourcePath.length > 0
                         && copyPath.text.trim().length > 0
                onClicked: disks.copyImage(vm.qemuPath, disks.sourcePath, copyPath.text,
                                           copyFormat.selectedFormat)
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
                text: "A converted image is independent of its source, including QCOW1/2 backing chains. Copying a multi-gigabyte disk can require substantial free space."
            }

        }
    }
}
