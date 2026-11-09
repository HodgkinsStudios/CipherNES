#!/usr/bin/env python3
"""Verify chunks, reconstruct if needed, then verify final ZIP SHA256.

Usage: python3 tools/checkpoints/reassemble_source.py /path/to/checkpoint-folder
"""
import hashlib, json, pathlib, sys

def hashfile(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(4*1024*1024),b''):h.update(b)
    return h.hexdigest()

def main():
    folder=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else '.').resolve()
    manifest=json.loads((folder/'SOURCE_CHECKPOINT_PARTS.json').read_text())
    target=folder/manifest['full_archive']
    parts=manifest['parts']
    for part in parts:
        p=folder/part['file']
        if not p.is_file() or p.stat().st_size!=part['bytes'] or hashfile(p)!=part['sha256']:
            raise SystemExit('Missing or invalid part: '+str(p))
    if len(parts)!=1 or parts[0]['file']!=manifest['full_archive']:
        with target.open('wb') as dest:
            for part in parts:
                with (folder/part['file']).open('rb') as f:
                    for block in iter(lambda:f.read(4*1024*1024),b''):dest.write(block)
    if target.stat().st_size!=manifest['full_bytes'] or hashfile(target)!=manifest['full_sha256']:
        raise SystemExit('Assembled full-source SHA256 does not match manifest')
    print('VERIFIED',target,manifest['full_sha256'])

if __name__=='__main__': main()
