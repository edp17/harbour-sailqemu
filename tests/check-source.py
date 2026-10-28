#!/usr/bin/env python3
"""Source/package invariants; this is NOT a substitute for an SDK build."""
from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
h = (root/'src/vmcontroller.h').read_text()
cpp = (root/'src/vmcontroller.cpp').read_text()
mainqml = (root/'qml/pages/MainPage.qml').read_text()
profileqml = (root/'qml/pages/ProfilesPage.qml').read_text()
editqml = (root/'qml/pages/VmEditPage.qml').read_text()
aboutqml = (root/'qml/pages/AboutPage.qml').read_text()
desktop = (root/'harbour-sailqemu.desktop').read_text()
spec = (root/'rpm/harbour-sailqemu.spec').read_text()
cmake = (root/'CMakeLists.txt').read_text()

assert 'X-Sailfish-ApplicationType=Silica' in desktop
assert 'X-Nemo-Application-Type=silica-qt5' not in desktop
assert 'Sandboxing=Disabled' not in desktop and 'SailQemuDbus' in desktop
assert 'Version:    0.5.0' in spec
assert any('Release:    ' + v in spec for v in ('1', '2', '3', '4'))
assert 'QUuid::WithoutBraces' not in cpp.replace('// QUuid::WithoutBraces was introduced in Qt 5.11; Sailfish Qt is older.', '')
assert 'id.remove(QLatin1Char(' in cpp
assert 'VERSION 0.5.0' in cmake
assert 'QStandardPaths::AppConfigLocation' in cpp
assert 'QStandardPaths::ConfigLocation' in cpp
assert '"/vm.ini"' in cpp and '"/profiles.ini"' in cpp
assert 'migrateLegacy()' in cpp
assert 'QFile::exists(legacyFile())' in cpp
assert 'legacy.value(QStringLiteral("vm/qemuPath")' in cpp
assert 'legacy.value(QStringLiteral("runtime/pid")' in cpp
assert 'settings.remove(QStringLiteral("profiles"))' in cpp
assert 'QFile::remove(socketFile())' in cpp
assert 'QFile::remove(legacyFile())' not in cpp
assert 'profile.qemuPath = m_qemuPath' in cpp
assert 'p.qemuPath = m_qemuPath' in cpp
assert 'm_profiles.append(copy)' in cpp
assert 'VmController::processExists() const' in cpp
assert 'args.contains(m_socketPath.toUtf8())' in cpp
assert '"qmp.sock"' not in cpp  # filename held in QStringLiteral, not magic duplicated
assert 'QStringLiteral("/qmp.sock")' in cpp
for method in ['createProfile', 'selectProfile', 'renameProfile', 'duplicateProfile', 'deleteProfile', 'startVm']:
    assert f'Q_INVOKABLE bool {method}(' in h
    assert f'bool VmController::{method}(' in cpp
for method in ['pauseVm', 'resumeVm', 'requestShutdown', 'forceQuit']:
    assert f'void VmController::{method}()' in cpp
assert 'vm.activeProfileName' in mainqml
assert 'Qt.resolvedUrl("ProfilesPage.qml")' in mainqml
assert 'Remorse.popupAction' in mainqml
assert 'vm.profiles' in profileqml
assert 'vm.saveProfileEdits(' in editqml
assert 'vm.duplicateProfile(' in profileqml
assert 'vm.deleteProfile(' in profileqml
assert 'vm.createProfile(' in editqml
for path in (root/'qml').rglob('*.qml'):
    s = path.read_text()
    # Braces sanity, strip quoted strings, simple line comments.
    s = re.sub(r'"(?:\\.|[^"\\])*"', '""', s)
    s = re.sub(r'//[^\n]*', '', s)
    assert s.count('{') == s.count('}'), str(path)
assert 'automaticCheck: false' in editqml
assert 'onCheckedChanged: if (!vm.running) vm.fullscreen' not in mainqml
assert 'vm.fullscreen = !vm.fullscreen' in editqml
assert 'text: "Manage VMs"' in mainqml
assert 'VmFilePickerPage.qml' in editqml
assert 'FilePickerPage' in (root/'qml/pages/VmFilePickerPage.qml').read_text()
assert 'Sailfish.Pickers 1.0' in (root/'qml/pages/VmFilePickerPage.qml').read_text()
for key in ['isoBootFirst', 'networkEnabled']:
    assert key in h and key in cpp
    assert f'profile.{key} = m_' in cpp
    assert f'settings.setValue(QStringLiteral("{key}"), p.{key})' in cpp
    assert f'settings.value(QStringLiteral("{key}")' in cpp
assert 'VmController::legacyBootOrder() const' in cpp and 'VmController::effectiveBootOrder() const' in cpp
assert "bootOrder += QLatin1Char('a')" in cpp
assert "bootOrder += QLatin1Char('c')" in cpp
assert 'QStringLiteral("-boot") << (QStringLiteral("order=") + bootOrder)' in cpp
assert 'nic,model=rtl8139' in cpp
assert 'VmController::chooseDiskImage(' in cpp
assert (root/'scripts/qemu-packaging-inventory.sh').exists()
for key in ['floppyPath','floppyBootFirst']:
    assert key in h and key in cpp
    assert f'profile.{key} = m_' in cpp
    assert f'settings.setValue(QStringLiteral("{key}"), p.{key})' in cpp
    assert f'settings.value(QStringLiteral("{key}")' in cpp
assert 'selectFloppy' in (root/'qml/pages/VmFilePickerPage.qml').read_text()
assert 'if=floppy,index=0,format=raw,readonly=on' in cpp
assert 'migratedLegacyFloppy' in cpp
assert 'p.isoPath.endsWith(QLatin1String(".fd")' in cpp
assert 'p.isoPath.endsWith(QLatin1String(".ima")' in cpp
assert 'meta/version"), 7' in cpp
assert 'GuestKeyboardPage.qml' in mainqml
keyboardqml = (root/'qml/pages/GuestKeyboardPage.qml').read_text()
assert 'sendGuestText' in keyboardqml and 'sendGuestKey' in keyboardqml
assert 'guestPreviewUrl' in keyboardqml and 'refreshGuestPreview' in keyboardqml
assert 'PageStatus.Active' in keyboardqml
assert 'Q_INVOKABLE bool sendGuestText' in h
assert 'Q_INVOKABLE bool sendGuestKey' in h
assert 'Q_INVOKABLE void refreshGuestPreview' in h
assert 'bool VmController::sendGuestText' in cpp
assert 'bool VmController::sendGuestKey' in cpp
assert 'void VmController::refreshGuestPreview' in cpp
assert 'QStringLiteral("send-key")' in cpp
assert 'QStringLiteral("screendump")' in cpp
assert 'm_keyTimer.setInterval(130)' in cpp
assert 'm_guestPreviewPending = false;' in cpp
assert 'bool VmController::startVmWithConsole()' in cpp
assert 'bool VmController::startVmInternal(bool dbusDisplay)' in cpp
assert 'QStringLiteral("-S")' not in cpp  # startup must not be paused
assert 'vm.startVmWithConsole()' in mainqml
assert 'Start SDL only' in mainqml
assert 'screenPan' in keyboardqml and 'page.zoomFactor' in keyboardqml
assert 'vm.paused ? vm.resumeVm() : vm.pauseVm()' in keyboardqml
assert 'interval: 1000' in keyboardqml
assert 'vm.sendGuestKey(modelData.code)' in keyboardqml
assert 'X-Sailfish-ApplicationType=Silica' in desktop
print('M3r4 source/package invariants: PASS')
print('NOTE: Cannot validate SDK compilation or runtime/QML type registration here.')

# Regression: Sailfish Silica TextField has no `accepted` signal.
# `EnterKey.onClicked` is the supported callback for its Return key.
assert 'onAccepted:' not in keyboardqml
assert 'EnterKey.onClicked:' in keyboardqml
assert 'EnterKey.iconSource: "image://theme/icon-m-enter-accept"' in keyboardqml
assert 'vm.sendGuestText(commandInput.text, true)' in keyboardqml
print('M3r4r2 Silica EnterKey compatibility: PASS')

# M5 QtDBus discovery-only mode; SDL must remain available.
assert 'Qt5::DBus' in cmake and 'Qt5DBus' in spec
assert 'startVmWithDbusConsole' in h and 'startVmWithDbusConsole' in cpp
assert 'Q_PROPERTY(bool dbusReady' in h and 'refreshDbusDisplay()' in h
assert 'QStringLiteral("dbus,gl=off")' in cpp
assert 'QStringLiteral("sdl,gl=off")' in cpp
assert 'm_qmpNameConfirmed' in cpp
assert 'QStringLiteral("org.qemu.Display1.Console")' in cpp
assert 'QStringLiteral("ConsoleIDs")' in cpp
assert 'runtime/dbusMode' in cpp
assert 'text: "Start VM"' in mainqml
assert 'Start SDL only' in mainqml
assert 'startVmWithDbusConsole()' in mainqml
print('M5 QtDBus discovery and SDL fallback source invariants: PASS')

# M6 unprivileged D-Bus framebuffer, screenshot fallback, and packaging.
listener = (root/'src/dbusdisplayhelper.cpp').read_text()
assert 'harbour-sailqemu-display-listener' in cmake and spec
assert 'pkg_check_modules(GIO REQUIRED gio-2.0 gio-unix-2.0)' in cmake
assert 'pkgconfig(gio-unix-2.0)' in spec
assert 'RegisterListener' in listener and 'g_unix_fd_list_append' in listener
assert 'G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT' in listener
assert 'Scanout' in listener and 'Update' in listener
assert 'g_dbus_connection_register_object' in listener
assert 'QSaveFile' in listener and 'QImage' in listener
assert 'm_qmpNameConfirmed' in cpp
assert 'VmController::launchLiveDisplay' in cpp
assert 'VmController::stopLiveDisplay' in cpp
assert 'm_dbusMode && m_liveDisplayAvailable' in cpp
assert 'vm.liveDisplayStatus' in keyboardqml
print('M6 D-Bus frame listener, sandbox fallback and build wiring: PASS')

# M6r2: Qt5/GLib C++ compatibility and source-only regression checks.
assert 'QT_NO_KEYWORDS' in cmake.split('target_compile_definitions(harbour-sailqemu-display-listener PRIVATE ',1)[1]
assert 'Qt::hex' not in listener and 'Qt::dec' not in listener
assert 'QString::number(fmt, 16)' in listener
assert 'g_variant_new("(h)", index)' in listener  # Unix RegisterListener ABI unchanged
print('M6r2 Qt/GLib compatibility source checks: PASS')

# M7 mouse/cursor integration, reopen active guest, and NAT preflight.
assert 'Q_INVOKABLE bool guestMouseMove' in h
assert 'Q_INVOKABLE bool guestMouseClick' in h
assert 'Q_INVOKABLE bool guestMouseWheel' in h
assert 'VmController::refreshDbusMouse' in cpp
assert 'QStringLiteral("org.qemu.Display1.Mouse")' in cpp
assert 'QStringLiteral("IsAbsolute")' in cpp
assert 'QStringLiteral("SetAbsPosition")' in cpp
assert 'QStringLiteral("RelMotion")' in cpp
assert 'QRegExp userBackend' in cpp
assert 'QProcess probe;' in cpp
assert 'Return to running VM' in mainqml
assert 'guestMouseClick' in keyboardqml
assert 'page.mouseMode === "tap"' in keyboardqml
assert 'page.mouseMode === "drag"' in keyboardqml
assert 'mode: "pan"' in keyboardqml
assert 'guestCursorUrl' in keyboardqml
assert 'CursorDefine' in listener and 'MouseSet' in listener
assert 'writeCursorState' in listener
print('M7 mouse/cursor, navigation and network preflight source invariants: PASS')

# M8: per-profile alignment, focus view and Windows shortcuts.
assert 'mouseOffsetX' in h and 'mouseOffsetY' in h
assert 'VmController::adjustMouseOffset' in cpp
assert 'VmController::resetMouseOffset' in cpp
assert 'settings.setValue(QStringLiteral("mouseOffsetX"), p.mouseOffsetX)' in cpp
assert 'settings.value(QStringLiteral("mouseOffsetY"), 0)' in cpp
assert 'code == QLatin1String("alt-tab")' in cpp
assert 'code == QLatin1String("ctrl-esc")' in cpp
assert 'page.touchMarkX' in keyboardqml and 'page.touchMarkY' in keyboardqml
assert 'function guestX(px)' in keyboardqml
assert 'screenImage.sourceSize.width' in keyboardqml
assert 'page.focusView' not in keyboardqml
assert 'page.keyboardExpanded' in keyboardqml
assert 'vm.adjustMouseOffset' in keyboardqml
assert 'vm.resetMouseOffset()' in keyboardqml
print('M8 pointer alignment and hotkeys source invariants: PASS')

# M8r2: mouse input and cursor must use the same dynamic console geometry.
assert 'Q_PROPERTY(int mouseConsoleWidth' in h
assert 'Q_PROPERTY(int mouseConsoleHeight' in h
assert 'VmController::refreshMouseGeometry' in cpp
assert 'refreshMouseGeometry();' in cpp
assert 'QDBusPendingCallWatcher' in cpp
assert 'function cursorDisplayX(guestX)' in keyboardqml
assert 'function cursorDisplayY(guestY)' in keyboardqml
assert 'page.cursorDisplayX(vm.guestCursorX - vm.guestCursorHotX)' in keyboardqml
assert 'page.cursorDisplayY(vm.guestCursorY - vm.guestCursorHotY)' in keyboardqml
assert 'page.inputWidth / screenImage.width' in keyboardqml
assert 'page.inputHeight / screenImage.height' in keyboardqml
assert 'Cursor hidden' in keyboardqml
assert 'vm.mouseInputDiagnostic' in keyboardqml
assert any('Release:    ' + v in spec for v in ('1', '2', '3', '4'))
print('M8r2 input-console mapping and cursor diagnostics: PASS')

# M9r2: stable display host, genuinely separate controls, and guide.
assert 'Q_PROPERTY(bool shutdownPending' in h
assert 'VmController::resyncGuestDisplay' in cpp
assert 'system_powerdown' in cpp and 'm_shutdownPending = true;' in cpp
assert 'm_shutdownPending = false;' in cpp
assert 'Force off (unsafe)' in keyboardqml
assert 'id: portraitToolbar' in keyboardqml
assert 'id: displayHost' in keyboardqml
assert 'id: controlsPane' in keyboardqml
assert 'id: consoleFrame' in keyboardqml
assert 'id: zoomRow' in keyboardqml
assert 'ParentChange' not in keyboardqml
assert 'landscapeDisplayHost' not in keyboardqml
assert 'controlsPane.contentY = 0' in keyboardqml
assert 'page.sideWidth' in keyboardqml
assert 'page.showDiagnostics' in keyboardqml
assert 'property bool keyboardExpanded: false' in keyboardqml
assert 'visible: page.landscapeLayout' in keyboardqml
assert 'icon-m-display' in keyboardqml and 'icon-m-refresh' in keyboardqml
assert 'icon-m-mouse' in keyboardqml
assert 'text: "ACPI off"' in mainqml
assert 'PullDownMenu' in mainqml
assert 'SailQemuGuidePage.qml' in mainqml
assert 'property bool createMode: false' in editqml
assert 'visible: !page.createMode' in editqml
assert 'visible: page.showDiagnostics' in keyboardqml
assert (root/'qml/pages/SailQemuGuidePage.qml').exists()
guide=(root/'qml/pages/SailQemuGuidePage.qml').read_text()
for key in ['User guide', 'ACPI off', 'libslirp', 'Guest console', 'Resync']:
    assert key in guide
print('M9r2 stable orientation, compact controls and guide source checks: PASS')

# M10: notch-safe stable layout and modest zoom increments.
assert 'CutoutMode.AvoidLandscapeCutout' in keyboardqml
assert 'id: safePortraitHeader' in keyboardqml
assert 'y: page.landscapeLayout ? 0 : safePortraitHeader.height' in keyboardqml
assert 'id: zoomColumn' in keyboardqml
assert 'visible: !page.landscapeLayout' in keyboardqml
assert 'height: page.landscapeLayout ? 4 * Theme.itemSizeSmall : 0' in keyboardqml
assert 'zoomFactor * zoomStep' in keyboardqml
assert 'zoomFactor / zoomStep' in keyboardqml
assert 'readonly property real zoomStep: 1.15' in keyboardqml
assert 'ParentChange' not in keyboardqml
print('M10 vertical zoom, notch-aware toolbar, and stable framebuffer host: PASS')

# M10r2: predictable landscape rows, matching portrait actions and mouse legends.
assert 'toggleFocus' not in keyboardqml and 'focusView' not in keyboardqml
assert 'text: "VM controls"' not in keyboardqml
assert 'text: vm.status + (vm.dbusMode ?' in keyboardqml
assert keyboardqml.index('id: commandStrip') < keyboardqml.index('id: mouseModes') < keyboardqml.index('id: mouseExtraRow') < keyboardqml.index('id: windowsKeys')
assert keyboardqml.count('icon.source: "image://theme/icon-m-about"') == 2
assert 'text: "L"' in keyboardqml and 'text: "R"' in keyboardqml
assert 'visible: modelData.mode === "tap"' in keyboardqml
assert 'visible: modelData.cmd === "right"' in keyboardqml
assert 'label: "Wheel' not in keyboardqml and 'label: "Tap"' not in keyboardqml
assert 'Keyboard, Pause/Resume, ACPI off and Info' in guide
assert keyboardqml.index('id: mouseExtraRow') < keyboardqml.index('text: "D-Bus: " + vm.dbusStatus') < keyboardqml.index('id: windowsKeys')
assert 'A separately built network-enabled QEMU RPM' in guide
print('M10r2 sidebar spacing, click legends and guide: PASS')

# M11: guest double-click, page-scoped orientation lock and icon guide.
assert 'page.mouseMode === "double"' in keyboardqml
assert 'id: secondClickTimer' in keyboardqml
assert 'interval: 110' in keyboardqml
assert 'secondClickTimer.restart()' in keyboardqml
assert 'page.mouseMode = "tap"' in keyboardqml
assert 'orientationLocked ? page.lockedOrientation : Orientation.All' in keyboardqml
assert 'lockedOrientation = page.orientation' in keyboardqml
assert 'id: secondaryActions' in keyboardqml
assert 'icon-m-screenlock' in keyboardqml and 'icon-m-display' in keyboardqml
assert 'Controls — illustrated reference' in guide
assert 'Theme.iconSizeMedium' in guide
for icon in ['icon-m-keyboard', 'icon-m-pause', 'icon-m-stop', 'icon-m-about',
             'icon-m-mouse', 'icon-m-gesture', 'icon-m-swipe', 'icon-m-up',
             'icon-m-down', 'icon-m-scale', 'icon-m-refresh']:
    assert icon in guide
assert 'Double click — arm two clicks at the next touch' in guide
assert 'Orientation lock — lock the Guest console' in guide
print('M11 one-shot double-click, orientation lock and illustrated guide: PASS')

# 0.9.3: independent VM management/settings, simplified landing screen and About.
assert 'PullDownMenu' in mainqml
for title in ['Manage VMs', 'User guide', 'About']:
    assert f'text: "{title}"' in mainqml
assert 'ComboBox' in mainqml and 'vm.selectProfile(modelData.id)' in mainqml
assert 'text: "Start VM"' in mainqml and 'vm.startVmWithDbusConsole()' in mainqml
assert 'Behavior on rotation' in mainqml and 'NumberAnimation' in mainqml
assert 'text: "Start"' in mainqml and 'text: "Start SDL only"' in mainqml
assert 'PageHeader { title: "Manage VMs" }' in profileqml
assert 'VmEditPage.qml' in profileqml and 'remorseDelete' in profileqml
assert 'vm.createProfile(' in editqml and 'vm.saveProfileEdits(' in editqml
assert 'vm.saveProfileEdits(' in editqml and 'automaticCheck: false' in editqml
assert 'Version 0.5.0' in aboutqml and 'Source code' in aboutqml
assert 'file:///usr/share/icons/hicolor/86x86/apps/harbour-sailqemu.png' in aboutqml
assert 'icon-m-display' in keyboardqml and 'icon-m-display' in guide
assert 'icon-m-rotate' not in keyboardqml
assert 'PageHeader { title: "User guide" }' in guide
assert 'Start a virtual machine' in guide
print('M11r2 Main / Manage / Add-Edit / User guide / About source checks: PASS')

# M11r3 0.9.4: structured VM settings with persistent media/boot choices.
for section in ['Disks', 'HDD', 'ISO / CD / DVD', 'Floppy', 'Boot order', 'Hardware', 'Miscellaneous']:
    assert section in editqml, section
for setting in ['secondaryDisks', 'secondaryFormats', 'isoFormat', 'bootPrimary', 'bootSecondary']:
    assert setting in h and setting in cpp, setting
    assert f'settings.setValue(QStringLiteral("{setting}"), p.{setting})' in cpp
    assert f'settings.value(QStringLiteral("{setting}")' in cpp
assert 'selectSecondary' in (root/'qml/pages/VmFilePickerPage.qml').read_text()
assert 'vm.chooseSecondaryDiskImage(secondaryIndex, file)' in (root/'qml/pages/VmFilePickerPage.qml').read_text()
assert 'vm.secondaryDisks' in editqml and 'vm.addSecondaryDisk()' in editqml
assert 'IntValidator { bottom: 16; top: 32768 }' in editqml
assert 'inputMethodHints: Qt.ImhDigitsOnly' in editqml
assert 'visible: memoryBox.currentIndex === 7' in editqml
assert 'vm.bootPrimary = "floppy"' in editqml and 'vm.bootSecondary = "none"' in editqml
assert 'enabled: vm.bootPrimary !== "hdd"' in editqml
assert 'if=ide,index=0' in cpp and 'const int secondaryUnits[] = {1, 3}' in cpp
assert 'if=ide,index=2,media=cdrom,readonly=on' in cpp
assert 'm_fullscreen && !dbusDisplay' in cpp
print('M11r3 structured VM configuration and backward-compatible boot profiles: PASS')
