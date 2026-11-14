# Proposed UC2 reader for libarchive

Submitted as [libarchive/libarchive#3832](https://github.com/libarchive/libarchive/pull/3832)
on 2026-11-07, from `evvaletov:uc2-optional-reader` at `5ad7e3d7`, based on
upstream `eb57ffefe20e98eb7ba05829ee1c23b90e589a71`. Maintainers requested
design clarification on 8 November; the 13 November
revision removes whole-input spooling from the optional reader. Acceptance is
pending.
`0x110000` remains proposed, not assigned. The local full-feature adapter keeps
its private ID until acceptance.

The patch in this directory is a reproducible **libarchive 3.8.11** validation
bundle. It is not the exact current-master PR diff; use the PR for that source,
its upstream formatting, man-page additions and pinned dependency instructions.
`review.json` records this distinction.

The patch registers a BSD-2-Clause adapter and links an optional external
`libuc2read` shared library. That library compiles the existing LGPL-3.0-only
reader with rANS and the new shared-chunk extension disabled. It does not
contain the GPL compressor, rANS or SHA-256 modules. No existing UC2 or third-party source license changes. Enable it
explicitly with CMake `-DENABLE_UC2READ=ON` or Autotools `--with-uc2read`.
The default build does not acquire the dependency and reports UC2 unavailable.

The adapter reads historical supported Huffman/delta and Turbo entries,
including custom dictionaries. rANS, shared chunks, encrypted and multi-volume archives are outside
this proposed dependency's scope. The regular UC2 release retains rANS.
The patch includes registration by code, public declarations, CMake/Autotools
wiring, and fixtures for legacy/Turbo reads, quota failures, seekable short reads, explicit pipe rejection and rANS rejection.

Input uses libarchive seek/read callbacks directly. The optional reader requires
a seekable source; actual pipes and non-seekable filters fail before any entry
is returned. Redirected regular-file stdin works. Each decoded entry uses one
temporary file to bridge the synchronous decoder and libarchive data callbacks. Decoder allocations,
including cached dictionaries, and adapter metadata/paths share a **64 MiB**
budget. A sorted directory index avoids a full entry scan for each path parent.
Defaults also limit input to 4 GiB minus one byte, entries to 100,000, a decoded
entry to 1 GiB, and cumulative decoded output to 8 GiB. Limits fail explicitly;
they never silently truncate output. Listing and skipping do not decode files.
The fixed adapter state (about 65 KiB), stdio and libarchive's own allocations
are outside the shared memory budget. Only the current decoded entry uses temporary disk space; its spool is released
between files. Spools use
libarchive's anonymous temporary-file helper, including its Windows temp-directory
handling. These libarchive build/runtime checks were performed on Linux; native
Windows execution of the optional adapter has not been validated.

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
  --source /path/to/libarchive-3.8.11 --uc2 /path/to/uc2/build/cli/uc2 \
  --output /tmp/uc2-review
cmake -S /tmp/uc2-review/libarchive -B /tmp/uc2-review/build \
  -DCMAKE_PREFIX_PATH=/tmp/uc2-reader-prefix -DENABLE_UC2READ=ON \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_TEST=ON -DENABLE_WERROR=OFF
cmake --build /tmp/uc2-review/build --target bsdtar libarchive_test
/tmp/uc2-review/build/bin/libarchive_test \
  -r /tmp/uc2-review/libarchive/libarchive/test test_read_format_uc2
python3 tests/scripts/check_bsdtar.py /path/to/uc2 \
  /tmp/uc2-review/build/bin/bsdtar --legacy-only --seekable-only
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

Current-master validation: all 969 libarchive tests pass. Focused enabled CMake
and Autotools tests pass 130,390 assertions; disabled CMake tests also pass with
warnings as errors. Full-suite testing caught and fixed format-slot exhaustion
when registering the raw fallback after all formats. Resource-limit and actual
bsdtar tests pass. The PR is submitted; acceptance and any review revisions
remain open under 7bbb6b6.
