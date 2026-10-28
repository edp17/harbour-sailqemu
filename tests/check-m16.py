#!/usr/bin/env python3
"""M16 regression guard for Qt 5.6 source, QML, and process sampling.

Requires real Sailfish 5.1 SDK/on-device validation for a release.
"""
from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
h=(root/'src/vmcontroller.h').read_text()
c=(root/'src/vmcontroller.cpp').read_text()
rh=(root/'src/runtimediagnostics.h').read_text()
rc=(root/'src/runtimediagnostics.cpp').read_text()
dh=(root/'src/diskmanager.h').read_text()
dc=(root/'src/diskmanager.cpp').read_text()
d=(root/'qml/pages/DiskToolsPage.qml').read_text()
perf=(root/'qml/pages/DiagnosticsPage.qml').read_text()
main=(root/'qml/pages/MainPage.qml').read_text()
guest=(root/'qml/pages/GuestKeyboardPage.qml').read_text()
about=(root/'qml/pages/AboutPage.qml').read_text()
assert 'project(harbour-sailqemu VERSION 0.5.0' in (root/'CMakeLists.txt').read_text()
assert 'src/runtimediagnostics.cpp' in (root/'CMakeLists.txt').read_text()
assert 'Version:    0.5.0' in (root/'rpm/harbour-sailqemu.spec').read_text()
assert 'Release:    4' in (root/'rpm/harbour-sailqemu.spec').read_text()
assert 'Version 0.5.0-4' in about
assert 'RuntimeDiagnostics diagnostics(&vm);' in (root/'src/main.cpp').read_text()
assert 'setContextProperty(QStringLiteral("diagnostics"), &diagnostics)' in (root/'src/main.cpp').read_text()
assert 'Q_PROPERTY(QString report READ report NOTIFY changed)' in rh
assert 'void RuntimeDiagnostics::refresh()' in rc
assert 'm_timer.setInterval(2500)' in rc
assert 'row.lastIndexOf(\')\')' in rc
assert 'fields.at(11)' in rc and 'fields.at(12)' in rc
assert 'sysconf(_SC_CLK_TCK)' in rc
assert 'sysconf(_SC_PAGESIZE)' in rc
assert 'QFileInfo' in rc
assert 'lastLiveFrameMs()' in h
assert 'm_liveLastFrameMs = now;' in c
assert 'Observed framebuffer file changes' in rc
assert 'NOT presentation FPS' in rc
assert 'DiagnosticsPage.qml' in (root/'qml/pages/ProfilesPage.qml').read_text() and 'DiagnosticsPage.qml' in guest
assert 'diagnostics.report' in perf
assert 'onOperationCompleted: diskFlick.contentY = 0' in d
assert d.index('SectionHeader { text: "Operation" }') < d.index('SectionHeader { text: "Inspect existing image" }')
assert 'void operationCompleted();' in dh
assert dc.count('emit operationCompleted();') == 2
assert 'm_cancelRequested = true;' in dc
assert 'm_process.waitForFinished(1500);' not in dc.split('void DiskManager::cancel()',1)[1].split('void DiskManager::finish(',1)[0]
assert 'QDir(m_stage->path()).filePath' in dc
assert 'm_stage->filePath(' not in dc
assert 'm_resultPath.clear();' in dc.split('void DiskManager::fail',1)[1].split('bool DiskManager::begin',1)[0]
# Check simulated Linux /proc/self/stat line with spaces/parentheses in comm.
sample='123 (name with ) inside) S ' + ' '.join(str(n) for n in range(4,60))
rem=sample[sample.rfind(')')+1:].split()
assert rem[11]=='14' and rem[12]=='15'
# Ensure all QML files have braces balanced after stripping quoted strings and comments.
for qml in (root/'qml').rglob('*.qml'):
    text=qml.read_text()
    text=re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
    text=re.sub(r'//[^\n]*','',text)
    assert text.count('{')==text.count('}'), qml
print('M16 diagnostics, safe disk result, async cancellation, Qt 5.6 source checks: PASS')
