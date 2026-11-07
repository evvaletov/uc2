#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the static browser reader with Emscripten 4.0.15 or newer."""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / 'web/dist')
    parser.add_argument('--emcc', default='emcc')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    data = (ROOT / 'lib/src/super.bin').read_bytes()
    exports = ['uc2_web_open', 'uc2_web_close', 'uc2_web_name', 'uc2_web_field',
               'uc2_web_extract', 'uc2_web_output', 'uc2_web_free_output', 'uc2_web_error']
    with tempfile.TemporaryDirectory(prefix='uc2-web-build-') as temporary:
        super_c = Path(temporary) / 'super_data.c'
        super_c.write_text('unsigned char uc2_supermaster_compressed[] = {' +
                           ','.join(str(x) for x in data) + '};\n' +
                           f'const unsigned int uc2_supermaster_compressed_size = {len(data)};\n')
        command = [args.emcc, '-O2', '-DNDEBUG', '-DUC2_SUPER_C', '-std=c99',
                   '-I' + str(ROOT / 'lib/include'), '-I' + str(ROOT / 'lib/src'),
                   str(ROOT / 'web/bridge.c'), str(ROOT / 'lib/src/decompress.c'),
                   str(ROOT / 'lib/src/uc2_tables.c'), str(ROOT / 'lib/src/uc2_rans.c'), str(super_c),
                   '-sMODULARIZE=1', '-sEXPORT_NAME=createUC2', '-sENVIRONMENT=worker',
                   '-sALLOW_MEMORY_GROWTH=1', '-sMAXIMUM_MEMORY=268435456',
                   '-sSTACK_SIZE=1048576', '-sABORTING_MALLOC=0', '-sFILESYSTEM=1',
                   '-sEXPORTED_FUNCTIONS=' + json.dumps(['_' + x for x in exports]),
                   '-sEXPORTED_RUNTIME_METHODS=["FS","WORKERFS","UTF8ToString","HEAPU8"]',
                   '-lworkerfs.js', '-o', str(out / 'uc2-core.js')]
        env = os.environ.copy()
        for name in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'OBJC_INCLUDE_PATH', 'LIBRARY_PATH'):
            env.pop(name, None)
        subprocess.run(command, check=True, env=env)
    for name in ('index.html', 'app.js', 'worker.js', 'style.css'):
        shutil.copyfile(ROOT / 'web' / name, out / name)
    for name in ('LICENSE', 'CREDITS.md'):
        shutil.copyfile(ROOT / name, out / name)
    shutil.copyfile(ROOT / 'docs/license-audit.md', out / 'license-audit.md')
    shutil.copytree(ROOT / 'LICENSES', out / 'LICENSES', dirs_exist_ok=True)
    (out / 'examples').mkdir(exist_ok=True)
    shutil.copyfile(ROOT / 'tests/archives/basic.uc2', out / 'examples/basic.uc2')
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    (out / 'BUILD-INFO.json').write_text(json.dumps({
        'source': 'https://github.com/evvaletov/uc2',
        'commit': commit,
        'source_archive': f'https://github.com/evvaletov/uc2/archive/{commit}.tar.gz',
        'compiler': subprocess.check_output([args.emcc, '--version'], text=True).splitlines()[0],
    }, indent=2) + '\n')
    print(out)


if __name__ == '__main__':
    main()
