# Fuzzing the UC2 reader

`fuzz_extract.c` is a libFuzzer harness that drives the full read path
(`uc2_open` -> `uc2_read_cdir` -> `uc2_finish_cdir` -> `uc2_extract`)
over arbitrary bytes with an in-memory reader and a discard writer. It
targets the code that parses **untrusted** `.uc2` archives.

It is intentionally **not** part of the CMake build or CI: libFuzzer
needs a Clang toolchain, and a fuzz run is open-ended rather than
pass/fail. Build and run it by hand.

## Build

Compile the harness together with the library sources and the embedded
super-master, against a configured build tree (for `uc2_version.h` and
`super_data.S`):

```sh
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug   # any tree works; provides the generated files
clang -fsanitize=fuzzer,address -O1 -g \
    -Ilib/include -Ilib/src -Ibuild-asan/lib \
    tests/fuzz/fuzz_extract.c $(ls lib/src/*.c) build-asan/lib/super_data.S \
    -lm -o fuzz_extract
```

## Run

```sh
mkdir -p corpus && cp tests/archives/*.uc2 corpus/
./fuzz_extract -max_len=65536 -timeout=25 corpus/
```

ASan flags any out-of-bounds access; libFuzzer writes a `crash-*` (or
`timeout-*`) artifact for each finding. Re-run a single artifact with
`./fuzz_extract <artifact>`.

## Status

Memory-safety: clean over sustained runs after the 2026-06-13 cdir
hardening (git-bug 69e8e52). A residual slow-input (decompression-bomb)
timeout is tracked separately; it is a bounded-CPU issue, not a
memory-safety one.
