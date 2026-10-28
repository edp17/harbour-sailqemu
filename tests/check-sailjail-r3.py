#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parent.parent
cpp=(r/'src/vmcontroller.cpp').read_text()
qml=(r/'qml/pages/GuestKeyboardPage.qml').read_text()
per=(r/'sailjail/SailQemuDbus.permission').read_text()
spec=(r/'rpm/harbour-sailqemu.spec').read_text()
assert 'dbus-user.see org.qemu' in per
assert 'dbus-user.call org.qemu=org.freedesktop.DBus.Properties.*@/org/qemu/Display1/*' in per
assert 'dbus-user.call org.qemu=org.freedesktop.DBus.Properties.*@/org/qemu/Display1' in per
assert 'm_pid > 0 && processExists()' in cpp
assert '::kill(static_cast<pid_t>(m_pid), SIGTERM)' in cpp
assert 'm_forceQuitRequested = true' in cpp
assert 'Guest mouse unavailable. QMP:' in qml
assert 'visible: vm.dbusMode\n                spacing: 0' in qml
assert 'vm.connected && vm.mouseAvailable && !vm.paused' in qml
assert 'Release:    4' in spec
print('Sailjail r3 source regression: PASS')
