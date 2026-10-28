#!/usr/bin/env python3
"""Regression invariants for M14r2 (no SDK/toolchain compile performed)."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
c=(root/'src/vmcontroller.cpp').read_text()
h=(root/'src/vmcontroller.h').read_text()
e=(root/'qml/pages/VmEditPage.qml').read_text()
g=(root/'qml/pages/GuestKeyboardPage.qml').read_text()
spec=(root/'rpm/harbour-sailqemu.spec').read_text()
assert any('Release:    ' + v in spec for v in ('1', '2', '3', '4'))
assert 'Q_INVOKABLE bool saveProfileEdits' in h
assert 'bool VmController::saveProfileEdits' in c
assert 'if (id != m_activeId || profileIndex(id) < 0)' in c
assert 'copyActiveToProfile();' in c.split('bool VmController::saveProfileEdits',1)[1].split('QString VmController::mediaConfigurationReport',1)[0]
assert 'vm.saveProfileEdits(page.editingProfileId, nameField.text)' in e
assert 'text: page.createMode ? "" : vm.activeProfileName' not in e
assert 'Component.onCompleted:' in e
assert 'vm.inspectAttachedMedia()' in g
assert 'vm.mediaConfigurationReport()' in e
assert 'Q_INVOKABLE void inspectAttachedMedia();' in h
assert 'sendQmp(QStringLiteral("query-block"))' in c
assert 'QStringLiteral("file")' in c and 'QStringLiteral("drv")' in c
for slot in ('if=ide,index=0', 'if=ide,index=2,media=cdrom,readonly=on', 'if=floppy,index=0,format=raw,readonly=on'):
    assert slot in c
assert 'const int secondaryUnits[] = {1, 3};' in c
assert 'settings.setValue(QStringLiteral("diskPath"), p.diskPath)' in c
assert 'settings.setValue(QStringLiteral("isoPath"), p.isoPath)' in c
assert 'settings.value(QStringLiteral("diskPath"))' in c
assert 'settings.value(QStringLiteral("isoPath"))' in c
assert 'QByteArray::fromHex("514649fb")' in c
assert 'if (/\\.iso$/i.test(file)) vm.isoFormat = "raw"' in (root/'qml/pages/VmFilePickerPage.qml').read_text()
print('M14r2 name-save, configured storage, IDE slots, QMP runtime media diagnostics: PASS')
