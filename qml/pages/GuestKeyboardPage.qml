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
    // The lock affects this Guest console page only, and holds the current
    // Silica orientation (including landscape-inverted where supported).
    allowedOrientations: page.orientationLocked ? page.lockedOrientation : Orientation.All
    // M12r2: auto-return only after the QEMU process has genuinely exited.
    // An ACPI power-button request and a QMP STOP/paused state are NOT exits.
    property bool sawRunningVm: vm.running
    property bool returningAfterStop: false
    function checkVmExit() {
        if (vm.running) {
            sawRunningVm = true
        } else if (sawRunningVm && !returningAfterStop) {
            returningAfterStop = true
            returnToMainTimer.start()
        }
    }
    Connections {
        target: vm
        onStateChanged: page.checkVmExit()
    }
    Timer {
        id: returnToMainTimer
        interval: 150
        repeat: false
        onTriggered: {
            if (!vm.running && pageStack.currentPage === page)
                pageStack.pop()
        }
    }
    property bool orientationLocked: false
    property int lockedOrientation: Orientation.Portrait
    function toggleOrientationLock() {
        if (orientationLocked) {
            orientationLocked = false
        } else {
            lockedOrientation = page.orientation
            orientationLocked = true
        }
    }
    // Sailfish excludes a landscape camera cutout from Page geometry.
    // In portrait the Silica PageHeader supplies notch-aware top padding.
    cutoutMode: CutoutMode.AvoidLandscapeCutout
    // M9r2: the framebuffer has ONE permanent parent (displayHost).
    // Moving it between a scrolling Column and a landscape host caused
    // different portrait layouts depending on previous orientation.
    readonly property bool landscapeLayout: width > height
    readonly property real sideWidth: Math.min(460, Math.max(340, width * 0.26))
    onLandscapeLayoutChanged: {
        releaseGuestMouse()
        if (controlsPane) controlsPane.contentY = 0
    }

    // M6 uses D-Bus scanouts in experimental mode. When no frames are
    // available, the QMP screenshot path remains a tested fallback.
    property real zoomFactor: 1.0
    // 15% zoom steps allow fine adjustments on small phone screens.
    readonly property real zoomStep: 1.15
    function zoomIn() { zoomFactor = Math.min(4.0, zoomFactor * zoomStep) }
    function zoomOut() { zoomFactor = Math.max(1.0, zoomFactor / zoomStep) }
    function zoomFit() { zoomFactor = 1.0; screenPan.contentX = 0; screenPan.contentY = 0 }
    // Tap to click, Drag for click-and-hold movement, Pan for inspecting the
    // guest screen without sending input. The QML mode never changes QEMU.
    // "double" is a one-shot left double-click at the next touched position.
    // Timer separates the two D-Bus clicks while staying inside XP's default
    // double-click interval. After the first click we revert to normal Tap.
    property string mouseMode: "tap"
    property int doubleClickX: -1
    property int doubleClickY: -1
    Timer {
        id: secondClickTimer
        interval: 110
        repeat: false
        onTriggered: {
            if (page.doubleClickX >= 0 && page.doubleClickY >= 0 &&
                vm.dbusMode && vm.running && vm.connected && !vm.paused &&
                vm.mouseAvailable)
                vm.guestMouseClick(page.doubleClickX, page.doubleClickY, "left")
            page.doubleClickX = -1
            page.doubleClickY = -1
        }
    }
    property bool mouseDragging: false
    property bool mouseHeldRight: false
    property int pendingMouseX: 0
    property int pendingMouseY: 0
    property bool mouseMotionPending: false
    // Legacy PS/2 guests (e.g. Windows 95) have no screen-coordinate mouse.
    // The console behaves like a laptop touchpad in relative mode: swipe to
    // move, tap the pad to click at the current guest cursor location.
    property bool relativeFingerDown: false
    property real relativePendingX: 0
    property real relativePendingY: 0
    readonly property real relativeSpeed: 1.0

    function flushRelativeMotion() {
        if (!vm.mouseAvailable || vm.mouseAbsolute) {
            relativePendingX = 0
            relativePendingY = 0
            return
        }
        // Retain fractional movement across refreshes, avoiding dead zones
        // on scaled-down screens and avoiding lost fast finger motion.
        var dx = Math.max(-512, Math.min(512, Math.round(relativePendingX)))
        var dy = Math.max(-512, Math.min(512, Math.round(relativePendingY)))
        if ((dx !== 0 || dy !== 0) && vm.guestMouseRelative(dx, dy)) {
            relativePendingX -= dx
            relativePendingY -= dy
        }
    }

    // M8r2: screenImage and MouseArea share image-local coordinates.
    // Rendered PNG pixels need not equal QEMU's current D-Bus console size
    // during a guest resolution change. SetAbsPosition expects console pixels.
    // Both touch input and cursor overlay use the SAME guest-to-image geometry.
    property bool keyboardExpanded: false
    property bool showDiagnostics: false
    property bool pointerTuning: false
    property bool showCursorOverlay: true
    property real touchMarkX: 0
    property real touchMarkY: 0
    property bool touchMarkVisible: false
    property int lastGuestX: -1
    property int lastGuestY: -1
    readonly property int inputWidth: vm.mouseConsoleWidth > 0
                                     ? vm.mouseConsoleWidth : screenImage.sourceSize.width
    readonly property int inputHeight: vm.mouseConsoleHeight > 0
                                      ? vm.mouseConsoleHeight : screenImage.sourceSize.height

    function guestX(px) {
        if (page.inputWidth <= 0 || screenImage.width <= 0) return 0
        var pos = Math.floor(px * page.inputWidth / screenImage.width)
        return Math.max(0, Math.min(page.inputWidth - 1,
                       pos + (vm.mouseAbsolute ? vm.mouseOffsetX : 0)))
    }
    function guestY(py) {
        if (page.inputHeight <= 0 || screenImage.height <= 0) return 0
        var pos = Math.floor(py * page.inputHeight / screenImage.height)
        return Math.max(0, Math.min(page.inputHeight - 1,
                       pos + (vm.mouseAbsolute ? vm.mouseOffsetY : 0)))
    }
    function cursorDisplayX(guestX) {
        return page.inputWidth > 0 ? guestX * screenImage.width / page.inputWidth : 0
    }
    function cursorDisplayY(guestY) {
        return page.inputHeight > 0 ? guestY * screenImage.height / page.inputHeight : 0
    }
    function showTouchMark(px, py) {
        page.touchMarkX = px
        page.touchMarkY = py
        page.lastGuestX = page.guestX(px)
        page.lastGuestY = page.guestY(py)
        page.touchMarkVisible = true
        touchMarkerTimer.restart()
    }
    Timer {
        id: touchMarkerTimer
        interval: 800
        repeat: false
        onTriggered: page.touchMarkVisible = false
    }
    function releaseGuestMouse() {
        if (page.relativeFingerDown) page.flushRelativeMotion()
        page.relativeFingerDown = false
        page.relativePendingX = 0
        page.relativePendingY = 0
        if (page.mouseDragging) {
            vm.guestMouseButton("left", false)
            page.mouseDragging = false
            page.mouseMotionPending = false
        }
    }
    Component.onDestruction: releaseGuestMouse()

    Timer {
        id: mouseMotionTimer
        interval: 40
        repeat: true
        running: page.mouseDragging || page.relativeFingerDown
        onTriggered: {
            if (page.relativeFingerDown && !vm.mouseAbsolute) {
                page.flushRelativeMotion()
            } else if (page.mouseMotionPending && vm.mouseAvailable) {
                vm.guestMouseMove(page.pendingMouseX, page.pendingMouseY)
                page.mouseMotionPending = false
            }
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: page.status === PageStatus.Active && vm.running && vm.connected
        onTriggered: vm.refreshGuestPreview()
    }

    // PageHeader accounts for portrait camera cutouts unlike a raw Item at y=0.
    // Keep a normal title, without placing icon buttons in the notch area.
    PageHeader {
        id: safePortraitHeader
        visible: !page.landscapeLayout
        width: page.width
        title: "Guest console"
    }

    // Compact action strip immediately below the notch-aware title.
    Item {
        id: portraitToolbar
        visible: !page.landscapeLayout
        x: 0
        y: page.landscapeLayout ? 0 : safePortraitHeader.height
        width: page.width
        height: page.landscapeLayout ? 0 : Theme.itemSizeMedium
        Row {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingSmall
            anchors.rightMargin: Theme.paddingSmall
            IconButton {
                width: parent.width / 4
                icon.source: "image://theme/icon-m-keyboard"
                highlighted: page.keyboardExpanded
                onClicked: page.keyboardExpanded = !page.keyboardExpanded
            }
            IconButton {
                width: parent.width / 4
                icon.source: vm.paused ? "image://theme/icon-m-play" : "image://theme/icon-m-pause"
                enabled: vm.running && vm.connected
                onClicked: vm.paused ? vm.resumeVm() : vm.pauseVm()
            }
            IconButton {
                width: parent.width / 4
                icon.source: "image://theme/icon-m-stop"
                enabled: vm.running && vm.connected && !vm.shutdownPending
                onClicked: vm.requestShutdown()
            }
            IconButton {
                width: parent.width / 4
                icon.source: "image://theme/icon-m-about"
                highlighted: page.showDiagnostics
                onClicked: page.showDiagnostics = !page.showDiagnostics
            }
        }
    }

    // Stable framebuffer host. It is never reparented during rotation.
    Item {
        id: displayHost
        x: 0
        y: page.landscapeLayout ? 0 : portraitToolbar.y + portraitToolbar.height
        width: page.landscapeLayout ? Math.max(1, page.width - page.sideWidth) : page.width
        height: page.landscapeLayout ? page.height
                : Math.min(page.height * 0.55, Math.max(page.width * 0.87, 350))
        clip: true

            // Preserve the original guest aspect ratio; zoom increases pixel
            // size and permits panning instead of stretching DOS into portrait.
            Item {
                id: consoleFrame
                x: Theme.paddingSmall
                y: Theme.paddingSmall
                // In landscape the four zoom actions are in a vertical strip.
                // They consume horizontal space, not guest-display height.
                width: Math.max(1, parent.width - 2 * Theme.paddingSmall -
                                 (page.landscapeLayout ? zoomColumn.width + Theme.paddingSmall : 0))
                height: page.landscapeLayout
                        ? Math.max(1, parent.height - 2 * Theme.paddingSmall)
                        : Math.max(1, zoomRow.y - 2 * Theme.paddingSmall)

                Rectangle { anchors.fill: parent; color: "black" }
                Flickable {
                    id: screenPan
                    anchors.fill: parent
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    contentWidth: Math.max(width, screenImage.width)
                    contentHeight: Math.max(height, screenImage.height)

                    Image {
                        id: screenImage
                        x: Math.max(0, (screenPan.contentWidth - width) / 2)
                        y: Math.max(0, (screenPan.contentHeight - height) / 2)
                        // sourceSize is the native PNG scanout size. Mouse
                        // input maps into current QEMU console coordinates.
                        width: Math.max(1, sourceSize.width > 0 && sourceSize.height > 0
                               ? Math.min(screenPan.width / sourceSize.width,
                                          screenPan.height / sourceSize.height)
                                 * sourceSize.width * page.zoomFactor : screenPan.width)
                        height: Math.max(1, sourceSize.width > 0 && sourceSize.height > 0
                                ? width * sourceSize.height / sourceSize.width : screenPan.height)
                        source: vm.guestPreviewUrl
                        fillMode: Image.Stretch
                        smooth: false
                        cache: false
                        asynchronous: false

                        // Render QEMU's supplied cursor shape and hotspot on
                        // top of the framebuffer. This does not alter pixels.
                        Image {
                            id: guestCursor
                            visible: page.showCursorOverlay && vm.liveDisplay &&
                                     vm.guestCursorVisible && vm.guestCursorUrl.length > 0
                            source: vm.guestCursorUrl
                            asynchronous: false
                            cache: false
                            smooth: false
                            fillMode: Image.Stretch
                            width: page.cursorDisplayX(sourceSize.width)
                            height: page.cursorDisplayY(sourceSize.height)
                            x: page.cursorDisplayX(vm.guestCursorX - vm.guestCursorHotX)
                            y: page.cursorDisplayY(vm.guestCursorY - vm.guestCursorHotY)
                        }
                        MouseArea {
                            id: guestTouch
                            anchors.fill: parent
                            enabled: vm.dbusMode && vm.mouseAvailable && vm.connected &&
                                     vm.running && !vm.paused && vm.guestPreviewAvailable &&
                                     page.mouseMode !== "pan"
                            preventStealing: true
                            property bool heldRight: false
                            property real pressedX: 0
                            property real pressedY: 0
                            property real lastMotionX: 0
                            property real lastMotionY: 0
                            property bool relativeMoved: false
                            onPressed: {
                                heldRight = false
                                pressedX = mouse.x
                                pressedY = mouse.y
                                lastMotionX = mouse.x
                                lastMotionY = mouse.y
                                relativeMoved = false
                                page.relativeFingerDown = !vm.mouseAbsolute
                                page.relativePendingX = 0
                                page.relativePendingY = 0
                                // In PS/2 mode the pad is NOT an absolute touch
                                // surface. Do not show a misleading click marker.
                                if (vm.mouseAbsolute) page.showTouchMark(pressedX, pressedY)
                                else page.touchMarkVisible = false
                                if (page.mouseMode === "drag") {
                                    if (vm.mouseAbsolute) {
                                        if (vm.guestMouseMove(page.guestX(mouse.x), page.guestY(mouse.y)) &&
                                            vm.guestMouseButton("left", true))
                                            page.mouseDragging = true
                                    } else if (vm.guestMouseButton("left", true)) {
                                        page.mouseDragging = true
                                    }
                                }
                            }
                            onPositionChanged: {
                                if (!vm.mouseAbsolute && page.relativeFingerDown) {
                                    var dx = mouse.x - lastMotionX
                                    var dy = mouse.y - lastMotionY
                                    lastMotionX = mouse.x
                                    lastMotionY = mouse.y
                                    // Scale gesture distances from actual image
                                    // pixels to guest mouse pixels; zoom aware.
                                    page.relativePendingX += dx * page.inputWidth /
                                        Math.max(1, screenImage.width) * page.relativeSpeed
                                    page.relativePendingY += dy * page.inputHeight /
                                        Math.max(1, screenImage.height) * page.relativeSpeed
                                    if (Math.abs(mouse.x - pressedX) > 10 ||
                                        Math.abs(mouse.y - pressedY) > 10)
                                        relativeMoved = true
                                } else if (page.mouseDragging) {
                                    page.pendingMouseX = page.guestX(mouse.x)
                                    page.pendingMouseY = page.guestY(mouse.y)
                                    page.mouseMotionPending = true
                                }
                            }
                            onReleased: {
                                if (page.relativeFingerDown) {
                                    page.flushRelativeMotion()
                                    page.relativeFingerDown = false
                                }
                                if (page.mouseDragging) {
                                    if (vm.mouseAbsolute)
                                        vm.guestMouseMove(page.guestX(mouse.x), page.guestY(mouse.y))
                                    page.releaseGuestMouse()
                                }
                            }
                            onCanceled: page.releaseGuestMouse()
                            onClicked: {
                                // A PS/2 swipe moves the existing guest cursor;
                                // it MUST NOT generate a click at the finger's
                                // final (unrelated) screen coordinate.
                                if (!heldRight && (vm.mouseAbsolute || !relativeMoved)) {
                                    if (page.mouseMode === "tap") {
                                        vm.guestMouseClick(page.guestX(pressedX), page.guestY(pressedY), "left")
                                    } else if (page.mouseMode === "double") {
                                        var gx = page.guestX(pressedX)
                                        var gy = page.guestY(pressedY)
                                        if (vm.guestMouseClick(gx, gy, "left")) {
                                            page.doubleClickX = gx
                                            page.doubleClickY = gy
                                            secondClickTimer.restart()
                                        }
                                        page.mouseMode = "tap"
                                    }
                                }
                            }
                            onPressAndHold: {
                                if (page.mouseMode === "tap") {
                                    heldRight = true
                                    vm.guestMouseClick(page.guestX(pressedX), page.guestY(pressedY), "right")
                                } else if (page.mouseMode === "double") {
                                    // Long holding must not trigger a double-click.
                                    heldRight = true
                                    page.mouseMode = "tap"
                                }
                            }
                        }
                        // Crosshair shows the exact image location that receives
                        // the touch, independent of guest cursor hotspot/scale.
                        Item {
                            visible: page.touchMarkVisible && page.mouseMode !== "pan"
                            x: page.touchMarkX - width / 2
                            y: page.touchMarkY - height / 2
                            width: 20
                            height: 20
                            Rectangle {
                                anchors.centerIn: parent
                                width: 16
                                height: 1
                                color: Theme.highlightColor
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width: 1
                                height: 16
                                color: Theme.highlightColor
                            }
                        }
                    }
                }
                // This QML waiting label disappears as soon as a frame exists.
                // QEMU's "Guest has not initialized the display (yet)" is
                // embedded in its original framebuffer, not rendered here.
                Label {
                    anchors.centerIn: parent
                    visible: !vm.guestPreviewAvailable
                    text: vm.connected ? "Waiting for guest screen..." : "Connecting to QEMU..."
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
            Row {
                id: zoomRow
                visible: !page.landscapeLayout
                x: Theme.paddingSmall
                y: parent.height - height - Theme.paddingSmall
                width: parent.width - 2 * Theme.paddingSmall
                height: Theme.itemSizeSmall
                spacing: 0
                IconButton {
                    width: zoomRow.width / 4
                    icon.source: "image://theme/icon-m-remove"
                    enabled: page.zoomFactor > 1.01
                    onClicked: page.zoomOut()
                }
                IconButton {
                    width: zoomRow.width / 4
                    icon.source: "image://theme/icon-m-scale"
                    onClicked: page.zoomFit()
                }
                IconButton {
                    width: zoomRow.width / 4
                    icon.source: "image://theme/icon-m-add"
                    enabled: page.zoomFactor < 3.99
                    onClicked: page.zoomIn()
                }
                IconButton {
                    width: zoomRow.width / 4
                    icon.source: "image://theme/icon-m-refresh"
                    enabled: vm.running && vm.connected
                    onClicked: vm.resyncGuestDisplay()
                }
            }
            // Stable, always-accessible landscape zoom column. The VM is
            // never reparented and the mouse geometry remains image-local.
            Column {
                id: zoomColumn
                visible: page.landscapeLayout
                width: page.landscapeLayout ? Theme.itemSizeSmall : 0
                height: page.landscapeLayout ? 4 * Theme.itemSizeSmall : 0
                x: parent.width - width - Theme.paddingSmall
                y: Math.max(Theme.paddingSmall, (parent.height - height) / 2)
                spacing: 0
                IconButton {
                    width: zoomColumn.width
                    height: Theme.itemSizeSmall
                    icon.source: "image://theme/icon-m-remove"
                    enabled: page.zoomFactor > 1.01
                    onClicked: page.zoomOut()
                }
                IconButton {
                    width: zoomColumn.width
                    height: Theme.itemSizeSmall
                    icon.source: "image://theme/icon-m-scale"
                    onClicked: page.zoomFit()
                }
                IconButton {
                    width: zoomColumn.width
                    height: Theme.itemSizeSmall
                    icon.source: "image://theme/icon-m-add"
                    enabled: page.zoomFactor < 3.99
                    onClicked: page.zoomIn()
                }
                IconButton {
                    width: zoomColumn.width
                    height: Theme.itemSizeSmall
                    icon.source: "image://theme/icon-m-refresh"
                    enabled: vm.running && vm.connected
                    onClicked: vm.resyncGuestDisplay()
                }
            }
    }

    SilicaFlickable {
        id: controlsPane
        x: page.landscapeLayout ? displayHost.width : 0
        y: page.landscapeLayout ? 0 : displayHost.y + displayHost.height
        width: page.landscapeLayout ? page.sideWidth : page.width
        height: Math.max(1, page.height - y)
        clip: true
        contentHeight: keyboardContent.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        Column {
            id: keyboardContent
            width: parent.width
            spacing: Theme.paddingSmall

            // Row 1: the active VM status replaces the redundant title.
            Label {
                x: Theme.paddingSmall
                width: parent.width - 2 * x
                visible: page.landscapeLayout
                text: vm.status + (vm.dbusMode ? (vm.liveDisplay ? " - Live D-Bus" : " - D-Bus") : " - SDL")
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.highlightColor
                truncationMode: TruncationMode.Fade
            }
            // Row 2: fixed, equally spaced actions. Focus/squeeze mode is gone.
            Row {
                id: commandStrip
                visible: page.landscapeLayout
                x: Theme.paddingSmall
                width: parent.width - 2 * x
                spacing: 0
                IconButton {
                    width: commandStrip.width / 4
                    height: Theme.itemSizeMedium
                    icon.source: "image://theme/icon-m-keyboard"
                    highlighted: page.keyboardExpanded
                    onClicked: page.keyboardExpanded = !page.keyboardExpanded
                }
                IconButton {
                    width: commandStrip.width / 4
                    height: Theme.itemSizeMedium
                    icon.source: vm.paused ? "image://theme/icon-m-play" : "image://theme/icon-m-pause"
                    enabled: vm.running && vm.connected
                    onClicked: vm.paused ? vm.resumeVm() : vm.pauseVm()
                }
                IconButton {
                    width: commandStrip.width / 4
                    height: Theme.itemSizeMedium
                    icon.source: "image://theme/icon-m-stop"
                    enabled: vm.running && vm.connected && !vm.shutdownPending
                    onClicked: vm.requestShutdown()
                }
                IconButton {
                    width: commandStrip.width / 4
                    height: Theme.itemSizeMedium
                    icon.source: "image://theme/icon-m-about"
                    highlighted: page.showDiagnostics
                    onClicked: page.showDiagnostics = !page.showDiagnostics
                }
            }
            // Three mouse-mode icons occupy one full-width row. L marks the
            // left-click/tap action; Drag and Pan have their own theme icons.
            Row {
                id: mouseModes
                x: page.landscapeLayout ? Theme.paddingSmall : Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: vm.dbusMode
                spacing: 0
                Repeater {
                    model: [
                        { mode: "tap", iconName: "icon-m-mouse" },
                        { mode: "drag", iconName: "icon-m-gesture" },
                        { mode: "pan", iconName: "icon-m-swipe" }
                    ]
                    delegate: IconButton {
                        width: mouseModes.width / 3
                        height: Theme.itemSizeMedium
                        icon.source: "image://theme/" + modelData.iconName
                        highlighted: page.mouseMode === modelData.mode
                        // Pan must remain usable without a guest mouse device.
                        onClicked: {
                            page.releaseGuestMouse()
                            page.mouseMode = modelData.mode
                        }
                        Label {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: Theme.paddingSmall / 2
                            visible: modelData.mode === "tap"
                            text: "L"
                            font.bold: true
                            font.pixelSize: Math.max(12, Theme.fontSizeExtraSmall * 0.72)
                            color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                        }
                    }
                }
            }
            // Row 4: right-click (R) and vertical scroll, without labels.
            Row {
                id: mouseExtraRow
                x: page.landscapeLayout ? Theme.paddingSmall : Theme.horizontalPageMargin
                width: parent.width - 2 * x
                // Keep R/scroll buttons present while the D-Bus mouse is
                // unavailable. Disabled controls make missing input explicit.
                visible: vm.dbusMode
                spacing: 0
                Repeater {
                    model: [
                        { iconName: "icon-m-mouse", cmd: "right" },
                        { iconName: "icon-m-up", cmd: "up" },
                        { iconName: "icon-m-down", cmd: "down" }
                    ]
                    delegate: IconButton {
                        width: mouseExtraRow.width / 3
                        height: Theme.itemSizeMedium
                        icon.source: "image://theme/" + modelData.iconName
                        enabled: vm.running && vm.connected && vm.mouseAvailable && !vm.paused
                        onClicked: {
                            if (modelData.cmd === "right") {
                                vm.guestMouseButton("right", true)
                                vm.guestMouseButton("right", false)
                            } else vm.guestMouseWheel(modelData.cmd === "up")
                        }
                        Label {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: Theme.paddingSmall / 2
                            visible: modelData.cmd === "right"
                            text: "R"
                            font.bold: true
                            font.pixelSize: Math.max(12, Theme.fontSizeExtraSmall * 0.72)
                            color: Theme.primaryColor
                        }
                    }
                }
            }

            Label {
                x: page.landscapeLayout ? Theme.paddingSmall : Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: vm.dbusMode && vm.running && (!vm.mouseAvailable || !vm.connected)
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.highlightColor
                text: "Guest mouse unavailable. QMP: " + (vm.connected ? "connected" : "disconnected")
                    + "; D-Bus: " + vm.dbusStatus + "; Mouse: " + vm.mouseStatus
            }

            Label {
                x: page.landscapeLayout ? Theme.paddingSmall : Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: vm.dbusMode && vm.mouseAvailable && !vm.mouseAbsolute
                text: "PS/2 touchpad: swipe to move the Windows pointer; tap to click where it is."
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
            }

            // Secondary input actions keep the existing three-column mouse
            // rows readable. 2xL arms one double-click on the next guest tap;
            // the lock holds this page's current orientation until toggled.
            Row {
                id: secondaryActions
                x: page.landscapeLayout ? Theme.paddingSmall : Theme.horizontalPageMargin
                width: parent.width - 2 * x
                spacing: 0
                IconButton {
                    width: secondaryActions.width / 2
                    height: Theme.itemSizeMedium
                    icon.source: "image://theme/icon-m-mouse"
                    highlighted: page.mouseMode === "double"
                    enabled: vm.dbusMode && vm.mouseAvailable && vm.running &&
                             vm.connected && !vm.paused
                    onClicked: {
                        page.releaseGuestMouse()
                        page.mouseMode = page.mouseMode === "double" ? "tap" : "double"
                    }
                    Label {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: Theme.paddingSmall / 2
                        text: "2×"
                        font.bold: true
                        font.pixelSize: Math.max(12, Theme.fontSizeExtraSmall * 0.70)
                        color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                }
                IconButton {
                    width: secondaryActions.width / 2
                    height: Theme.itemSizeMedium
                    icon.source: page.orientationLocked
                                 ? "image://theme/icon-m-screenlock"
                                 : "image://theme/icon-m-display"
                    highlighted: page.orientationLocked
                    onClicked: page.toggleOrientationLock()
                }
            }

            Label {
                x: Theme.paddingSmall
                width: parent.width - 2*x
                visible: vm.shutdownPending
                text: vm.shutdownStatus
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.highlightColor
            }

            Label {
                x: Theme.paddingSmall
                width: parent.width - 2*x
                visible: page.showDiagnostics
                text: "D-Bus: " + vm.dbusStatus
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }
            Label {
                x: Theme.paddingSmall
                width: parent.width - 2*x
                visible: page.showDiagnostics
                text: vm.liveDisplayStatus + " · " + vm.mouseStatus
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }
            BackgroundItem {
                x: Theme.paddingSmall
                width: parent.width - 2*x
                visible: page.showDiagnostics
                height: Theme.itemSizeSmall
                onClicked: pageStack.push(Qt.resolvedUrl("DiagnosticsPage.qml"))
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.paddingSmall
                    Icon { source: "image://theme/icon-m-about" }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Performance and runtime diagnostics"
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }
            }
            BackgroundItem {
                x: Theme.paddingSmall
                width: parent.width - 2*x
                visible: page.showDiagnostics && vm.dbusMode
                height: Theme.itemSizeSmall
                enabled: vm.running && vm.connected
                onClicked: vm.resyncGuestDisplay()
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.paddingSmall
                    Icon { source: "image://theme/icon-m-refresh" }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Resync guest display"
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }
            }
            BackgroundItem {
                x: Theme.paddingSmall
                width: parent.width - 2*x
                visible: page.showDiagnostics
                height: Theme.itemSizeSmall
                enabled: vm.running && vm.connected
                onClicked: vm.inspectAttachedMedia()
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.paddingSmall
                    Icon { source: "image://theme/icon-m-about" }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Inspect QEMU attached drives"
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }
            }
            Label {
                x: Theme.paddingSmall
                width: parent.width - 2*x
                visible: page.showDiagnostics && vm.attachedMediaReport.length > 0
                text: vm.attachedMediaReport
                wrapMode: Text.WrapAnywhere
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: vm.dbusMode && page.showDiagnostics
                text: page.mouseMode === "tap" ? "Tap: left-click; hold a finger for right-click."
                      : page.mouseMode === "drag" ? "Drag: hold and move to drag a window or select text."
                      : "Pan: swipe the zoomed guest display without sending mouse events."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }

            // Keep the cursor overlay optional: some guest drivers draw the
            // pointer directly into the framebuffer as well (double cursor).
            Row {
                x: page.landscapeLayout ? Theme.paddingSmall : Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: vm.dbusMode && page.showDiagnostics
                IconButton {
                    icon.source: "image://theme/icon-m-mouse"
                    highlighted: page.showCursorOverlay
                    onClicked: page.showCursorOverlay = !page.showCursorOverlay
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.showCursorOverlay ? "Cursor visible" : "Cursor hidden"
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: vm.dbusMode && page.showDiagnostics && vm.mouseAvailable
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                text: "Frame " + screenImage.sourceSize.width + " × " +
                      screenImage.sourceSize.height + "; QEMU mouse " +
                      page.inputWidth + " × " + page.inputHeight +
                      "; QEMU cursor " + vm.guestCursorX + "," + vm.guestCursorY +
                      ". " + vm.mouseInputDiagnostic
            }

            Button {
                x: Theme.horizontalPageMargin
                visible: page.showDiagnostics && vm.dbusMode && vm.mouseAvailable && vm.mouseAbsolute
                text: page.pointerTuning ? "Hide pointer alignment" : "Adjust pointer alignment"
                onClicked: page.pointerTuning = !page.pointerTuning
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: page.showDiagnostics && page.pointerTuning && vm.dbusMode && vm.mouseAbsolute
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
                text: "Per-VM offset (guest pixels): X " + vm.mouseOffsetX +
                      ", Y " + vm.mouseOffsetY +
                      ". Last touch sent (" + page.lastGuestX + ", " + page.lastGuestY +
                      "). First try Reset pointer offset, then adjust only if needed."
            }
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: page.showDiagnostics && page.pointerTuning && vm.dbusMode && vm.mouseAbsolute
                spacing: Theme.paddingSmall
                Repeater {
                    model: [
                        { label: "X −", dx: -4, dy: 0 },
                        { label: "X +", dx: 4, dy: 0 },
                        { label: "Y −", dx: 0, dy: -4 },
                        { label: "Y +", dx: 0, dy: 4 }
                    ]
                    delegate: Button {
                        width: (keyboardContent.width - 2 * Theme.horizontalPageMargin -
                                3 * Theme.paddingSmall) / 4
                        text: modelData.label
                        onClicked: vm.adjustMouseOffset(modelData.dx, modelData.dy)
                    }
                }
            }
            Button {
                x: Theme.horizontalPageMargin
                visible: page.showDiagnostics && page.pointerTuning && vm.dbusMode && vm.mouseAbsolute
                text: "Reset pointer offset"
                onClicked: vm.resetMouseOffset()
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
                visible: page.showDiagnostics
                text: vm.liveDisplay
                      ? "Live D-Bus display (CPU-conscious frame pacing). See Info for observed frame updates; Pan moves the zoomed view."
                      : vm.dbusMode
                        ? "Waiting for D-Bus frames; using QMP screenshot fallback. See listener status above."
                        : "SDL mode: guest display is a separate window. This preview refreshes about once per second."
            }

            Row {
                id: windowsKeys
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                spacing: Theme.paddingSmall
                visible: page.keyboardExpanded
                Repeater {
                    model: [
                        { label: "Start", code: "ctrl-esc" },
                        { label: "Alt+Tab", code: "alt-tab" },
                        { label: "Alt+F4", code: "alt-f4" }
                    ]
                    delegate: BackgroundItem {
                        width: (windowsKeys.width - 2 * windowsKeys.spacing) / 3
                        height: Theme.itemSizeSmall
                        enabled: vm.running && vm.connected && !vm.paused
                        Label {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: parent.down ? Theme.highlightColor : Theme.primaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }
                        onClicked: vm.sendGuestKey(modelData.code)
                    }
                }
            }
            // Boot menus can be controlled immediately, without first opening
            // the virtual keyboard or scrolling through configuration fields.
            Row {
                id: bootKeyRow
                visible: true
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                spacing: Theme.paddingSmall
                Repeater {
                    model: [
                        {label: "↑", code: "up"},
                        {label: "↓", code: "down"},
                        {label: "Enter", code: "ret"},
                        {label: "Esc", code: "esc"}
                    ]
                    delegate: BackgroundItem {
                        width: (bootKeyRow.width - 3 * bootKeyRow.spacing) / 4
                        height: Theme.itemSizeSmall
                        enabled: vm.running && vm.connected && !vm.paused
                        Label {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: parent.down ? Theme.highlightColor : Theme.primaryColor
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        onClicked: vm.sendGuestKey(modelData.code)
                    }
                }
            }

            TextField {
                id: commandInput
                visible: page.keyboardExpanded
                width: parent.width
                label: "Guest input"
                placeholderText: "Type guest command"
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                enabled: vm.running && vm.connected && !vm.paused
                // Silica TextField is not Qt Quick Controls TextField:
                // it does not provide an `accepted` signal. EnterKey is
                // Sailfish's supported software/hardware Return handler.
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: {
                    if (vm.sendGuestText(commandInput.text, true))
                        commandInput.text = ""
                }
            }
            Row {
                visible: page.keyboardExpanded
                id: sendActions
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                spacing: 0
                Column {
                    width: sendActions.width / 2
                    IconButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        icon.source: "image://theme/icon-m-send"
                        enabled: vm.running && vm.connected && !vm.paused
                        onClicked: {
                            if (vm.sendGuestText(commandInput.text, false))
                                commandInput.text = ""
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        font.pixelSize: Theme.fontSizeExtraSmall
                        text: "Send text"
                    }
                }
                Column {
                    width: sendActions.width / 2
                    IconButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        icon.source: "image://theme/icon-m-enter-accept"
                        enabled: vm.running && vm.connected && !vm.paused
                        onClicked: {
                            if (vm.sendGuestText(commandInput.text, true))
                                commandInput.text = ""
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        font.pixelSize: Theme.fontSizeExtraSmall
                        text: "Send + Enter"
                    }
                }
            }

            SectionHeader { text: "Guest special keys"; visible: page.keyboardExpanded }
            Grid {
                id: keyGrid
                visible: page.keyboardExpanded
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                columns: page.landscapeLayout ? 3 : 4
                spacing: Theme.paddingSmall
                Repeater {
                    model: [
                        { label: "Esc", code: "esc" },
                        { label: "Tab", code: "tab" },
                        { label: "Enter", code: "ret" },
                        { label: "Bksp", code: "backspace" },
                        { label: "Home", code: "home" },
                        { label: "Up", code: "up" },
                        { label: "End", code: "end" },
                        { label: "PgUp", code: "pgup" },
                        { label: "Left", code: "left" },
                        { label: "Down", code: "down" },
                        { label: "Right", code: "right" },
                        { label: "PgDn", code: "pgdn" },
                        { label: "Ins", code: "insert" },
                        { label: "Del", code: "delete" },
                        { label: "Space", code: "spc" },
                        { label: "Ctrl+C", code: "ctrl-c" },
                        { label: "F1", code: "f1" },
                        { label: "F2", code: "f2" },
                        { label: "F3", code: "f3" },
                        { label: "F4", code: "f4" },
                        { label: "F5", code: "f5" },
                        { label: "F6", code: "f6" },
                        { label: "F7", code: "f7" },
                        { label: "F8", code: "f8" },
                        { label: "F9", code: "f9" },
                        { label: "F10", code: "f10" },
                        { label: "F11", code: "f11" },
                        { label: "F12", code: "f12" }
                    ]
                    delegate: BackgroundItem {
                        width: (keyGrid.width - (keyGrid.columns - 1) * keyGrid.spacing) / keyGrid.columns
                        height: Theme.itemSizeSmall
                        enabled: vm.running && vm.connected && !vm.paused
                        Label {
                            anchors.centerIn: parent
                            text: modelData.label
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: parent.down ? Theme.highlightColor : Theme.primaryColor
                        }
                        onClicked: vm.sendGuestKey(modelData.code)
                    }
                }
            }
            BackgroundItem {
                x: Theme.paddingSmall
                width: parent.width - 2 * x
                height: Theme.itemSizeSmall
                visible: page.showDiagnostics
                enabled: vm.running && vm.connected
                onClicked: Remorse.popupAction(page, "Force power off VM", function() {
                    if (vm.running && vm.connected) vm.forceQuit()
                }, 4000)
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    Icon { source: "image://theme/icon-m-close" }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Force off (unsafe)"
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }
            }
            BackgroundItem {
                x: Theme.paddingSmall
                width: parent.width - 2 * x
                height: Theme.itemSizeSmall
                visible: page.keyboardExpanded
                enabled: vm.running && vm.connected && !vm.paused
                onClicked: {
                    Remorse.popupAction(page, "Restarting guest", function() {
                        if (vm.running && vm.connected && !vm.paused)
                            vm.sendGuestKey("ctrl-alt-delete")
                    }, 3000)
                }
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    Icon { source: "image://theme/icon-m-reboot" }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Ctrl+Alt+Del"
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: vm.lastError.length > 0
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: vm.lastError
            }
        }
    }
}
