#!/usr/bin/env python3
"""M13 focused static regression tests; device build and QEMU runtime testing required."""
from pathlib import Path
import re
root = Path(__file__).resolve().parent.parent
h = (root/'src/vmcontroller.h').read_text()
c = (root/'src/vmcontroller.cpp').read_text()
q = (root/'qml/pages/VmEditPage.qml').read_text()
p = (root/'qml/pages/ProfilesPage.qml').read_text()
about = (root/'qml/pages/AboutPage.qml').read_text()

assert 'vm.paused ? "Paused" : "Running"' in p
for member in ('cpuModel', 'cpuCount', 'videoAdapter'):
    assert re.search(r'Q_PROPERTY\([^\n]*\b'+member+r'\b', h), member
    assert 'settings.setValue(QStringLiteral("'+member+'"), p.'+member+')' in c, member
    assert 'settings.value(QStringLiteral("'+member+'"),' in c, member
    assert 'profile.'+member+' = m_'+member+';' in c
    assert 'm_'+member+' = profile.'+member+';' in c
    assert 'vm.'+member in q
assert 'settings.setValue(QStringLiteral("meta/version"), 7)' in c
assert 'QString cpuModel = QStringLiteral("default")' in h
assert 'int cpuCount = 1;' in h
assert 'QString videoAdapter = QStringLiteral("std")' in h
assert 'if (m_cpuModel != QLatin1String("default"))' in c
assert 'args << QStringLiteral("-cpu") << m_cpuModel' in c
assert 'if (m_cpuCount > 1)' in c
assert 'args << QStringLiteral("-smp") << (QStringLiteral("cpus=") + QString::number(m_cpuCount))' in c
assert '<< QStringLiteral("-vga") << m_videoAdapter' in c
assert 'if (value != 1 && value != 2 && value != 4) return;' in c
for item in ('486', 'pentium', 'pentium2', 'pentium3', 'qemu32', 'qemu64'):
    assert 'vm.cpuModel = "'+item+'"' in q
for item in ('std', 'cirrus', 'vmware'):
    assert 'vm.videoAdapter = "'+item+'"' in q
for item in (1, 2, 4):
    assert 'vm.cpuCount = '+str(item) in q
assert 'label: "Virtual CPUs"' in q
assert 'label: "Video adapter"' in q
assert 'label: "CPU model (x86 PC)"' in q
assert 'Version 0.5.0' in about
assert '0.5.0' in (root/'rpm/harbour-sailqemu.spec').read_text()
assert 'VERSION 0.5.0' in (root/'CMakeLists.txt').read_text()
print('M13 persisted hardware options, default compatibility, QEMU arguments, and paused status: PASS')
