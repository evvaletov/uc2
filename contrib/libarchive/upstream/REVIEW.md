# Proposed UC2 reader for libarchive

Prepared for review against libarchive 3.8.11. No contribution has been sent
upstream. `0x110000` is a proposed format code, not an assigned identifier.
The local full-feature plugin continues using its private ID until acceptance.

The patch registers a BSD-2-Clause adapter and links an optional external
`libuc2read` shared library. That library compiles the existing LGPL-3.0-only
reader with rANS disabled. It does not contain the GPL compressor or GPL rANS
module. No existing UC2 or third-party source license changes. Enable it
explicitly with CMake `-DENABLE_UC2READ=ON` or Autotools `--with-uc2read`.
The default build does not acquire the dependency and reports UC2 unavailable.

The adapter reads historical supported Huffman/delta and Turbo entries,
including custom dictionaries. rANS, encrypted and multi-volume archives are outside
this proposed dependency's scope. The regular UC2 release retains rANS.
The patch includes registration by code, public declarations, CMake/Autotools
wiring, and fixtures for legacy/Turbo reads, quota failures and rANS rejection.

Input and decoded payloads spool to temporary files. Decoder allocations,
including cached dictionaries, and adapter metadata/paths share a **64 MiB**
budget. A sorted directory index avoids a full entry scan for each path parent.
Defaults also limit input to 4 GiB minus one byte, entries to 100,000, a decoded
entry to 1 GiB, and cumulative decoded output to 8 GiB. Limits fail explicitly;
they never silently truncate output. Listing and skipping do not decode files.
The fixed adapter state (about 65 KiB), stdio and libarchive's own allocations
are outside the shared memory budget. The input and current decoded entry use
disk space; the decoded entry spool is released between files.

Override limits before reading with libarchive format options, for example
`bsdtar --options uc2:memory-limit=134217728,uc2:entry-limit=2147483648 -xf data.uc2`.
Options are `memory-limit`, `input-limit`, `entries-limit`, `entry-limit` and
`total-limit`, expressed as positive decimal bytes (or an entry count). Zero,
signs, suffixes and overflow are rejected. The 32-bit input/entry format limits
remain in force. These are conservative adapter defaults proposed for upstream
review, not libarchive-wide policy. Archive versions retain repeated paths in
stored order. DOS timestamps have no timezone; the adapter treats them as UTC
using Gregorian date arithmetic, without changing the process timezone.

The dependency is built as a shared LGPL library, with GPL rANS/compressor
objects excluded. CMake checks the required reader symbols at configure time;
Autotools checks the pkg-config dependency and links a reader API probe.
`0x110000` does not occur in the published libarchive master header checked on
2026-11-07. That absence is not an allocation: maintainer agreement is still
required. The patch remains based on the pinned 3.8.11 release, not master.

To reproduce from the UC2 source checkout:

```sh
cmake -S contrib/libuc2read -B /tmp/uc2-reader \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/uc2-reader-prefix
cmake --build /tmp/uc2-reader
cmake --install /tmp/uc2-reader
python3 contrib/libarchive/prepare_upstream.py \
  --source /path/to/libarchive-3.8.11 --uc2 /path/to/uc2 \
  --output /tmp/uc2-review
cmake -S /tmp/uc2-review/libarchive -B /tmp/uc2-review/build \
  -DCMAKE_PREFIX_PATH=/tmp/uc2-reader-prefix -DENABLE_UC2READ=ON \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_TEST=ON -DENABLE_WERROR=OFF
cmake --build /tmp/uc2-review/build --target bsdtar libarchive_test
/tmp/uc2-review/build/bin/libarchive_test \
  -r /tmp/uc2-review/libarchive/libarchive/test test_read_format_uc2
python3 tests/scripts/check_bsdtar.py /path/to/uc2 \
  /tmp/uc2-review/build/bin/bsdtar --legacy-only
```

The maintainers still need to decide whether to accept this optional LGPL
dependency and the proposed format number. Full permissive-source rANS support
would require separate licensing work; this patch does not grant it.

The Autotools path was also built and exercised on Linux. After applying the
patch, run `autoreconf -fi` in the libarchive tree, then configure in a separate
build directory with `--with-uc2read`. For a private installation prefix, set
`PKG_CONFIG_PATH` to its `lib/pkgconfig` or `lib64/pkgconfig` directory and
`LD_LIBRARY_PATH` to the matching library directory during both configure and
execution. Configure runs linked test programs, so setting the runtime path
only when running bsdtar is insufficient. This validation built bsdtar with
`--disable-shared --without-xml2 --without-expat` and restored the legacy fixture
byte for byte; it does not establish support for every optional dependency.
