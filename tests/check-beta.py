#!/usr/bin/env python3
"""Static public-beta source and packaging consistency checks."""
from pathlib import Path
r = Path(__file__).resolve().parent.parent
cmake = (r / 'CMakeLists.txt').read_text()
spec = (r / 'rpm/harbour-sailqemu.spec').read_text()
about = (r / 'qml/pages/AboutPage.qml').read_text()
readme = (r / 'README.md').read_text()
assert 'project(harbour-sailqemu VERSION 0.5.0 ' in cmake
assert 'Version:    0.5.0' in spec
assert 'Release:    4' in spec
assert 'Version 0.5.0-4' in about
assert '0.5.0-4' in readme
assert 'Source0:    %{name}-%{version}.tar.gz' in spec
assert 'https://github.com/edp17/harbour-sailqemu' in spec
assert (r/'docs/BETA-0.5.0.md').exists()
assert (r/'docs/OBS-source-build-plan.md').exists()
assert not any(r.glob('**/*.rpm')), 'Do not commit RPM/binary distributions'
assert 'QEMU' in (r/'docs/BETA-0.5.0.md').read_text()
print('0.5.0 public beta source metadata: PASS')
