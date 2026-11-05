# Integration validation, 2026-11-05

The previous roadmap priorities 4-5 produced a patched bsdtar reader,
Midnight Commander extfs reader, 64-bit Total Commander WCX reader, and
reproducible collection benchmarks. The CLI ingest path now spools unique
chunks and replaces its destination only after completing the archive.

## Verified locally

- 25 selected Debug CTests passed, including the optional libarchive driver.
- 24 selected Release CTests passed with test assertions enabled.
- Four focused ASan/UBSan checks passed: CLI ingest, integrations, libarchive
  round trip, and the committed fuzz-regression replay. Address-space limits
  were disabled for sanitizer processes because ASan reserves virtual memory.
- Actual bsdtar 3.8.11 listed and extracted Huffman and rANS archives, directories,
  empty entries, selected members, and piped input. A 128 MiB file restored
  under a 96 MiB address-space limit and matched SHA-256.
- CLI ingest restored boundary-size inputs exactly and ingested 64 MiB under a
  48 MiB address-space limit. Terminating an unfinished writer preserved its
  existing destination. A damaged payload caused MC copyout to fail while
  preserving the old destination.
- MC opened an archive through the installed extension action after Enter and
  listed the expected files. Helper tests cover literal wildcards, brackets,
  spaces, version-looking names, empty files, and read-only behavior.
- The 64-bit WCX DLL built with MinGW and passed a Wine ABI host test for both
  entropy methods: recognition, Unicode archive path, listing, skip/test/extract,
  byte comparisons, and cancellation. It rejects a literal filename colliding
  with a generated stored-version suffix.
- All 60 benchmark round trips verified SHA-256. See
  [results and methodology](benchmarks/2026-11-05.md).

The user installation is `~/.local/bin/uc2` plus the helper at
`~/.local/share/mc/extfs.d/uc2`. A new `~/.config/mc/mc.ext.ini` preserves the
system extension definitions and adds UC2 before the Default section. None of
these three user files existed before this installation.

## Scope still open

DOSBox and native Windows/MSVC were not run in this pass. The WCX test host is
separate from the Total Commander UI; native UI and 32-bit tests remain. New
CI jobs have been configured locally and have not yet run on GitHub.
The libarchive adapter uses a placeholder format ID and private API. Upstream
submission and a compatible core-dependency license arrangement remain open.

The standalone MinGW CLI exposed existing compatibility-header conflicts when
built alongside the plugin. The WCX targets themselves build and run; the
fixture generator uses the native CLI. This is a portability follow-up, not
native MSVC test evidence.

The full task-qa + task-at workflow remains pending. The standalone BLAKE3
module still fails official vectors and remains tracked in c9edf0b. The README
now states that limitation and separates library modules from CLI features.

The home filesystem filled during a late validation run. Only this task's
synthetic benchmark data and Wine profiles were moved to `/tmp`; the affected
tests and benchmarks were rerun successfully. Public benchmark evidence in this
repository comes from the successful run.
