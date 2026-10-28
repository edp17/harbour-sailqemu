#!/usr/bin/env python3
"""M15 structural regression suite, not an SDK compiler/test."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
h=(root/'src/diskmanager.h').read_text()
s=(root/'src/diskmanager.cpp').read_text()
v=(root/'src/vmcontroller.cpp').read_text()
q=(root/'qml/pages/DiskToolsPage.qml').read_text()
main=(root/'qml/pages/MainPage.qml').read_text()
edit=(root/'qml/pages/VmEditPage.qml').read_text()
cmake=(root/'CMakeLists.txt').read_text()
assert 'VERSION 0.5.0' in cmake and 'src/diskmanager.cpp' in cmake
assert 'Version:    0.5.0' in (root/'rpm/harbour-sailqemu.spec').read_text()
assert 'DiskManager disks(&vm);' in (root/'src/main.cpp').read_text()
assert 'setContextProperty(QStringLiteral("disks"), &disks)' in (root/'src/main.cpp').read_text()
for m in ['inspectImage','createImage','copyImage','cancel']:
    assert m in h and 'DiskManager::'+m in s
assert 'QTemporaryDir(parent.filePath' in s
assert 'QFile::rename(m_stagedPath, m_destination)' in s
assert 'm_stage.reset()' in s
assert 'm_vm->running()' in s
assert 'qemu-img' in s
assert 'QStringLiteral("convert")' in s and 'QStringLiteral("create")' in s
assert 'backing-filename' in s
assert 'VmController::storageReferences' in v
assert 'Shared writable disk' in v
assert 'DiskToolsPage.qml' in (root/'qml/pages/ProfilesPage.qml').read_text() and 'DiskToolsPage.qml' in edit
assert 'DiskSourcePickerPage.qml' in q
assert 'disks.copyImage' in q and 'disks.createImage' in q
assert 'qemu-img' in (root/'docs/M15-disk-tools.md').read_text()
print('M15 disk tools structure + guards: PASS')
