#!/usr/bin/env python3
"""M14 source regression. Not a replacement for SDK compile/device test."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
h=(root/'src/vmcontroller.h').read_text()
c=(root/'src/vmcontroller.cpp').read_text()
p=(root/'qml/pages/ProfilesPage.qml').read_text()
e=(root/'qml/pages/VmEditPage.qml').read_text()
g=(root/'qml/pages/SailQemuGuidePage.qml').read_text()
for field in ('acpiEnabled','usbTabletEnabled'):
    assert f'Q_PROPERTY(bool {field}' in h
    assert f'profile.{field} = m_{field};' in c
    assert f'm_{field} = profile.{field};' in c
    assert f'settings.value(QStringLiteral("{field}"), true).toBool()' in c
    assert f'settings.setValue(QStringLiteral("{field}"), p.{field})' in c
    assert f'checked: vm.{field}' in e
for guest, cpu, memory, video, acpi, usb in (
    ('freedos','default',64,'std',False,False),
    ('win95','pentium',128,'cirrus',False,False),
    ('winxp','default',512,'std',True,True)):
    assert f'applyHardwarePreset("{guest}")' in e
    block=c.split('guest == QLatin1String("'+guest+'")',1)[1].split('} else',1)[0]
    assert f'm_cpuModel = QStringLiteral("{cpu}");' in block
    assert f'm_memoryMiB = {memory};' in block
    assert f'm_videoAdapter = QStringLiteral("{video}");' in block
    assert f'm_acpiEnabled = {str(acpi).lower()};' in block
    assert f'm_usbTabletEnabled = {str(usb).lower()};' in block
preset=c.split('bool VmController::applyHardwarePreset(',1)[1].split('void VmController::setFullscreen(',1)[0]
for forbidden in ('m_diskPath =', 'm_isoPath =', 'm_floppyPath =',
                  'm_bootPrimary =', 'm_networkEnabled =', 'm_qemuPath ='):
    assert forbidden not in preset, forbidden
assert 'QStringLiteral("pc,acpi=off")' in c
assert 'if (m_usbTabletEnabled)' in c
assert 'settings.setValue(QStringLiteral("meta/version"), 7)' in c
assert 'anchors.horizontalCenter: parent.horizontalCenter' in e
assert 'text: "Save VM"' in e
assert 'pendingEditId' in p and 'delayedEdit.restart()' in p
assert 'onClicked: page.editProfile(item.profileId)' in p
assert 'pageStack.push(Qt.resolvedUrl("VmEditPage.qml"), {createMode: false})' in p
assert 'Windows 95 DOS prompt' in g
assert '0.5.0' in (root/'rpm/harbour-sailqemu.spec').read_text()
print('M14 compatibility presets, old profile compatibility, Edit routing and centered Save: PASS')
