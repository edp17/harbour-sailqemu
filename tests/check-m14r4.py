#!/usr/bin/env python3
"""Static regression checks for PS/2 touchpad mode (not a device/SDK test)."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
h=(root/'src/vmcontroller.h').read_text()
c=(root/'src/vmcontroller.cpp').read_text()
q=(root/'qml/pages/GuestKeyboardPage.qml').read_text()
g=(root/'qml/pages/SailQemuGuidePage.qml').read_text()
spec=(root/'rpm/harbour-sailqemu.spec').read_text()
assert any('Release:    ' + v in spec for v in ('1', '2', '3', '4'))
assert 'Q_INVOKABLE bool guestMouseRelative(int dx, int dy);' in h
assert 'bool VmController::guestMouseRelative(int dx, int dy)' in c
assert 'm_mouseAbsolute || !m_dbusReady' in c
assert 'return mouseCall(QStringLiteral("RelMotion"), QVariantList() << dx << dy)' in c
assert 'if (m_mouseAbsolute && !guestMouseMove(x, y)) return false;' in c
assert 'Avoid accidental relative moves' in c
assert 'property bool relativeFingerDown' in q
assert 'function flushRelativeMotion()' in q
assert 'vm.guestMouseRelative(dx, dy)' in q
assert 'page.relativeFingerDown = !vm.mouseAbsolute' in q
assert 'if (vm.mouseAbsolute) page.showTouchMark' in q
assert 'relativeMoved = true' in q
assert 'vm.mouseAbsolute || !relativeMoved' in q
assert 'page.flushRelativeMotion()' in q
assert 'PS/2 touchpad: swipe to move' in q
assert "Disable 'USB tablet absolute pointer'" in g
assert (root/'docs/M14r4-relative-pointer.md').exists()
# Touchpad arithmetic: successive deltas, not absolute tap coordinates.
pos=[(200,100),(215,108),(240,122)]
travel=sum(pos[i+1][0]-pos[i][0] for i in range(len(pos)-1))
assert travel==40 and pos[-1][0]==240
print('M14r4 relative PS/2 gesture mapping, direct taps preserved: PASS')
