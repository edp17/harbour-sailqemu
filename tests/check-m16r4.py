#!/usr/bin/env python3
"""M16r4 read-only diagnostics menu and source packaging checks."""
from pathlib import Path
root = Path(__file__).resolve().parent.parent
qml = (root/'qml/pages/DiagnosticsPage.qml').read_text()
menu = qml.split('PullDownMenu {', 1)[1].split('        Column {', 1)[0]
assert menu.count('MenuItem {') == 2
assert 'text: "Refresh now"' in menu
assert 'text: "Copy report"' in menu
assert 'onClicked: diagnostics.refresh()' in menu
assert 'diagnostics.copyReport()' in menu
assert 'diagnostics.refresh()\n                    diagnostics.copyReport()' in menu
assert 'Button {' not in qml
assert 'VerticalScrollDecorator' in qml
assert 'Performance and runtime' in qml
assert 'VERSION 0.5.0' in (root/'CMakeLists.txt').read_text()
spec = (root/'rpm/harbour-sailqemu.spec').read_text()
assert 'Version:    0.5.0' in spec
assert 'Release:    4' in spec
assert 'Version 0.5.0-4' in (root/'qml/pages/AboutPage.qml').read_text()
assert (root/'docs/OBS-source-build-plan.md').exists()
print('M16r4 Diagnostics pulley actions and beta OBS planning: PASS')
