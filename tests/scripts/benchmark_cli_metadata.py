#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Opt-in CLI listing timings for flat names, directories and file versions.

Run baseline and candidate against the same --work directory; outputs include
listing hashes. Uses the installed UC2Writer only to create missing fixtures.
"""
import argparse, hashlib, json, subprocess, time
from pathlib import Path
from uc2 import UC2Writer

if not __debug__:
    raise RuntimeError('validation requires Python without -O or PYTHONOPTIMIZE')

parser=argparse.ArgumentParser()
parser.add_argument('--uc2',required=True)
parser.add_argument('--label',required=True)
parser.add_argument('--work',type=Path,required=True)
parser.add_argument('--timeout',type=float,default=120)
parser.add_argument('--sizes',default='10000,50000,100000')
args=parser.parse_args()
root=args.work;root.mkdir(exist_ok=True,parents=True)
results=[]
for kind in ['flat','directories','versions']:
    for count in map(int,args.sizes.split(',')):
        archive=root/f'{kind}-{count}.uc2'
        if not archive.exists():
            with UC2Writer(archive,level=2,max_entries=count*2+10) as writer:
                for i in range(count):
                    name=f'file-{i:06d}' if kind=='flat' else (f'dir-{i:06d}/file' if kind=='directories' else 'versioned')
                    writer.writestr(name,b'')
        started=time.monotonic()
        try:
            r=subprocess.run([args.uc2,'-qalT',str(archive)],capture_output=True,timeout=args.timeout)
            assert r.returncode==0,r.stderr
            lines=r.stdout.splitlines()
            assert len(lines)==count*(2 if kind=='directories' else 1),(kind,count,len(lines))
            result={'kind':kind,'entries':len(lines),'requested_files':count,'seconds':time.monotonic()-started,
                    'listing_sha256':hashlib.sha256(r.stdout).hexdigest(),'archive_bytes':archive.stat().st_size}
        except subprocess.TimeoutExpired:
            result={'kind':kind,'requested_files':count,'timeout_seconds':args.timeout}
        results.append(result)
        (root/(args.label+'.json')).write_text(json.dumps(results,indent=2)+'\n')
        print(json.dumps(result),flush=True)
