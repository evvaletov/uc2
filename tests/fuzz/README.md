# Fuzzing UC2

Four libFuzzer harnesses cover input parsing and compression:

- `fuzz_extract.c` -- the read path. Drives
  `uc2_open -> uc2_read_cdir -> uc2_finish_cdir -> uc2_extract` over
  arbitrary bytes with an in-memory reader and a discard writer. This is
  the security-critical target: it parses attacker-controlled `.uc2`
  archives.
- `fuzz_roundtrip.c` -- the compress / round-trip path. Compresses the
  fuzzer bytes (the first byte selects the level), wraps them in a
  minimal one-file archive, decompresses, and asserts byte-identity.
  Catches encoder crashes and any compress<->decompress mismatch.
- `fuzz_chunks.c` -- the method-11 path. Recomputes the bounded recipe
  digest after mutations to reach descriptor and compressed-payload checks.
- `fuzz_sea.c` -- the standalone SEA Huffman decoder. Feeds bounded raw
  bitstreams to `uc2_decompress_raw` without running executable input.

## Build and run by hand

Compile a harness together with the library sources and the embedded
super-master, against a configured build tree (for `uc2_version.h` and
`super_data.S`):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release      # provides the generated files
clang -fsanitize=fuzzer,address -O1 -g \
    -Ilib/include -Ilib/src -Ibuild/lib \
    tests/fuzz/fuzz_extract.c lib/src/*.c build/lib/super_data.S \
    -lm -o fuzz_extract

mkdir -p corpus && cp tests/fuzz/seeds/* tests/fuzz/regressions/* corpus/
./fuzz_extract -max_len=65536 -timeout=20 corpus/
```

ASan checks memory accesses in the instrumented code; libFuzzer writes a `crash-*` (or
`timeout-*` / `oom-*`) artifact for each finding, and exits non-zero.
Re-run a single artifact with `./fuzz_extract <artifact>`.

## Corpus and regressions

- `seeds/` -- valid sample archives, used to seed coverage.
- `regressions/` -- minimised crash/timeout artifacts kept as permanent
  guards. `len0-match-underflow.uc2` is the input behind git-bug
  `b8f933c`: a zero-length match code made the decoder's copy loop
  underflow (`while (--len)` with `len == 0`) into ~4 billion iterations,
  a CPU denial-of-service. Fixed by rejecting `len == 0` and the negative
  extra-bit reads in `decompress_block`.

## CI

Two layers run automatically:

- `fuzz_regression` (ctest, every platform) -- a portable replay
  (`fuzz_replay.c`, no Clang/ASan needed) runs the read-path harness over
  every file in `regressions/` and `seeds/` under a 30 s ctest TIMEOUT,
  so a hang or failing replay is reported in the normal matrix.
- the `fuzz` workflow job (Linux) -- builds all four harnesses with
  libFuzzer + ASan, replays the committed artifacts, then runs a bounded
  coverage-guided session over each path. A crash, OOM, or slow input
  fails the job and uploads the artifact.

`regressions/turbo-empty-checksum.uc2` is a generated method80 archive with
a zero-length file and deliberately wrong file checksum. Libarchive must
verify the stream even when the declared file size is zero.

`chunk-raw.uc2`, `chunk-huffman.uc2` and `chunk-rans.uc2` are generated a11
fixtures containing pseudorandom data or repeated test strings. The regular
read-path replay also opens them. CI builds harnesses with `NDEBUG`, matching
the production library and suppressing diagnostic output.
