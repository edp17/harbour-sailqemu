#!/usr/bin/env python3
"""M12r2 rollback and lifecycle checks (static; verify on Sailfish)."""
from pathlib import Path
root = Path(__file__).resolve().parent.parent
helper = (root/'src/dbusdisplayhelper.cpp').read_text()
controller = (root/'src/vmcontroller.cpp').read_text()
header = (root/'src/vmcontroller.h').read_text()
qml = (root/'qml/pages/GuestKeyboardPage.qml').read_text()
main = (root/'qml/pages/MainPage.qml').read_text()
assert 'kFrameIntervalMs = 120' in helper
assert 'm_liveTimer.setInterval(200)' in controller
assert 'writePixels(s->frame, x, y, w, h' in helper
assert 'QImageWriter writer(&file, "png")' in helper
assert 'M12 SELF-TEST PASS' in helper
assert 'm_liveSampleFrames' in controller
assert 'QTemporaryDir m_liveFrameDir;' in header
assert 'return startVmInternal(false);' in controller
assert 'return startVmInternal(true);' in controller
assert 'QStringLiteral("-S")' not in controller
assert 'onStateChanged: page.checkVmExit()' in qml
assert 'if (!vm.running && pageStack.currentPage === page)' in qml
assert 'pageStack.pop()' in qml
assert 'onClicked: vm.paused ? vm.resumeVm() : vm.pauseVm()' in qml
assert 'id: frameA' not in qml and 'id: frameB' not in qml
assert 'source: vm.guestPreviewUrl' in qml
assert 'startVmWithDbusConsole()' in main
assert 'returnToMainTimer' in qml
print('M12r2 regression and lifecycle static checks: PASS')
