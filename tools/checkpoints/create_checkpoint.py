#!/usr/bin/env python3
"""ROM-free CipherNES full-source ZIP + reproducible SHA256/chunk manifest.

Usage:
  python3 tools/checkpoints/create_checkpoint.py --checkpoint CP006 --out /tmp/ciphernes-cp006
Only the archive and metadata will be written in --out, never uploaded automatically.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

MAX_BYTES = 50_000_000  # user-defined 50 MB (decimal), not 50 MiB
SKIP_DIRS = {'.git', '.svn', '__pycache__', 'build', 'Saves', 'saves', 'ROMs', 'roms', 'TestRoms', '.venv'}
SKIP_SUFFIXES = {'.nes', '.fds', '.qd', '.sav', '.7z', '.zip', '.obj', '.o', '.a', '.so', '.dll', '.exe', '.png', '.jpg', '.jpeg', '.iso', '.bin', '.part001'}

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for data in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(data)
    return h.hexdigest()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--checkpoint', required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--timestamp', default='')
    args = ap.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    if str(out).startswith(str(root) + '/'):
        ap.error('Checkpoint output must be OUTSIDE the source tree')
    out.mkdir(parents=True, exist_ok=True)
    timestamp = ('_' + args.timestamp) if args.timestamp else ''
    basename = 'CipherNES_' + args.checkpoint + timestamp + '_FULL_SOURCE.zip'
    target = out / basename
    paths = [p for p in root.rglob('*') if p.is_file() and not any(d in SKIP_DIRS for d in p.relative_to(root).parts[:-1])
             and p.suffix.lower() not in SKIP_SUFFIXES
             and '.part' not in p.name and not p.name.startswith('.DS_Store')]
    with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(paths):
            z.write(path, arcname='CipherNES-main/' + path.relative_to(root).as_posix())
    chunks = []
    if target.stat().st_size > MAX_BYTES:
        with target.open('rb') as source:
            n = 0
            while data := source.read(MAX_BYTES):
                n += 1
                part = out / (basename + '.part' + f'{n:03}')
                part.write_bytes(data)
                chunks.append({'file':part.name, 'bytes':len(data), 'sha256':sha256(part)})
        target.unlink()  # upload only parts, not duplicate oversized source
    else:
        chunks.append({'file':target.name,'bytes':target.stat().st_size,'sha256':sha256(target)})
    manifest = {'checkpoint':args.checkpoint, 'full_archive':basename, 'full_bytes':sum(p['bytes'] for p in chunks),
                'full_sha256':sha256(target) if target.exists() else None,
                'max_chunk_bytes':MAX_BYTES,'parts':chunks,
                'rom_free':True}
    if manifest['full_sha256'] is None:
        # Stream chunks without needing to reconstruct a potentially huge archive.
        h = hashlib.sha256()
        for part in chunks:
            with (out/part['file']).open('rb') as f:
                for data in iter(lambda:f.read(4*1024*1024),b''):
                    h.update(data)
        manifest['full_sha256'] = h.hexdigest()
    (out/'SOURCE_CHECKPOINT_PARTS.json').write_text(json.dumps(manifest, indent=2)+'\n')
    sums = '\n'.join(p['sha256']+'  '+p['file'] for p in chunks)+'\n'
    (out/('SOURCE_PART_SHA256SUMS.txt')).write_text(sums)
    print(json.dumps(manifest,indent=2))

if __name__=='__main__': main()
