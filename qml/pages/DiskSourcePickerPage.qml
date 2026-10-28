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
import Sailfish.Pickers 1.0

FilePickerPage {
    id: picker
    title: "Select existing disk image"
    allowedOrientations: Orientation.All
    property bool selectionHandled: false
    onSelectedContentPropertiesChanged: {
        if (selectionHandled || vm.running || disks.busy || !selectedContentProperties) return
        var path = selectedContentProperties.filePath
        if (!path || !path.length) return
        selectionHandled = true
        disks.sourcePath = path
    }
}
