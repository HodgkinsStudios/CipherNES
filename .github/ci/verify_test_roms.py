#!/usr/bin/env python3
"""Verify SHA-256 and inventory of intentionally tracked TEST_ROMS images."""
import csv
import hashlib
from pathlib import Path

root=Path(__file__).resolve().parents[2]/'TEST_ROMS'
manifest=root/'ROM_MANIFEST.csv'
with manifest.open(newline='',encoding='utf-8') as handle:
    rows=list(csv.DictReader(handle))
if not rows:
    raise SystemExit('ERROR: empty TEST_ROMS manifest')
expected=set()
counts={}
bad=[]
for row in rows:
    rel=Path(row['relative_path'])
    if rel.is_absolute() or '..' in rel.parts or rel.as_posix() in expected:
        bad.append('unsafe or repeated path: '+str(rel))
        continue
    expected.add(rel.as_posix())
    p=root/rel
    if not p.is_file():
        bad.append('missing: '+rel.as_posix())
        continue
    data=p.read_bytes()
    digest=hashlib.sha256(data).hexdigest()
    if digest!=row['sha256'] or len(data)!=int(row['bytes']):
        bad.append('hash/size mismatch: '+rel.as_posix())
    if p.suffix.lower()=='.nes' and ('YES' if len(data)>=16 and data[:4]==b'NES\x1a' else 'NO')!=row['valid_ines_header']:
        bad.append('iNES classification mismatch: '+rel.as_posix())
    kind=p.suffix.lower().lstrip('.')
    counts[kind]=counts.get(kind,0)+1
actual={p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file() and p.suffix.lower() in {'.nes','.fds','.nsf','.unif'}}
if actual!=expected:
    bad.append(f'image inventory differs: unexpected={sorted(actual-expected)}, absent={sorted(expected-actual)}')
for issue in bad:
    print('ERROR:',issue)
print(f'TEST_ROMS images checked: {len(rows)}, types: {counts}, problems: {len(bad)}')
if bad:
    raise SystemExit(1)
