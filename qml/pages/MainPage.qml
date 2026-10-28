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

// Landing page: choose and launch a VM. Editing lives under Manage VMs.
Page {
    id: page
    allowedOrientations: Orientation.All
    property bool showOtherModes: false

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        PullDownMenu {
            MenuItem {
                text: "Manage VMs"
                onClicked: pageStack.push(Qt.resolvedUrl("ProfilesPage.qml"))
            }
            MenuItem {
                text: "User guide"
                onClicked: pageStack.push(Qt.resolvedUrl("SailQemuGuidePage.qml"))
            }
            MenuItem {
                text: "About"
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
        }

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: "SailQEMU" }

            // The active guest is the most important item when coming back
            // from the console. Avoid making users hunt for it lower down.
            BackgroundItem {
                id: runningCard
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                height: runningContent.height + 2*Theme.paddingMedium
                visible: vm.running
                onClicked: {
                    if (vm.connected)
                        pageStack.push(Qt.resolvedUrl("GuestKeyboardPage.qml"))
                }
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.paddingMedium
                    color: Theme.rgba(Theme.highlightBackgroundColor, 0.22)
                    opacity: 0.7
                }
                Row {
                    id: runningContent
                    x: Theme.paddingMedium
                    y: Theme.paddingMedium
                    width: parent.width - 2*x
                    spacing: Theme.paddingMedium
                    Icon {
                        source: "image://theme/icon-m-play"
                        width: Theme.iconSizeMedium
                        height: Theme.iconSizeMedium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Column {
                        width: parent.width - Theme.iconSizeMedium - 2*Theme.paddingMedium - Theme.iconSizeSmall
                        spacing: Theme.paddingSmall
                        Label {
                            width: parent.width
                            text: "Return to running VM"
                            font.pixelSize: Theme.fontSizeMedium
                            font.bold: true
                            color: Theme.highlightColor
                            truncationMode: TruncationMode.Fade
                        }
                        Label {
                            width: parent.width
                            text: vm.activeProfileName + "  ·  " + (vm.paused ? "Paused" : "Running")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.primaryColor
                            truncationMode: TruncationMode.Fade
                        }
                    }
                    Icon {
                        source: "image://theme/icon-m-right"
                        width: Theme.iconSizeSmall
                        height: Theme.iconSizeSmall
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            SectionHeader { text: "Virtual machine" }
            ComboBox {
                id: vmSelector
                width: parent.width
                label: "Select VM"
                enabled: !vm.running
                function selectedIndex() {
                    var entries = vm.profiles
                    for (var i = 0; i < entries.length; ++i) {
                        if (entries[i].id === vm.activeProfileId) return i
                    }
                    return -1
                }
                Component.onCompleted: currentIndex = selectedIndex()
                menu: ContextMenu {
                    Repeater {
                        model: vm.profiles
                        delegate: MenuItem {
                            text: modelData.name
                            onClicked: vm.selectProfile(modelData.id)
                        }
                    }
                }
            }
            Connections {
                target: vm
                onProfilesChanged: vmSelector.currentIndex = vmSelector.selectedIndex()
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                text: vm.running ? vm.status
                    : (vm.status.indexOf("QEMU exited") === 0 ? vm.status
                       : "Ready to start " + vm.activeProfileName)
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }
            Button {
                id: startButton
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                text: "Start VM"
                visible: !vm.running
                enabled: vm.profileCount > 0
                onClicked: {
                    if (vm.startVmWithDbusConsole())
                        pageStack.push(Qt.resolvedUrl("GuestKeyboardPage.qml"))
                }
            }

            // Expand/collapse row uses a rotating chevron, not another large
            // button; the legacy launch alternatives stay visually secondary.
            BackgroundItem {
                id: otherModesToggle
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                height: Theme.itemSizeSmall
                visible: !vm.running
                onClicked: page.showOtherModes = !page.showOtherModes
                Row {
                    anchors.centerIn: parent
                    spacing: Theme.paddingMedium
                    Label {
                        text: "Other start modes"
                        color: Theme.secondaryColor
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    Icon {
                        source: "image://theme/icon-m-down"
                        width: Theme.iconSizeSmall
                        height: Theme.iconSizeSmall
                        rotation: page.showOtherModes ? 180 : 0
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on rotation { NumberAnimation { duration: 180 } }
                    }
                }
            }
            Column {
                id: otherModes
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                spacing: Theme.paddingSmall
                visible: page.showOtherModes && !vm.running
                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium
                    Button {
                        width: (parent.width - parent.spacing) / 2
                        text: "Start"
                        onClicked: {
                            if (vm.startVmWithConsole())
                                pageStack.push(Qt.resolvedUrl("GuestKeyboardPage.qml"))
                        }
                    }
                    Button {
                        width: (parent.width - parent.spacing) / 2
                        text: "Start SDL only"
                        onClicked: vm.startVm()
                    }
                }
                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium
                    Label {
                        width: (parent.width - parent.spacing) / 2
                        text: "SDL window + guest controls"
                        wrapMode: Text.Wrap
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                    Label {
                        width: (parent.width - parent.spacing) / 2
                        text: "Legacy: separate SDL window"
                        wrapMode: Text.Wrap
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }
            }

            // VM power controls remain accessible without entering a guest.
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                visible: vm.running
                spacing: Theme.paddingMedium
                Button {
                    width: (parent.width - 2*parent.spacing)/3
                    text: vm.paused ? "Resume" : "Pause"
                    enabled: vm.connected
                    onClicked: vm.paused ? vm.resumeVm() : vm.pauseVm()
                }
                Button {
                    width: (parent.width - 2*parent.spacing)/3
                    text: "ACPI off"
                    enabled: vm.connected && !vm.shutdownPending
                    onClicked: vm.requestShutdown()
                }
                Button {
                    width: (parent.width - 2*parent.spacing)/3
                    text: "Force off"
                    enabled: vm.connected
                    onClicked: Remorse.popupAction(page, "Powering off virtual machine", function() {
                        if (vm.running && vm.connected) vm.forceQuit()
                    }, 4000)
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                visible: vm.shutdownPending
                text: vm.shutdownStatus
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
                wrapMode: Text.Wrap
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                visible: vm.lastError.length > 0
                text: vm.lastError
                color: Theme.highlightColor
                wrapMode: Text.Wrap
            }
        }
    }
}
