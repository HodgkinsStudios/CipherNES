#!/usr/bin/env python3
"""Run the complete NTSC-only test ROM corpus without implying internal accuracy verdicts.

The suite scope manifest is a fixed inventory of the 290 user-supplied NES entries.
PAL images and invalid headers are excluded by exact archive path. Each included
ROM must match its SHA-256 before running. On completion, the 600-frame smoke
report is joined with historical internal grades from documented earlier runs;
those historical grades are NOT treated as freshly rerun internal verdicts.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

REPO=Path(__file__).resolve().parents[2]
MANIFEST=REPO/'tests/ntsc_rom_scope.csv'

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('corpus',type=Path,help='Path to TEST_ROMS/nes-test-roms or original NES ROM suite ZIP')
    ap.add_argument('--frames',type=int,default=600)
    ap.add_argument('--jobs',type=int,default=4)
    ap.add_argument('--timeout',type=int,default=150)
    ap.add_argument('--output',type=Path,default=REPO/'build/test_results/ntsc_baseline.json')
    ap.add_argument('--binary',type=Path,default=REPO/'build/ubuntu/bin/CipherNES')
    opts=ap.parse_args()
    if opts.frames<1:ap.error('--frames must be >= 1')
    if opts.jobs<1:ap.error('--jobs must be >= 1')
    if not opts.binary.is_file():ap.error('Build the Ubuntu frontend first')
    if not MANIFEST.is_file():ap.error('Missing tests/ntsc_rom_scope.csv')
    entries=list(csv.DictReader(MANIFEST.open(newline='')))
    scopes={x:sum(r['region_scope']==x for r in entries) for x in ('NTSC','PAL_EXCLUDED','INVALID_INES')}
    if len(entries)!=290 or scopes != {'NTSC':274,'PAL_EXCLUDED':15,'INVALID_INES':1}:
        ap.error(f'Unexpected suite scope inventory: {scopes}, total={len(entries)}')
    romroot=opts.corpus.resolve()
    iszip=romroot.is_file() and zipfile.is_zipfile(romroot)
    if not iszip and not romroot.is_dir():ap.error('Corpus must be directory or valid zip')
    with tempfile.TemporaryDirectory(prefix='ciphernes_ntsc_') as td:
        selection=Path(td)/'ntsc'
        selection.mkdir()
        zf=zipfile.ZipFile(romroot) if iszip else None
        try:
            for row in entries:
                rel=Path(row['rom_path'])
                if rel.is_absolute() or '..' in rel.parts: raise ValueError('Unsafe relative ROM path')
                if zf:
                    # User-supplied ZIP uses the archive's nes-test-roms-master/ root.
                    data=zf.read('nes-test-roms-master/'+rel.as_posix())
                else:
                    source=romroot/rel
                    data=source.read_bytes()
                digest=hashlib.sha256(data).hexdigest()
                if digest.lower() != row['rom_sha256'].lower():
                    raise ValueError('ROM hash mismatch: '+str(rel))
                if row['region_scope']=='NTSC':
                    if len(data)<16 or data[:4]!=b'NES\x1a':
                        raise ValueError('NTSC ROM has invalid iNES header: '+str(rel))
                    dest=selection/rel; dest.parent.mkdir(parents=True,exist_ok=True)
                    if zf:dest.write_bytes(data)
                    else:dest.symlink_to(source)
            output=opts.output.resolve(); output.parent.mkdir(parents=True,exist_ok=True)
            cmd=[sys.executable,str(REPO/'.github/ci/run_ntsc_corpus.py'),str(selection),'--frames',str(opts.frames),'--jobs',str(opts.jobs),'--timeout',str(opts.timeout),'--binary',str(opts.binary.resolve()),'--json',str(output)]
            print('Scope: 274 NTSC, 15 PAL excluded, 1 invalid excluded.',flush=True)
            print('Verifying hashes, then executing',opts.frames,'frames for every NTSC ROM.',flush=True)
            result=subprocess.run(cmd)
        finally:
            if zf:zf.close()
    if not opts.output.exists():return result.returncode or 1
    report=json.loads(opts.output.read_text())
    expected={r['rom_path']:r for r in entries if r['region_scope']=='NTSC'}
    output_by_name={r['rom']:r for r in report['results']}
    if set(expected)!=set(output_by_name):
        print('ERROR: Runner results do not match the scope inventory',file=sys.stderr)
        return 1
    enriched=[]
    for name in sorted(expected):
        row=expected[name]; test=output_by_name[name]
        enriched.append({
            'rom_path':name,'mapper':row['mapper'],'sha256':row['rom_sha256'],
            'frames_requested':opts.frames,
            'execution_save_state': 'PASS' if test['success'] else 'FAIL',
            'framebuffer_crc32':test.get('framebuffer_crc32'),
            'exit_code':test.get('exit_code'),
            'diagnostics':test.get('detail',''),
            'historical_internal_grade_NOT_RERUN':row['historical_internal_grade'],
            'historical_disposition_NOT_RERUN':row['historical_disposition'],
        })
    report.update({
        'scope':'NTSC NES test ROMs only. Excludes PAL and invalid iNES.',
        'inventory_total':290,'ntsc_test_roms':274,'pal_excluded':15,'invalid_ines_excluded':1,
        'emulator_sha256':hashlib.sha256(opts.binary.read_bytes()).hexdigest(),
        'manifest_sha256':hashlib.sha256(MANIFEST.read_bytes()).hexdigest(),
        'historical_internal_verdict_caution':'Historical CP021 grades are not freshly rerun PASS/FAIL results. No internal verdict inferred from smoke execution.',
        'historical_verified_passes':sum(r['historical_internal_grade_NOT_RERUN']=='PASS' for r in enriched),
        'historical_visual_mismatches':sum(r['historical_internal_grade_NOT_RERUN']=='VISUAL_MISMATCH' for r in enriched),
        'ntsc_results':enriched,
    })
    opts.output.write_text(json.dumps(report,indent=2)+'\n')
    csv_path=opts.output.with_suffix('.csv')
    with csv_path.open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(enriched[0]))
        writer.writeheader();writer.writerows(enriched)
    print('BASELINE: ',report['passed'],'/',274,'NTSC ROMs executed + state restoration',flush=True)
    print('RESULT JSON:',opts.output,'CSV:',csv_path,flush=True)
    print('HISTORICAL internal grades, not regraded: ',report['historical_verified_passes'],'PASS,',report['historical_visual_mismatches'],'visual mismatch',flush=True)
    return result.returncode

if __name__=='__main__': raise SystemExit(main())
