# Compression after the LZ match repair

The compressor previously inserted its current position into the hash chain
before searching. The search then stopped at distance zero, leaving ordinary
file compression effectively literal-only. The repair searches before
insertion, tracks absolute positions across circular-buffer generations, and
wraps all three hash bytes. Encoder and decoder dictionary lengths now agree.
The writer limits new master blocks to the format's 64,000-byte match distance,
and the parser avoids costly distant matches that save only three literal bytes.

Archive sizes below include dictionaries and directory overhead. All 168
baseline/final round trips across these seven collections matched SHA-256.
Each configuration ran three times; archive sizes were identical across trials.

| Collection | UC2 before | UC2 after | tar+UC2 after | 7z | tar+zstd |
| --- | ---: | ---: | ---: | ---: | ---: |
| Synthetic binary versions | 4,331,157 | 3,827,769 | 1,674,379 | 528,060 | 527,814 |
| Synthetic game assets | 4,265,513 | 4,201,371 | 2,246,281 | 791,208 | 853,609 |
| Synthetic source versions | 2,070,185 | 282,315 | 66,245 | 11,869 | 16,182 |
| Synthetic simulation checkpoints | 4,331,157 | 3,874,955 | 828,765 | 558,992 | 558,175 |
| Independent random data | 4,265,477 | 4,201,233 | 4,225,083 | 4,194,795 | 4,198,532 |
| UC2 source files, 315,760 input bytes | 255,993 | 95,817 | 106,683 | 77,063 | 94,737 |
| libarchive reader sources, 1,607,137 input bytes | 1,158,613 | 355,021 | 448,669 | 271,646 | 357,135 |

Ordinary UC2 archives shrink 86.4% for the synthetic source versions, 62.6%
for UC2 sources, and 69.4% for libarchive reader sources. The latter is close
to tar+zstd in size. 7z remains smaller on every collection. The synthetic
versioned binary collections still exceed the useful reach of a 64 KB shared
dictionary; the repair does not make them competitive with 7z or zstd.

## Time and memory

Match searching now does real work. Ordinary UC2 compression of independent
random data took a median 0.670 seconds in the final run, versus 0.077 seconds
in the baseline. The synthetic source collection took 0.184 seconds after the
repair. Measured final medians for the two real collections were:

| Collection | UC2 | tar+UC2 | 7z | tar+zstd |
| --- | ---: | ---: | ---: | ---: |
| UC2 sources | 0.210 s | 0.063 s | 0.294 s | 0.036 s |
| libarchive reader sources | 0.401 s | 0.156 s | 1.103 s | 0.052 s |

These short runs include process startup and shared-host scheduling noise.
Use them as local observations, not portable throughput claims. The final
ordinary UC2 process peaked at roughly 2.6-3.0 MiB RSS across these inputs;
Python fixture generation and tar materialization are outside the measured
process, equally for all compared methods. The wider absolute hash positions
add about 432 KiB to the compressor state.

A separate `gprof` build attributed about 79% of sampled CPU time to
`find_match` for source versions and 97% for independent random data. Master
construction and chunk fingerprinting did not dominate those profiles.
The profiles are coarse samples of short runs. They identify the next speed
target, without establishing a universal cost split.

## Reproduction and evidence

The baseline uses source commit `bc33446`; the final compressor and writer
changes are in `dcbbb5a`. Both use GCC 14.3.1 Release builds on the same Linux
host. Comparison settings remain UC2 level 4, default UC2 ingest, 7-Zip 26.04
`-mx=5 -mmt=1`, and zstd 1.5.5 `-3 -T1`.

```sh
python3 scripts/benchmark_collections.py --uc2 /path/to/uc2 \
  --output /tmp/new-synthetic-results
python3 scripts/benchmark_collections.py --uc2 /path/to/uc2 \
  --corpus /path/to/flat-source-files --output /tmp/new-source-results
```

The synthetic generator and fixed seed are unchanged from the
[baseline methodology](2026-11-05.md). The real collections contain public
source files: UC2's tracked `.c`, `.h`, and `.py` files under `lib`, `cli`, and
`python` at `bc33446`, and `libarchive/archive_read*.c` from the official
libarchive 3.8.11 source release. UC2 paths are flattened with `__`; provenance
records map every output filename to its original path and SHA-256.
No private user data is included.

[JSON/CSV results, binary hashes, source manifests, and profiles](compression-2026-11-05/)
include the independent-data control and every trial. The regression tests
now require repeated random blocks to compress substantially, as well as
round-trip correctly, at all eight compression levels. The round-trip fuzzer
also covers both Huffman and rANS.
