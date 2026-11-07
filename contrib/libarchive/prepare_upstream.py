#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Prepare a reviewable optional-reader patch against libarchive 3.8.11."""
import argparse
import binascii
import difflib
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("--source", type=Path, required=True, help="unmodified libarchive 3.8.11")
ap.add_argument("--uc2", type=Path, required=True, help="tested fixture generator")
ap.add_argument("--output", type=Path, required=True, help="new review directory")
args = ap.parse_args()
repo = Path(__file__).resolve().parents[2]
original = args.source.resolve()
work = args.output.resolve()
work.mkdir(parents=True, exist_ok=False)
source = work / "libarchive"
shutil.copytree(original, source)
changed = set()


def write(name, text):
    path = source / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    changed.add(name)


def replace(name, old, new):
    text = (source / name).read_text()
    if text.count(old) != 1:
        raise SystemExit(f"unexpected libarchive layout: {name}: {old!r}")
    write(name, text.replace(old, new, 1))


header = (source / "libarchive/archive.h").read_text()
if "ARCHIVE_VERSION_NUMBER 3008011" not in header or "ARCHIVE_FORMAT_UC2" in header:
    raise SystemExit("expected unmodified libarchive 3.8.11 headers")
replace("libarchive/archive.h", "#define\tARCHIVE_FORMAT_RAR_V5\t\t\t0x100000",
        "#define\tARCHIVE_FORMAT_RAR_V5\t\t\t0x100000\n"
        "/* Proposed allocation; subject to maintainer review. */\n"
        "#define\tARCHIVE_FORMAT_UC2\t\t\t0x110000")
replace("libarchive/archive.h", "__LA_DECL int archive_read_support_format_all(struct archive *);",
        "__LA_DECL int archive_read_support_format_uc2(struct archive *);\n"
        "__LA_DECL int archive_read_support_format_all(struct archive *);")
replace("libarchive/archive_read_private.h", "formats[16]", "formats[17]")
replace("libarchive/archive_read_support_format_all.c", "\tarchive_read_support_format_ar(a);",
        "\tarchive_read_support_format_uc2(a);\n\tarchive_read_support_format_ar(a);")
replace("libarchive/archive_read_support_format_by_code.c", "\tcase ARCHIVE_FORMAT_RAW:",
        "\tcase ARCHIVE_FORMAT_UC2:\n\t\treturn archive_read_support_format_uc2(a);\n\tcase ARCHIVE_FORMAT_RAW:")
replace("libarchive/archive_read_set_format.c", "    case ARCHIVE_FORMAT_RAW:",
        '    case ARCHIVE_FORMAT_UC2:\n      str = "uc2";\n      break;\n    case ARCHIVE_FORMAT_RAW:')

adapter = (repo / "contrib/libarchive/archive_read_support_format_uc2.c").read_text()
adapter = adapter.replace("#include <uc2/libuc2.h>", '''#if !defined(HAVE_LIBUC2READ)
int
archive_read_support_format_uc2(struct archive *a)
{
    archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
        "archive_read_support_format_uc2");
    archive_set_error(a, ARCHIVE_ERRNO_MISC, "UC2 support was not enabled in this build");
    return ARCHIVE_WARN;
}
#else
#include <uc2/libuc2.h>''', 1)
adapter += "\n#endif /* HAVE_LIBUC2READ */\n"
# The submitted source uses the user's approved BSD option. The repository
# source keeps its dual SPDX expression; the full notice is retained here.
notice = (repo / "contrib/libarchive/LICENSE.BSD-2-Clause").read_text()
adapter = ("/* SPDX-License-Identifier: BSD-2-Clause */\n/*\n * " +
           notice.rstrip().replace("\n", "\n * ") + "\n */\n" +
           "\n".join(adapter.splitlines()[2:]) + "\n")
write("libarchive/archive_read_support_format_uc2.c", "\n".join(line.rstrip() for line in adapter.splitlines()) + "\n")
replace("libarchive/CMakeLists.txt", "  archive_read_support_format_7zip.c",
        "  archive_read_support_format_uc2.c\n  archive_read_support_format_7zip.c")
replace("Makefile.am", "\tlibarchive/archive_read_support_format_7zip.c \\",
        "\tlibarchive/archive_read_support_format_uc2.c \\\n\tlibarchive/archive_read_support_format_7zip.c \\")
replace("CMakeLists.txt", 'SET(ADDITIONAL_LIBS "")', '''SET(ADDITIONAL_LIBS "")

# UC2 is opt-in: libuc2read is an external LGPL-3.0-only shared reader.
OPTION(ENABLE_UC2READ "Enable legacy UC2 through the LGPL libuc2read dependency" OFF)
IF(ENABLE_UC2READ)
  FIND_PATH(UC2READ_INCLUDE_DIR uc2/libuc2.h)
  FIND_LIBRARY(UC2READ_LIBRARY NAMES uc2read)
  IF(NOT UC2READ_INCLUDE_DIR OR NOT UC2READ_LIBRARY)
    MESSAGE(FATAL_ERROR "ENABLE_UC2READ requires the external libuc2read shared reader")
  ENDIF()
  IF(UC2READ_LIBRARY MATCHES "[.]a$" AND NOT UC2READ_LIBRARY MATCHES "[.]dll[.]a$")
    MESSAGE(FATAL_ERROR "UC2 requires a shared libuc2read, not a static archive")
  ENDIF()
  INCLUDE(CheckCSourceCompiles)
  SET(CMAKE_REQUIRED_INCLUDES ${UC2READ_INCLUDE_DIR})
  SET(CMAKE_REQUIRED_LIBRARIES ${UC2READ_LIBRARY})
  SET(CMAKE_REQUIRED_DEFINITIONS -DUC2_SHARED -DUC2_HUFFMAN_ONLY)
  UNSET(UC2READ_LINKS CACHE)
  CHECK_C_SOURCE_COMPILES("#include <uc2/libuc2.h>
int main(void) { struct uc2_io io = {0}; uc2_handle h = uc2_open(&io, 0); uc2_close(h); return 0; }" UC2READ_LINKS)
  UNSET(CMAKE_REQUIRED_INCLUDES)
  UNSET(CMAKE_REQUIRED_LIBRARIES)
  UNSET(CMAKE_REQUIRED_DEFINITIONS)
  IF(NOT UC2READ_LINKS)
    MESSAGE(FATAL_ERROR "libuc2read headers and required reader symbols do not link")
  ENDIF()
  SET(HAVE_LIBUC2READ 1)
  INCLUDE_DIRECTORIES(${UC2READ_INCLUDE_DIR})
  ADD_DEFINITIONS(-DUC2_SHARED -DUC2_HUFFMAN_ONLY)
  LIST(APPEND ADDITIONAL_LIBS ${UC2READ_LIBRARY})
ENDIF()''')
write("build/cmake/config.h.in", (source / "build/cmake/config.h.in").read_text() +
      "\n/* Optional legacy UC2 reader. */\n#cmakedefine HAVE_LIBUC2READ 1\n")
replace("configure.ac", "AC_ARG_WITH([lz4],", '''AC_ARG_WITH([uc2read],
  AS_HELP_STRING([--with-uc2read], [Enable legacy UC2 via LGPL libuc2read]),
  [], [with_uc2read=no])
if test "x$with_uc2read" = "xyes"; then
  PKG_CHECK_MODULES(UC2READ, [libuc2read >= 3.0.0])
  dnl The published dependency is shared-only. Refuse explicit static overrides.
  for uc2read_flag in $UC2READ_LIBS; do
    AS_CASE([$uc2read_flag], [*.dll.a], [], [*.a|-static|-Wl,-Bstatic],
      [AC_MSG_ERROR([UC2 requires the shared libuc2read dependency])])
  done
  CPPFLAGS="${CPPFLAGS} ${UC2READ_CFLAGS}"
  LIBS="${LIBS} ${UC2READ_LIBS}"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <uc2/libuc2.h>]],
    [[struct uc2_io io = {0}; uc2_handle h = uc2_open(&io, 0); uc2_close(h);]])],
    [], [AC_MSG_ERROR([libuc2read reader API does not link])])
  LIBSREQUIRED="$LIBSREQUIRED${LIBSREQUIRED:+ }libuc2read"
  AC_DEFINE(HAVE_LIBUC2READ, [1], [Define to 1 for the optional LGPL UC2 reader.])
fi

AC_ARG_WITH([lz4],''')

fixtures = work / "fixtures"
fixtures.mkdir()
(fixtures / "fixture.txt").write_bytes(b"UC2 optional reader\n" * 100)
os.utime(fixtures / "fixture.txt", (946684800, 946684800))
for level in (4, 6, 80):
    name = f"test_read_format_uc2_{level}.uc2"
    archive = fixtures / name
    if level == 80:
        shutil.copyfile(repo / "tests/archives/turbo0.uc2", archive)
    else:
        subprocess.run([str(args.uc2.resolve()), "-qw", "-L", str(level), str(archive), "fixture.txt"],
                       cwd=fixtures, check=True)
    data = archive.read_bytes()
    encoded = b"begin 644 " + name.encode() + b"\n"
    encoded += b"".join(binascii.b2a_uu(data[i:i+45], backtick=True) for i in range(0, len(data), 45))
    encoded += b"`\nend\n"
    write("libarchive/test/" + name + ".uu", encoded.decode())

test = '''/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2026 Eremey Valetov. */
#include "test.h"

DEFINE_TEST(test_read_format_uc2)
{
    struct archive *a = archive_read_new();
    int result = archive_read_support_format_uc2(a);
#ifndef HAVE_LIBUC2READ
    assertEqualInt(ARCHIVE_WARN, result);
    assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#else
    struct archive_entry *e;
    char data[2000];
    assertEqualInt(ARCHIVE_OK, result);
    /* Keep room for explicit raw fallback after registering every format. */
    struct archive *all = archive_read_new();
    assertEqualInt(ARCHIVE_OK, archive_read_support_format_all(all));
    assertEqualIntA(all, ARCHIVE_OK, archive_read_support_format_raw(all));
    assertEqualInt(ARCHIVE_OK, archive_read_free(all));
    extract_reference_file("test_read_format_uc2_4.uc2");
    assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, "test_read_format_uc2_4.uc2", 7));
    assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &e));
    assertEqualInt(ARCHIVE_FORMAT_UC2, archive_format(a));
    assertEqualString("fixture.txt", archive_entry_pathname(e));
    assertEqualInt(2000, archive_entry_size(e));
    assertEqualIntA(a, 2000, archive_read_data(a, data, sizeof data));
    for (int i = 0; i < 100; ++i)
        assertEqualMem(data + 20 * i, "UC2 optional reader\\n", 20);
    assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &e));
    assertEqualInt(ARCHIVE_OK, archive_read_free(a));

    const char *limits[] = {"uc2:memory-limit=1", "uc2:input-limit=20", "uc2:entry-limit=1999", "uc2:total-limit=1999"};
    for (unsigned k = 0; k < sizeof limits / sizeof *limits; k++) {
        a = archive_read_new();
        assertEqualInt(ARCHIVE_OK, archive_read_support_format_uc2(a));
        assertEqualIntA(a, ARCHIVE_OK, archive_read_set_options(a, limits[k]));
        assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, "test_read_format_uc2_4.uc2", 7));
        if (k < 2) assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &e));
        else {
            assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &e));
            assertEqualIntA(a, ARCHIVE_FATAL, archive_read_data(a, data, sizeof data));
        }
        assertEqualInt(ARCHIVE_OK, archive_read_free(a));
    }
    a = archive_read_new();
    assertEqualInt(ARCHIVE_OK, archive_read_support_format_uc2(a));
    extract_reference_file("test_read_format_uc2_80.uc2");
    assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, "test_read_format_uc2_80.uc2", 7));
    assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &e));
    assertEqualInt(70001, archive_entry_size(e));
    unsigned char *turbo = malloc(70001);
    assert(turbo != NULL);
    if (turbo) {
        assertEqualIntA(a, 70001, archive_read_data(a, turbo, 70001));
        unsigned x = 80;
        for (unsigned i = 0; i < 70001; i++) {
            x = x * 1664525u + 1013904223u;
            assertEqualInt(i < 30000 ? (unsigned char)(x >> 24) : (unsigned char)(i % 251), turbo[i]);
        }
        free(turbo);
    }
    assertEqualInt(ARCHIVE_OK, archive_read_free(a));

    a = archive_read_new();
    assertEqualInt(ARCHIVE_OK, archive_read_support_format_by_code(a, ARCHIVE_FORMAT_UC2));
    extract_reference_file("test_read_format_uc2_6.uc2");
    assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, "test_read_format_uc2_6.uc2", 7));
    assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &e));
    assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#endif
}
'''
write("libarchive/test/test_read_format_uc2.c", test)
replace("libarchive/test/CMakeLists.txt", "    test_read_format_lha.c",
        "    test_read_format_uc2.c\n    test_read_format_lha.c")
replace("Makefile.am", "\tlibarchive/test/test_read_format_lha.c \\",
        "\tlibarchive/test/test_read_format_uc2.c \\\n\tlibarchive/test/test_read_format_lha.c \\")
write("Makefile.am", (source / "Makefile.am").read_text() + '''
EXTRA_DIST += libarchive/test/test_read_format_uc2_4.uc2.uu \\
             libarchive/test/test_read_format_uc2_6.uc2.uu \\
             libarchive/test/test_read_format_uc2_80.uc2.uu \\
             doc/uc2-reader.md
''')
write("doc/uc2-reader.md", (repo / "contrib/libarchive/upstream/REVIEW.md").read_text())

patch = []
for name in sorted(changed):
    before = (original / name).read_text().splitlines(keepends=True) if (original / name).exists() else []
    after = (source / name).read_text().splitlines(keepends=True)
    patch.extend(difflib.unified_diff(before, after,
                 fromfile="a/" + name if before else "/dev/null", tofile="b/" + name))
target = work / "libarchive-3.8.11-uc2-review.patch"
target.write_text("".join(patch))
metadata = {"base": "libarchive 3.8.11", "submitted": False,
            "upstream_pr": "https://github.com/libarchive/libarchive/pull/3832",
            "note": "Release-based reproduction; the submitted PR targets upstream master",
            "upstream_base": "eb57ffefe20e98eb7ba05829ee1c23b90e589a71",
            "upstream_head": "5ad7e3d7",
            "format_id": "0x110000 (proposed, not allocated)",
            "decoder": "optional shared libuc2read; existing LGPL-3.0-only sources",
            "adapter_license": "BSD-2-Clause option",
            "patch_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
            "files": sorted(changed)}
(work / "review.json").write_text(json.dumps(metadata, indent=2) + "\n")
shutil.copyfile(repo / "contrib/libarchive/upstream/REVIEW.md", work / "REVIEW.md")
print(target)
