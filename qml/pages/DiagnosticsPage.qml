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
        contentHeight: content.height + Theme.paddingLarge
        VerticalScrollDecorator {}
        PullDownMenu {
            MenuItem {
                text: "Refresh now"
                onClicked: diagnostics.refresh()
            }
            MenuItem {
                text: "Copy report"
                onClicked: {
                    diagnostics.refresh()
                    diagnostics.copyReport()
                }
            }
        }
        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium
            PageHeader { title: "Diagnostics" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: "Read-only measurements and recent QEMU log. Pull down to copy the report or refresh it manually when investigating startup failures. Open while a guest is running to compare its CPU use with the frontend's CPU use. Values update every 2.5 seconds; display updates are not actual rendered FPS."
            }
            SectionHeader { text: "Performance and runtime" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.WrapAnywhere
                font.pixelSize: Theme.fontSizeSmall
                text: diagnostics.report
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: "The CPU percentages are process CPU time, normalised to one core; they are not a measure of guest performance, screen-present rate, or total system CPU load. Diagnostic sampling never changes VM hardware or guest storage."
            }
        }
    }
}
