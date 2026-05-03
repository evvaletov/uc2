# UC2 read-format plugin for libarchive

This directory contains the design and a skeleton implementation of a
read-only `.uc2` format handler for libarchive.  The goal is to make
UC2 archives transparently extractable by every libarchive-using tool
(`bsdtar`, `cmake`, `pkg(8)`, file-roller, Ark, and others).

## Status

- `archive_read_support_format_uc2.c` -- skeleton with the five
  required callbacks (bid, read_header, read_data, read_data_skip,
  cleanup).  Implementation is annotated with `// TODO` markers at
  the points where libuc2 calls are wired in.
- `CMakeLists.txt` -- gated build target.  Only configures if
  `find_package(LibArchive)` succeeds; produces a static archive that
  exposes `archive_read_support_format_uc2`.
- Full implementation is tracked separately in git-bug; the skeleton
  here is the starting point.

## Why an out-of-tree skeleton?

libarchive's read-format plugin API is internal.
`archive_read_register_format` is a static function, not part of the
public ABI.  An out-of-tree `.so` cannot be loaded into an unmodified
libarchive at runtime.

The supported integration paths are:

1. **Upstream merge.**  Submit
   `archive_read_support_format_uc2.c` as a PR against
   `libarchive/libarchive`.  Once merged, distros pick it up and
   every tool that links libarchive sees `.uc2` automatically.  This
   is the long-term goal.

2. **Patched libarchive build.**  Distribute a small patch that
   includes the UC2 plugin against a known libarchive version.
   Useful for testing before upstream merge and for users who want
   `.uc2` support before the upstream release reaches their distro.

3. **Static-library wrapper.**  Build the plugin as part of a custom
   tool that statically links libarchive + this plugin.  Useful for
   demo binaries; not a substitute for upstream merge because the
   wrapper still won't be picked up by `bsdtar` etc.

## Architecture

UC2 archives use a fixed front header (29 bytes), a record stream
of compressed bodies, and a compressed central directory whose
offset is recorded in the front header.  The central directory
holds OHEAD records for masters, dirs, and files; entry attributes
are in OSMETA + DIRMETA / FILEMETA.

The plugin uses libuc2 for parsing and decompression and adapts the
results to libarchive's `struct archive_entry` model.  libuc2 already
exposes a streaming read API (`uc2_open`, `uc2_read_cdir`,
`uc2_extract`) and is GPL-3.0 / LGPL-3.0; the plugin is GPL-3.0-or-later
to match the cli/main.c license boundary.  See
[`docs/license-audit.md`](../../docs/license-audit.md) for the
provenance table.

### Callback responsibilities

- **bid**: read the first 4 bytes via `__archive_read_ahead`, check
  for the UC2 magic (`0x1A324355`).  Return 64 on match, 0 otherwise.
  libarchive uses the highest bid to pick a format; 64 is the
  conventional "format-recognised" score.

- **read_header**: on first call, open the libuc2 handle and read
  the central directory into memory.  On every call, return one
  entry's metadata via `archive_entry_*` setters.  When entries are
  exhausted, return `ARCHIVE_EOF`.

- **read_data**: stream decompressed bytes for the current entry.
  libuc2's `uc2_extract` invokes a write callback per chunk; the
  plugin needs to convert this push model into libarchive's pull
  model (the standard way: a small ring buffer, plus a generator
  loop or coroutine).  The simplest first implementation buffers
  the whole entry, which is correct but increases memory pressure
  for very large files; refine later.

- **read_data_skip**: advance to the next entry without producing
  data.  Decompression cannot be safely skipped (master-block
  dependencies), so the plugin still decompresses, just discards.

- **cleanup**: close the libuc2 handle, free buffers.

### libuc2 IO callbacks

libuc2 takes user-supplied callbacks for read/alloc/free/warn.  The
plugin wires these to libarchive's filter stack:
- `read` -> `__archive_read_seek` + `__archive_read_ahead`
- `alloc`/`free` -> `malloc`/`free`
- `warn` -> push to libarchive's warning log via
  `archive_set_error`.

## Build

The CMake target only configures when libarchive headers are present.
Install on Fedora/RHEL with `dnf install libarchive-devel`, on Debian
with `apt install libarchive-dev`, or build libarchive from source.

```sh
cmake -B build -DUC2_BUILD_LIBARCHIVE_PLUGIN=ON
cmake --build build --target uc2_libarchive
```

The built object can be linked into a libarchive-using application or
patched into libarchive's source tree (`libarchive/libarchive/`).

## Roadmap

The current skeleton compiles into a stub library that registers a
no-op format.  The implementation milestones, in order:

1. bid function with magic check (~20 lines)
2. read_header for the first entry only (single-file archives)
3. read_data for uncompressed-by-master entries
4. Master-block decompression and dependency tracking
5. Multi-file archives + directory entries
6. Tagged entries (long names, extended attributes)
7. Round-trip test against bsdtar built from a patched libarchive
8. Upstream PR

Each milestone is independently shippable as a working subset.
