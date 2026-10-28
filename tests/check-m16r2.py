#!/usr/bin/env python3
"""M16r2 static regression checks; SDK compilation/device test still required."""
from pathlib import Path
p = Path(__file__).resolve().parent.parent
cpp=(p/'src/vmcontroller.cpp').read_text()
h=(p/'src/vmcontroller.h').read_text()
d=(p/'src/runtimediagnostics.cpp').read_text()
dh=(p/'src/runtimediagnostics.h').read_text()
qml=(p/'qml/pages/DiagnosticsPage.qml').read_text()
main=(p/'qml/pages/MainPage.qml').read_text()
assert 'VERSION 0.5.0' in (p/'CMakeLists.txt').read_text()
assert 'Version:    0.5.0' in (p/'rpm/harbour-sailqemu.spec').read_text()
assert 'Version 0.5.0-4' in (p/'qml/pages/AboutPage.qml').read_text()
assert 'm_hadQmpConnection = true;' in cpp
assert 'm_seenGuestShutdown = true;' in cpp
assert 'm_forceQuitRequested = true;' in cpp
assert 'm_qmpConnectionStartMs' in h
assert 'm_qmpConnectionWarned' in cpp
assert 'QEMU exited before QMP connected' in cpp
assert 'Guest shut down; QEMU exited.' in cpp
assert 'QEMU process is alive, but QMP is not connected after 15 seconds' in cpp
save=cpp.split('void VmController::save()',1)[1].split('#define DEFINE_STRING_SETTER',1)[0]
assert 'm_lastError.clear()' not in save
assert 'QClipboard' in d
assert 'QGuiApplication::clipboard()' in d
assert 'recentQemuLog' in d
assert 'file.seek(length - 4096)' in d
assert 'file.read(4096)' in d
assert 'VM status: %1' in d
assert 'Recent QEMU log' in d
assert 'm16-release-check.sh' in (p/'CMakeLists.txt').read_text()
assert (p/'scripts/m16-release-check.sh').is_file()
assert 'Copy report' in qml
assert 'diagnostics.copyReport()' in qml
assert 'vm.status.indexOf("QEMU exited")' in main
print('M16r2 startup recovery, Qt 5.6 paths, and diagnostic report checks: PASS')
