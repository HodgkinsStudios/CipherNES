#!/usr/bin/env python3
# Created by Jacob Hodgkins
"""Fail CI when package/workflow version references drift from VERSION."""
from pathlib import Path
import re, sys
root = Path(__file__).resolve().parents[2]
version = (root / 'VERSION').read_text().strip()
failures = []
if version != '1.0.1':
    failures.append(f'Unexpected source VERSION: {version!r}')
for name in ('appimage', 'flatpak'):
    p = root / 'packaging' / name / 'io.github.hodgkinsstudios.CipherNES.metainfo.xml'
    s = p.read_text()
    if f'<release version="{version}"' not in s:
        failures.append(f'{p} does not list current release')
for p in (root / '.github/workflows').glob('*.yml'):
    for match in re.finditer(r'CipherNES-(\d+\.\d+\.\d+)-', p.read_text()):
        if match.group(1) != version:
            failures.append(f'{p} contains older package filename: {match.group(0)}')
for p in ('package_release.sh', 'package_release_windows.ps1'):
    s = (root / p).read_text()
    if 'RELEASE_NOTES_1.0.0.md' in s:
        failures.append(f'{p} packages old release notes')
for failure in failures:
    print('FAIL:', failure, file=sys.stderr)
print('release version check:', version, 'OK' if not failures else 'FAILED')
sys.exit(bool(failures))
