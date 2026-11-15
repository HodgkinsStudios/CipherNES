#!/usr/bin/env python3
"""Deterministic source-based diagnostic capture, no modification of the release source tree.

The ROM is read at its supplied external path. ROM bytes are never copied into
project folders, reports or checkpoints. Default input: none. Optionally inject
START at an explicit frame range via --start-at; never silently press buttons.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image


def run():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('rom', type=Path)
    ap.add_argument('--probe', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--frames', default='60,240,600', help='Comma-separated checkpoints; optionally 1800,3600')
    ap.add_argument('--start-at', type=int, help='START on port 1 for six frames, starting at this frame')
    ap.add_argument('--input-schedule', type=Path, help='Explicit CSV (start_frame,end_frame,port1_mask,port2_mask)')
    ap.add_argument('--repeat', action='store_true', help='Repeat the exact run in a clean runtime and compare checkpoint CRCs')
    args = ap.parse_args()
    rom, probe = args.rom.resolve(), args.probe.resolve()
    if not rom.is_file() or not probe.is_file():ap.error('ROM or diagnostic probe not found')
    if args.start_at and args.input_schedule:ap.error('Use either --start-at or --input-schedule')
    if args.start_at is not None and args.start_at<1:ap.error('--start-at must be >= 1')
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    digest=hashlib.file_digest(rom.open('rb'),'sha256').hexdigest()
    if args.input_schedule:
        script=args.input_schedule.read_text(encoding='utf-8')
    elif args.start_at:
        script='start_frame,end_frame,port1_mask,port2_mask\n%d,%d,0x10,0x00\n' % (args.start_at, args.start_at+5)
    else:
        script='start_frame,end_frame,port1_mask,port2_mask\n'
    (output/'input_schedule.csv').write_text(script,encoding='utf-8')

    def invoke(out):
        out.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ciphernes_capture_runtime_') as runroot:
            env={**os.environ,'CIPHERNES_RUNTIME_DIR':runroot,
                 'HOME':runroot,'XDG_DATA_HOME':runroot,'XDG_CONFIG_HOME':runroot,
                 'CIPHERNES_DATACH_SAVE_PATH':str(Path(runroot)/'datach.sav')}
            p=subprocess.run([str(probe),str(rom),str(out),args.frames,str(output/'input_schedule.csv')],
                             env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,text=True,timeout=480)
        (out/'probe.log').write_text(p.stdout+'\n'+p.stderr,encoding='utf-8')
        if p.returncode != 0: raise RuntimeError(f'diagnostic exited {p.returncode}; see {out}/probe.log')
        with (out/'checkpoints.csv').open(newline='') as f: rows=list(csv.DictReader(f))
        for row in rows:
            fpath=out/row['screenshot_ppm']
            png=fpath.with_suffix('.png')
            with Image.open(fpath) as im: im.save(png,format='PNG')
            fpath.unlink()
            row['screenshot_png']=png.name
        with (out/'checkpoints.csv').open('w',newline='') as f:
            writer=csv.DictWriter(f,fieldnames=[k for k in rows[0] if k!='screenshot_ppm'])
            writer.writeheader();writer.writerows([{k:v for k,v in r.items() if k!='screenshot_ppm'} for r in rows])
        with (out/'replay.csv').open() as f: replay=list(csv.DictReader(f))[0]
        return rows,replay

    first, replay=invoke(output)
    result={'rom_filename':rom.name,'rom_sha256':digest,'checkpoint_count':len(first),
            'frames':args.frames,'start_at':args.start_at,'transient_state_replay':replay['verdict'],
            'independent_rerun':'NOT_REQUESTED'}
    if args.repeat:
        second, replay2=invoke(output/'independent_repeat')
        result['independent_rerun']='PASS' if [(r['frame'],r['crc32_argb_hex'],r['master_ticks_total']) for r in first] == [(r['frame'],r['crc32_argb_hex'],r['master_ticks_total']) for r in second] else 'FAIL'
        result['repeat_transient_state_replay']=replay2['verdict']
    (output/'run_metadata.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    if replay['verdict']!='PASS' or result['independent_rerun']=='FAIL': return 4
    return 0
if __name__=='__main__':raise SystemExit(run())
