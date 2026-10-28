#!/usr/bin/env python3
"""M14r3 source-level QCOW safety checks. Does not compile Sailfish Qt/C++."""
from pathlib import Path
root = Path(__file__).resolve().parent.parent
c=(root/'src/vmcontroller.cpp').read_text()
e=(root/'qml/pages/VmEditPage.qml').read_text()
g=(root/'qml/pages/GuestKeyboardPage.qml').read_text()
spec=(root/'rpm/harbour-sailqemu.spec').read_text()
assert any('Release:    ' + v in spec for v in ('1', '2', '3', '4'))
assert 'static QString qcowHeaderFormat' in c
assert 'file.read(8)' in c
assert 'QByteArray::fromHex("514649fb")' in c
assert 'header.at(7) == 1' in c
assert 'header.at(7) == 2 || header.at(7) == 3' in c
assert 'const QString headerFormat = qcowHeaderFormat(path);' in c
assert 'setDiskFormat(detectDiskFormat(expanded));' in c
assert 'm_secondaryFormats[index] = detectDiskFormat(expanded);' in c
assert 'checkQcowMismatch(QStringLiteral("Primary HDD")' in c
assert 'checkQcowMismatch(QStringLiteral("Secondary HDD %1")' in c
assert 'checkQcowMismatch(QStringLiteral("CD/DVD")' in c
assert c.index('checkQcowMismatch(QStringLiteral("Primary HDD")') < c.index('QStringList args;')
assert 'if (m_liveHelper.isOpen()) m_liveHelper.readAllStandardOutput();' in c
assert 'image://theme/icon-m-info' not in e
assert 'image://theme/icon-m-info' not in g
for selection in ('vm.diskFormat = "qcow"','vm.setSecondaryDiskFormat(diskIndex, "qcow")','vm.isoFormat = "qcow"'):
    assert selection in e
for selected in ('vm.diskFormat === "qcow" ? 1','modelData.format === "qcow" ? 1','vm.isoFormat === "qcow" ? 1'):
    assert selected in e
# Representative QCOW big-endian headers (QFI\xfb followed by version).
def fmt(header):
    return ('qcow' if header[:4]==bytes.fromhex('514649fb') and int.from_bytes(header[4:8],'big')==1
            else 'qcow2' if header[:4]==bytes.fromhex('514649fb') and int.from_bytes(header[4:8],'big') in (2,3)
            else 'unknown')
assert fmt(bytes.fromhex('514649fb00000001')) == 'qcow'
assert fmt(bytes.fromhex('514649fb00000002')) == 'qcow2'
assert fmt(bytes.fromhex('514649fb00000003')) == 'qcow2'
assert fmt(bytes.fromhex('eb3c904d53444f53')) == 'unknown'
print('M14r3 QCOW1/2 detection, safety guard, dropdowns, missing icons: PASS')
