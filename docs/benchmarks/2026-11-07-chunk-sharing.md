# Named-file chunk sharing — 2026-11-07

Linux x86-64 development build of a11, level 4, three trials per case. All
restores matched the source SHA-256 manifests. Timings below are medians;
file sizes were identical across trials. These are small-collection measurements,
not capacity or universal compression claims.

## Synthetic collections

Each case has eight 512 KiB revisions, except generated source text (3.62 MB
total). The fixed seed and edit rules are in `scripts/benchmark_collections.py`.
Binary versions have eight local edits per revision; assets reorder shared
blocks; simulation checkpoints change one block; independent inputs share no
content. Tar modes preserve the complete stream; named modes expose files.

| Collection | Default UC2 bytes | Shared-chunk UC2 bytes | Change | 7-Zip bytes | tar+Zstd bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| binary versions | 3,763,693 | 1,556,525 | -58.6% | 528,060 | 527,814 |
| game assets | 4,204,367 | 2,216,932 | -47.3% | 791,208 | 853,609 |
| simulation | 3,818,737 | 689,405 | -81.9% | 558,992 | 558,175 |
| source versions | 282,315 | 68,771 | -75.6% | 11,869 | 16,182 |
| independent | 4,201,233 | 4,213,383 | +0.3% | 4,194,795 | 4,198,532 |

The chunk mode reduced creation time on all five cases (0.21–0.40 seconds
versus 0.60–2.27 seconds). Restores took 0.09–0.16 seconds versus 0.05–0.15
seconds. Zstandard and 7-Zip still produced smaller archives in these comparisons;
Zstandard was also faster. Full times, process peak RSS, tool versions and input
parameters are in [the raw results](chunk-sharing-2026-11-07/synthetic.json).

## Edited public source files

The twelve largest `archive_read_support_format_*.c` files in libarchive commit
`eb57ffefe20e98eb7ba05829ee1c23b90e589a71` each have an original and a copy with
a short inserted comment at the midpoint: 24 files, 2,237,336 input bytes.

| Mode | Archive bytes | Creation seconds | Restore seconds |
| --- | ---: | ---: | ---: |
| Default dictionary mode | 378,945 | 0.627 | 0.073 |
| Shared chunks | 413,429 | 0.319 | 0.169 |

Here shared chunks made the archive **9.1% larger**. Independent small-chunk
compression loses some of the full-file dictionary benefit on this text corpus.
The writer therefore keeps its existing default. Choose `--chunked` when its
reuse benefits your collection; further codec/chunk-boundary tuning remains open.
[Raw source hashes and measurements](chunk-sharing-2026-11-07/public-source.json).

## Reproduce

```sh
python3 scripts/benchmark_collections.py --uc2 /path/to/uc2 \
  --output /tmp/uc2-comparison --size-kib 512 --repeat 3
git clone https://github.com/libarchive/libarchive /tmp/libarchive-source
git -C /tmp/libarchive-source checkout eb57ffefe20e98eb7ba05829ee1c23b90e589a71
python3 tests/scripts/benchmark_chunk_sharing.py --source /tmp/libarchive-source \
  --uc2 /path/to/uc2 --output /tmp/uc2-source-comparison --repeat 3
```

The scripts require new output directories and `/usr/bin/time`; the comparison
also needs 7-Zip and Zstandard. No private collection data was used.
