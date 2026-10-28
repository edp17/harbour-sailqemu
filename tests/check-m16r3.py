#!/usr/bin/env python3
"""M16r3 navigation and release-readiness source checks (not an SDK build)."""
from pathlib import Path
import re
root = Path(__file__).resolve().parent.parent
main = (root/'qml/pages/MainPage.qml').read_text()
profiles = (root/'qml/pages/ProfilesPage.qml').read_text()
guide = (root/'qml/pages/SailQemuGuidePage.qml').read_text()
edit = (root/'qml/pages/VmEditPage.qml').read_text()
guest = (root/'qml/pages/GuestKeyboardPage.qml').read_text()
script = (root/'scripts/m16-release-check.sh').read_text()
about = (root/'qml/pages/AboutPage.qml').read_text()
cmake = (root/'CMakeLists.txt').read_text()
spec = (root/'rpm/harbour-sailqemu.spec').read_text()
assert main.split('PullDownMenu {', 1)[1].split('        Column {', 1)[0].count('MenuItem {') == 3
assert profiles.split('PullDownMenu {', 1)[1].split('        Column {', 1)[0].count('MenuItem {') == 3  # Pulley only; ContextMenu is separate
for name, route in [('Manage VMs', 'ProfilesPage.qml'),
                    ('User guide', 'SailQemuGuidePage.qml'), ('About', 'AboutPage.qml')]:
    assert f'text: "{name}"' in main and route in main
for name, route in [('Add new', 'VmEditPage.qml'),
                    ('Disk tools', 'DiskToolsPage.qml'), ('Diagnostics', 'DiagnosticsPage.qml')]:
    assert f'text: "{name}"' in profiles and route in profiles
assert 'text: "Disk tools"' not in main and 'text: "Diagnostics"' not in main
assert 'DiskToolsPage.qml' in edit and 'DiagnosticsPage.qml' in guest
assert 'From Main, open Manage VMs and choose Diagnostics' in guide
assert "from Manage VMs' pulley menu" in guide
assert 'First run: QEMU installation' in guide
assert 'check_readable' in script
for path in ('harbour-sailqemu.png', 'MainPage', 'ProfilesPage', 'VmEditPage',
             'GuestKeyboardPage', 'DiskToolsPage', 'DiagnosticsPage', 'SailQemuGuidePage', 'AboutPage'):
    assert path in script, path
assert 'VERSION 0.5.0' in cmake
assert 'Version:    0.5.0' in spec and 'Release:    4' in spec
assert 'Version 0.5.0-4' in about
assert 'QTemporaryDir::filePath' not in (root/'src/diskmanager.cpp').read_text()
print('M16r3 navigation, guidance, preflight packaging and version checks: PASS')
