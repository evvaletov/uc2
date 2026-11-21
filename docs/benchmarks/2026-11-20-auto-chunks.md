# Automatic archive layout selection, 20 November 2026

The a13 development writer adds `--chunked=auto`. It assembles the existing
classic and shared-chunk layouts from the same input spool and emits the
smaller complete archive, selecting classic on ties. Neither layout nor the
default changes. C uses `UC2_WRITER_AUTO_CHUNKED`; Python uses `chunked="auto"`.

Three trials per collection used level 4 on Linux x86-64. Every restored file
matched its SHA-256 manifest. The public-source inputs match all twelve source
hashes from the [original benchmark](2026-11-07-chunk-sharing.md), including
its inserted edits: 24 files and 2,237,336 input bytes. The five synthetic
collections use the original seed and eight revisions per collection.

| Collection | Classic bytes | Chunked bytes | Auto bytes | Auto choice |
| --- | ---: | ---: | ---: | --- |
| Public C sources | 378,945 | 413,429 | 378,945 | Classic |
| Edited binaries | 3,763,693 | 1,556,525 | 1,556,525 | Chunks |
| Reordered game assets | 4,204,367 | 2,216,932 | 2,216,932 | Chunks |
| Simulation checkpoints | 3,818,737 | 689,405 | 689,405 | Chunks |
| Generated source revisions | 282,315 | 68,771 | 68,771 | Chunks |
| Independent binary inputs | 4,201,233 | 4,213,383 | 4,201,233 | Classic |

Sizes were identical across trials. Auto removes the 9.1% public-source
expansion and retains the 58.6% edited-binary reduction relative to classic.
Explicit `--chunked` still produces the same larger public-source archive;
the improvement comes from selecting the layout. It does not claim better
compression than 7-Zip or Zstandard, which remain smaller on these fixtures.

Auto performs both compression passes and retains both complete archives until
selection. Temporary disk space must cover the uncompressed input and both
candidates; the selected archive is then copied to the destination's sibling
temporary file. Both candidates must satisfy the existing limits. Any assembly
failure aborts before output, even if the other candidate succeeded. No
cross-archive reuse or new wire format is introduced.

The raw [public-source results](auto-chunks-2026-11-20/public-source.json) and
[synthetic results](auto-chunks-2026-11-20/synthetic.json) include times, peak RSS,
tool versions and the measured executable hash. Runs shared the host with other
work; no controlled speed comparison was performed.
The Python regression independently compares auto's exact bytes and statistics
with both explicit layouts at every level from 2 through 9, including empty archives,
stored versions, duplicates and empty files.

Run the updated scripts with a fresh output directory:

```sh
python3 tests/scripts/benchmark_chunk_sharing.py --source /path/to/libarchive \
  --uc2 /path/to/uc2 --output /tmp/uc2-auto-public --repeat 3
python3 scripts/benchmark_collections.py --uc2 /path/to/uc2 \
  --output /tmp/uc2-auto-synthetic --size-kib 512 --repeat 3
```

The libarchive checkout must be at
`eb57ffefe20e98eb7ba05829ee1c23b90e589a71` to reproduce these inputs.
