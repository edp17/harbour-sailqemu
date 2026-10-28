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
        contentHeight: aboutContent.height + Theme.paddingLarge
        VerticalScrollDecorator {}
        Column {
            id: aboutContent
            width: parent.width
            spacing: Theme.paddingMedium
            PageHeader { title: "About" }
            Item {
                width: parent.width
                height: Theme.itemSizeHuge + Theme.paddingLarge
                Image {
                    anchors.centerIn: parent
                    source: "file:///usr/share/icons/hicolor/86x86/apps/harbour-sailqemu.png"
                    width: Theme.iconSizeLarge
                    height: Theme.iconSizeLarge
                    fillMode: Image.PreserveAspectFit
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                text: "SailQEMU"
                font.pixelSize: Theme.fontSizeHuge
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                text: "Version 0.5.0-4"
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
            }
            SectionHeader { text: "About SailQEMU" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                text: "SailQEMU is a native Sailfish OS frontend for running and managing virtual machines with QEMU. It provides VM profiles, an integrated D-Bus display, and touch, mouse and keyboard controls."
                wrapMode: Text.Wrap
            }
            SectionHeader { text: "Project" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                text: "SailQEMU — an independent open-source frontend project."
                wrapMode: Text.Wrap
            }
            Button {
                x: Theme.horizontalPageMargin
                text: "Source code"
                onClicked: Qt.openUrlExternally("https://github.com/edp17/harbour-sailqemu")
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                text: "SailQEMU source: MIT license. QEMU and libslirp are separate upstream projects with their own licenses; they are not bundled with the frontend."
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }
        }
    }
}
