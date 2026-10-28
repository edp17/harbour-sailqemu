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

// Loaded on demand: the picker module does not affect SailQEMU startup.
FilePickerPage {
    id: picker
    property bool selectIso: false
    property bool selectFloppy: false
    property bool selectSecondary: false
    property int secondaryIndex: -1
    title: selectFloppy ? "Choose floppy image" : (selectIso ? "Choose boot ISO" : (selectSecondary ? "Choose secondary HDD" : "Choose disk image"))
    allowedOrientations: Orientation.All
    property bool selectionHandled: false

    onSelectedContentPropertiesChanged: {
        if (selectionHandled || vm.running) return
        if (!selectedContentProperties) return
        var file = selectedContentProperties.filePath
        if (!file || file.length === 0) return
        selectionHandled = true
        var selected = false
        if (selectFloppy) {
            vm.floppyPath = file
            selected = true
        } else if (selectIso) {
            vm.isoPath = file
            // A freshly selected .iso is a raw optical image even if the
            // previous virtual CD used a QCOW2/VMDK/VDI container.
            if (/\.iso$/i.test(file)) vm.isoFormat = "raw"
            selected = true
        } else if (selectSecondary) {
            selected = vm.chooseSecondaryDiskImage(secondaryIndex, file)
        } else {
            selected = vm.chooseDiskImage(file)
        }
        if (selected) vm.save()
    }
}
