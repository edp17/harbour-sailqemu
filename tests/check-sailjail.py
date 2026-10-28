#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parent.parent
cpp=(r/'src/vmcontroller.cpp').read_text()
perm=(r/'sailjail/SailQemuDbus.permission').read_text()
desktop=(r/'harbour-sailqemu.desktop').read_text()
assert 'posix_spawn(&child' in cpp
assert 'posix_spawn_file_actions_addopen' in cpp
assert 'QProcess::startDetached(QStringLiteral("/bin/sh")' not in cpp
assert 'Could not start the QEMU process' not in cpp
assert 'dbus-user.own org.qemu' in perm
assert 'dbus-user.talk org.qemu' in perm
assert 'private-bin harbour-sailqemu-display-listener' in perm
assert 'Permissions=UserDirs;RemovableMedia;Internet;SailQemuDbus' in desktop
assert 'Sandboxing=Disabled' not in desktop
assert 'SailQemuDbus.permission' in (r/'CMakeLists.txt').read_text()
assert '/etc/sailjail/permissions/SailQemuDbus.permission' in (r/'rpm/harbour-sailqemu.spec').read_text()
print('Sailjail launch and permission source checks: PASS')
