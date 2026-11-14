This adds an opt-in reader for historical UltraCompressor II archives. With
`ENABLE_UC2READ=ON` or `--with-uc2read`, libarchive and bsdtar can list and extract
Huffman/delta and Turbo entries, including custom dictionaries, CP850 names and
stored versions.

The BSD-2-Clause adapter links a separately maintained shared LGPL-3.0-only
`libuc2read`. It does not vendor the decoder or link the GPL compressor, rANS or
SHA-256 implementation. Support is disabled by default. The optional dependency
and proposed `0x110000` format identifier remain subject to maintainer agreement.

Following review, input now uses libarchive's seek/read callbacks directly.
The reader requires seekable input; pipes and non-seekable filters fail before
returning entries. Redirected regular-file stdin works. One decoded-entry
temporary file remains to bridge the synchronous push decoder and libarchive's
pull interface, and to validate the entry before returning bytes. Listing and
skipping do not decode payloads. Configurable limits cover input bytes,
decoder/metadata memory, entry count, per-entry output and total decoded output.

The patch includes CMake/Autotools wiring, registration APIs and man pages,
reference fixtures, seekable short-read and pipe-rejection tests, and a
raw-fallback registration regression. Encryption, PCP, volume/SFX resolution,
rANS and the revival's shared-chunk extension are outside this reader's scope.

Linux validation against master `eb57ffefe20e98eb7ba05829ee1c23b90e589a71`:

- CMake with UC2 enabled: the 969-test suite reports no failures and 83 skips,
  including passing UC2 and raw registration tests.
- CMake Debug with UC2 disabled and warnings as errors: focused tests pass
  without the reader dependency.
- Autotools with UC2 enabled: focused UC2/raw tests pass.
- Actual bsdtar: files/directories, Unicode names, empty entries, selective
  extraction, seekable stdin, pipe rejection, quota boundaries/overrides and
  invalid options pass. A 128 MiB entry restores under a 96 MiB address-space
  limit.

`doc/uc2-reader.md` pins the public dependency source and gives reproduction
commands, format references, resource limits and compatibility boundaries.
Native Windows/macOS execution of this optional upstream configuration has not
been validated. The full UC2 integration is separate.
