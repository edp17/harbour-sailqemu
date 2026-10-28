#!/usr/bin/env python3
"""Guard the namespace-safe QEMU identity binding (source checks only)."""
from pathlib import Path
r = Path(__file__).resolve().parent.parent
cpp = (r / "src/vmcontroller.cpp").read_text()
listener = (r / "src/dbusdisplayhelper.cpp").read_text()
h = (r / "src/vmcontroller.h").read_text()
spec = (r / "rpm/harbour-sailqemu.spec").read_text()
assert 'QStringLiteral("-name") << launchName' in cpp
assert 'QStringLiteral("sailqemu-") + newId()' in cpp
assert 'QStringLiteral("runtime/qemuName")' in cpp
assert 'm_expectedQemuName = launchName;' in cpp
assert 'm_expectedQemuName.clear();' in cpp
assert 'QString m_expectedQemuName;' in h
assert 'bool m_qmpNameConfirmed = false;' in h
assert 'sendQmp(QStringLiteral("query-name"))' in cpp
assert 'qmpName == m_expectedQemuName' in cpp
assert 'name.toString() != m_expectedQemuName' in cpp
assert 'if (!m_qmpNameConfirmed || m_expectedQemuName.isEmpty())' in cpp
assert 'setDbusStatus(QStringLiteral("QMP disconnected; re-verifying VM identity..."))' in cpp
assert 'servicePid(QStringLiteral("org.qemu"))' not in cpp
assert 'QEMU PID is %2. Refusing to attach.' not in cpp
assert 'QStringLiteral("-uuid")' not in cpp
assert '<< m_expectedQemuName' in cpp
assert 'org.qemu.Display1.VM", "Name"' in listener
assert 'expectedName.constData()' in listener
assert 'GetConnectionUnixProcessID' not in listener
assert 'QEMU bus PID mismatch' not in listener
assert 'Release:    4' in spec
print('Sailjail PID-namespace VM identity regression: PASS')
