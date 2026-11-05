# UC2 reader for libarchive

The adapter reads UC2 directories, long-name tags, file versions, master blocks,
and Huffman/rANS payloads through libarchive's normal read interface. It is an
opt-in integration using libarchive's private API. It cannot be loaded as a
runtime plugin into an unmodified system libarchive. Stored versions are
returned as repeated pathnames; normal bsdtar extraction leaves the last one.

Input and each decoded file are spooled to anonymous temporary files. Payload
RAM is bounded by the read/decode buffers; directory metadata and the decoder's
master cache still scale with archive contents. Temporary disk space can reach
the compressed archive size plus the largest decoded entry. A file is decoded
and checked before its first data block is returned. Pipes and filter input
work, but this is not low-latency incremental decompression.

## Build actual bsdtar with UC2 support

Download a libarchive source release, then run:

```sh
python3 contrib/libarchive/build_bsdtar.py \
  --source /path/to/libarchive-3.8.11 --build build-bsdtar
build-bsdtar/build/bin/bsdtar -tf tests/archives/basic.uc2
python3 tests/scripts/check_bsdtar.py \
  build/cli/uc2 build-bsdtar/build/bin/bsdtar
```

The script copies libarchive into its new build directory, registers the UC2
reader in `archive_read_support_format_all`, and links the current UC2 core.
It leaves the supplied libarchive tree untouched. The generated binary is a
patched distribution including GPL/LGPL components. This is not an upstream
libarchive release.

The November 5 build used libarchive 3.8.11, `.tar.xz` archive SHA-256
`a3dc0872b1b39da34dc18ef1b944f0f11ffd37425d70ae04be679279f1cf43d2`.
The executable test covers listing, directories, empty files, selected-member
extraction, both entropy methods, and pipe input. On Linux it also restores a
128 MiB entry under a 96 MiB address-space limit.

## Build the separate adapter target

For applications already linking their own static libarchive build:

```sh
cmake -B build-larch -DUC2_BUILD_LIBARCHIVE_PLUGIN=ON \
  -DLIBARCHIVE_SOURCE_DIR=/path/to/libarchive-source \
  -DLIBARCHIVE_LIBRARY=/path/to/libarchive.a
cmake --build build-larch
ctest --test-dir build-larch -R libarchive_roundtrip --output-on-failure
```

Call `archive_read_support_format_uc2()` before opening input. The target needs
private libarchive headers, so installed development headers alone are
insufficient. Explicitly requesting the target with missing prerequisites is a
configuration error. Test against the same libarchive source/library version.

## Licensing and upstream work

On November 5, 2026, Eremey Valetov approved dual licensing of his adapter and
independently written integration glue under
`GPL-3.0-or-later OR BSD-2-Clause`. The BSD license is supplied in this directory.
The UC2 core and third-party code retain their existing licenses. In particular,
Bobrowski's LGPL decoder has not been relicensed.

The adapter can therefore be offered under BSD, but upstream still needs to
accept a compatible external dependency arrangement or an independently
licensed decoder. It also needs to assign an official format number; this
prototype uses `0xC0FF0000`. The distribution recipe and tests are ready for
review. No upstream acceptance or submission is implied.
