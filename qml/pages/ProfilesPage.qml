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

// VM library and CRUD actions; MainPage never edits profile settings.
Page {
    id: page
    allowedOrientations: Orientation.All
    property string pendingEditId: ""

    function editProfile(id) {
        // Selecting the profile rebuilds this Repeater. Do it *after* the
        // context-menu click has finished, from this persistent Page instead
        // of the soon-to-be-destroyed ListItem delegate.
        pendingEditId = id
        delayedEdit.restart()
    }
    Timer {
        id: delayedEdit
        interval: 120
        repeat: false
        onTriggered: {
            var id = page.pendingEditId
            if (id.length && vm.selectProfile(id))
                pageStack.push(Qt.resolvedUrl("VmEditPage.qml"), {createMode: false})
            // Keep the pending marker through the click dispatch that closes
            // the ContextMenu; clear it on a separate event-loop tick.
            clearEdit.start()
        }
    }
    Timer {
        id: clearEdit
        interval: 150
        repeat: false
        onTriggered: page.pendingEditId = ""
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge
        VerticalScrollDecorator {}
        PullDownMenu {
            MenuItem {
                text: "Add new"
                enabled: !vm.running
                onClicked: pageStack.push(Qt.resolvedUrl("VmEditPage.qml"), {createMode: true})
            }
            MenuItem {
                text: "Disk tools"
                onClicked: pageStack.push(Qt.resolvedUrl("DiskToolsPage.qml"))
            }
            MenuItem {
                text: "Diagnostics"
                onClicked: pageStack.push(Qt.resolvedUrl("DiagnosticsPage.qml"))
            }
        }
        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingSmall
            PageHeader { title: "Manage VMs" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                text: vm.running
                      ? "Stop the running VM before changing its configuration."
                      : "Tap a VM to select it; press and hold to edit, copy or delete it. Disk tools and Diagnostics are in this page's pulley menu."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }
            SectionHeader { text: "Saved virtual machines (" + vm.profileCount + ")" }
            Repeater {
                model: vm.profiles
                delegate: ListItem {
                    id: item
                    width: content.width
                    contentHeight: Theme.itemSizeLarge
                    property string profileId: modelData.id
                    property string profileName: modelData.name
                    enabled: !vm.running
                    onClicked: {
                        if (!page.pendingEditId.length && vm.selectProfile(profileId))
                            pageStack.pop()
                    }
                    Icon {
                        id: vmIcon
                        x: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        source: "image://theme/icon-m-display"
                        width: Theme.iconSizeMedium
                        height: Theme.iconSizeMedium
                        color: modelData.selected ? Theme.highlightColor : Theme.primaryColor
                    }
                    Column {
                        x: vmIcon.x + vmIcon.width + Theme.paddingMedium
                        width: item.width - x - Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.paddingSmall
                        Label {
                            width: parent.width
                            text: profileName
                            font.bold: modelData.selected
                            color: modelData.selected ? Theme.highlightColor : Theme.primaryColor
                            truncationMode: TruncationMode.Fade
                        }
                        Label {
                            width: parent.width
                            text: modelData.selected && vm.running ? (vm.paused ? "Paused" : "Running") :
                                  (modelData.selected ? "Selected" : "Ready")
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: modelData.selected ? Theme.highlightColor : Theme.secondaryColor
                        }
                    }
                    menu: ContextMenu {
                        MenuItem {
                            text: "Edit"
                            onClicked: page.editProfile(item.profileId)
                        }
                        MenuItem {
                            text: "Duplicate"
                            onClicked: vm.duplicateProfile(item.profileId)
                        }
                        MenuItem {
                            text: "Delete"
                            enabled: vm.profileCount > 1
                            onClicked: {
                                var id = item.profileId
                                item.remorseDelete(function() { vm.deleteProfile(id) }, 4000)
                            }
                        }
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
