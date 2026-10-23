#!/usr/bin/env python3
"""Created by Jacob Hodgkins. Smoke-test all iNES ROMs in an owned NTSC ZIP.

This is NOT a claim that a test ROM reported PASS on-screen: it verifies the
ROM loaded, executed for N frames, and passed a savestate round trip. Some
ROMs require human inspection, special hardware, or test-specific pass flags.
No ROM files are copied into the release ZIP or uploaded to CI.
"""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import zipfile


def run_one(args):
    exe, path, frames, runtime = args
    proc = subprocess.run([str(exe), '--validate', str(path), str(frames)],
                          env={**os.environ, 'CIPHERNES_RUNTIME_DIR':str(runtime)},
                          capture_output=True, text=True, timeout=max(30, frames))
    output = proc.stdout + proc.stderr
    return {'rom':str(path), 'success':proc.returncode == 0 and 'State round-trip: PASS' in output,
            'exit_code':proc.returncode,
            'framebuffer_crc32': (output.split('Framebuffer CRC32: ')[1].splitlines()[0] if 'Framebuffer CRC32: ' in output else None),
            'detail':output.strip()[-1200:] if proc.returncode else ''}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('zip',type=Path,help='NES test ROM tree .zip (provided locally, never redistributed)')
    p.add_argument('--frames',type=int,default=120)
    p.add_argument('--jobs',type=int,default=4)
    p.add_argument('--binary',type=Path,default=Path(__file__).resolve().parents[2]/'build/ubuntu/bin/CipherNES')
    p.add_argument('--json',type=Path,default=Path(__file__).resolve().parents[2]/'build/test_results/ntsc_corpus_smoke.json')
    options=p.parse_args()
    binary=options.binary.resolve()
    if not binary.is_file(): p.error('Build the Ubuntu frontend first')
    start=time.monotonic()
    with tempfile.TemporaryDirectory(prefix='ciphernes_ntsc_suite_') as tmp:
        root=Path(tmp)
        roms=[]
        with zipfile.ZipFile(options.zip) as arc:
            for name in arc.namelist():
                if not name.lower().endswith('.nes'): continue
                data=arc.read(name)
                if len(data)<16 or data[:4]!=b'NES\x1a':
                    print('SKIP: Invalid iNES header', name)
                    continue
                # Flatten with a deterministic unique name; do not trust archive paths.
                dest=root/('rom_%04d.nes'%len(roms))
                dest.write_bytes(data)
                roms.append((dest,name))
        print('iNES images:',len(roms),'frames per ROM:',options.frames,flush=True)
        results=[]
        runtime=root/'runtime'; runtime.mkdir()
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,options.jobs)) as pool:
            futures={pool.submit(run_one,(binary,rom,options.frames,runtime)):original for rom,original in roms}
            for index,future in enumerate(concurrent.futures.as_completed(futures),1):
                original=futures[future]
                try: result=future.result()
                except Exception as ex: result={'success':False,'exit_code':None,'detail':str(ex)}
                result['rom']=original
                results.append(result)
                if not result['success'] or index%30==0: print(index,'/',len(roms),original, 'PASS' if result['success'] else 'FAIL',flush=True)
    results.sort(key=lambda entry:entry['rom'])
    passed=sum(r['success'] for r in results)
    report={'note':'Smoke test only. No inference about each ROM\'s internal pass/fail result.','frames_per_rom':options.frames,
            'passed':passed,'total':len(results),'seconds':round(time.monotonic()-start,2),'results':results}
    options.json.parent.mkdir(exist_ok=True, parents=True)
    options.json.write_text(json.dumps(report,indent=2)+'\n')
    print('FINISHED',passed,'/',len(results),'boot, execute, state round-trip; elapsed',report['seconds'],'s',flush=True)
    print('REPORT',options.json,flush=True)
    return int(passed != len(results))

if __name__=='__main__':raise SystemExit(main())
