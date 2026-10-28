#!/usr/bin/env python3
"""Focused M12r2 source-level invariants. Runtime tests require a device."""
from pathlib import Path
import re
r=Path(__file__).resolve().parent.parent
c=(r/'src/vmcontroller.cpp').read_text()
q=(r/'qml/pages/GuestKeyboardPage.qml').read_text()
for method, expected in [('startVm', 'return startVmInternal(false);'),
                         ('startVmWithConsole', 'return startVmInternal(false);'),
                         ('startVmWithDbusConsole', 'return startVmInternal(true);')]:
    match = re.search(r'bool VmController::'+method+r'\(\)\s*\{([^}]+)\}', c)
    assert match and expected in match.group(1), method
assert 'QStringLiteral("-S")' not in c
# No automatic pause and no accidental pop on a shutdown command/STOP event.
assert 'resetStopped(' in c and 'setRunning(false, false);' in c
assert 'onStateChanged: page.checkVmExit()' in q
assert 'else if (sawRunningVm && !returningAfterStop)' in q
assert 'if (!vm.running && pageStack.currentPage === page)' in q
assert 'onClicked: vm.paused ? vm.resumeVm() : vm.pauseVm()' in q
assert 'id: frameA' not in q and 'id: frameB' not in q
print('M12r2 immediate-boot / auto-return invariants: PASS')
