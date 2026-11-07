#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Build actual bsdtar with UC2 enabled in a private copy of libarchive."""
import argparse
from pathlib import Path
import shutil
import subprocess

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--source', type=Path, required=True, help='unmodified libarchive source')
ap.add_argument('--build', type=Path, required=True, help='new working directory')
ap.add_argument('--jobs', type=int, default=4)
args = ap.parse_args()
root = Path(__file__).resolve().parents[2]
work = args.build.resolve()
work.mkdir(parents=True, exist_ok=False)
source = work / 'source'
shutil.copytree(args.source, source)
p = source/'libarchive/archive_read_private.h'
p.write_text(p.read_text().replace('formats[16]', 'formats[17]'))
p = source/'libarchive/archive_read_support_format_all.c'
s = p.read_text()
anchor = '\tarchive_read_support_format_ar(a);'
if anchor not in s:
    raise SystemExit('Unsupported libarchive registration layout')
s = s.replace(anchor, '\tarchive_read_support_format_uc2(a);\n'+anchor, 1)
s = s.replace('#include "archive.h"',
              '#include "archive.h"\nint archive_read_support_format_uc2(struct archive *);', 1)
p.write_text(s)
with (source/'CMakeLists.txt').open('a') as f:
    f.write(f'''
# UC2 adapter: local opt-in build; the linked distribution includes GPL/LGPL code.
set(UC2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(UC2_BUILD_LIBARCHIVE_PLUGIN OFF CACHE BOOL "" FORCE)
set(UC2_BUILD_WCX OFF CACHE BOOL "" FORCE)
add_subdirectory("{root.as_posix()}" uc2-source EXCLUDE_FROM_ALL)
target_sources(archive_static PRIVATE "{(root/'contrib/libarchive/archive_read_support_format_uc2.c').as_posix()}")
target_link_libraries(archive_static uc2)
''')
config = ['cmake', '-S', str(source), '-B', str(work/'build'),
          '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_SHARED_LIBS=OFF',
          '-DENABLE_TEST=OFF', '-DENABLE_TAR=ON', '-DENABLE_TAR_SHARED=OFF',
          '-DENABLE_CPIO=OFF', '-DENABLE_CAT=OFF', '-DENABLE_UNZIP=OFF',
          '-DENABLE_WERROR=OFF', '-DENABLE_INSTALL=OFF']
subprocess.run(config, check=True)
subprocess.run(['cmake', '--build', str(work/'build'), '--target', 'bsdtar',
                '-j', str(args.jobs)], check=True)
print('bsdtar:', work/'build/bin/bsdtar')
