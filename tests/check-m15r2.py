#!/usr/bin/env python3
"""M15r2 Qt 5.6 source-compatibility regression guard (not an SDK build)."""
from pathlib import Path
r = Path(__file__).resolve().parent.parent
cpp = (r / "src/diskmanager.cpp").read_text()
assert "QDir(m_stage->path()).filePath(QStringLiteral(\"new-image\"))" in cpp
assert "m_stage->filePath(" not in cpp
assert "QTemporaryDir(parent.filePath(" in cpp
assert "QFile::rename(m_stagedPath, m_destination)" in cpp
assert "Release:    4" in (r / "rpm/harbour-sailqemu.spec").read_text()
print("M15r2 Qt 5.6 compatibility: PASS")
