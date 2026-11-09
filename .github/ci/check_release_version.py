#!/usr/bin/env python3
"""Validate release metadata and CI artifacts against the VERSION file."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
version = (root / 'VERSION').read_text(encoding='utf-8').strip()
issues = []
if not re.fullmatch(r'\d+\.\d+\.\d+', version):
    issues.append(f'Invalid semantic version: {version!r}')
for location in ('appimage', 'flatpak'):
    path = root / 'packaging' / location / 'io.github.hodgkinsstudios.CipherNES.metainfo.xml'
    content = path.read_text(encoding='utf-8')
    if f'<release version="{version}"' not in content:
        issues.append(f'Missing current release metadata: {path}')
for path in (root / '.github/workflows').glob('*.yml'):
    for match in re.finditer(r'CipherNES-(\d+\.\d+\.\d+)-', path.read_text(encoding='utf-8')):
        if match.group(1) != version:
            issues.append(f'Stale filename in {path}: {match.group(0)}')
if not (root / f'RELEASE_NOTES_{version}.md').is_file():
    issues.append(f'Missing RELEASE_NOTES_{version}.md')
for issue in issues:
    print('FAIL:', issue, file=sys.stderr)
print('release version:', version, 'PASS' if not issues else 'FAILED')
sys.exit(bool(issues))
