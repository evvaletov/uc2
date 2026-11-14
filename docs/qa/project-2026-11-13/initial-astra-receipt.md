# Initial independent review receipt

Reviewer: GPT-6 Astra, independent model seat `/root/independent_astra`.
Date: 13 November 2026.

These were completed static reviews of frozen source bundles. No tests were run, code was modified, live working tree was inspected, or other reviewers' findings were consulted. The reviewer did not delegate or invoke skills. This receipt records initial findings, not verification of subsequent repairs. No current code was inspected to prepare it.

## Native library — FINDINGS

Scope: complete `panel/native-r1.txt`, including supplied native code, tests and contracts. The original detailed native response is no longer fully available in retained context; the following is its retained condensed record. Exact original wording, priorities and line references cannot be reconstructed reliably.

Substantive findings reported:

- rANS frequency normalisation can underflow a frequency when correction exceeds the selected symbol's count; the reported example used 200 frequencies of 100 and 144 frequencies of 1.
- Unchecked rANS reallocation/final allocation failures.
- Ingest can continue with a partial Merkle result after allocation failure; oversized offsets can truncate to 32 bits.
- Block-store write failures can leave partial objects treated as successfully stored.
- `uc2_read_cdir(NULL)` can unconditionally invoke tag handling on a bare entry and return a user-callback error.
- Ordinary Huffman decoding can swallow negative errors in the path where the consumed-size pointer is NULL; compatibility implications were explicitly qualified.
- Decoded counts above `INT_MAX` can become negative although writer limits permit unsigned 32-bit sizes.
- BWT allocation-size multiplication/32-bit length boundaries.
- BCJ signed arithmetic overflow and SimHash vote-counter overflow.
- OTS serialisation size overflow and rejection of a short valid SHA-1 envelope by a fixed minimum-length check.

Maintainability observations included BWT global state/quadratic sorting and ingest cleanup ownership. A test-assertion concern was subsequently withdrawn as applicable context: the coordinator confirmed that `tests/CMakeLists` forces assertions active with `-UNDEBUG`. It must not be counted as an outstanding finding.

The coordinator said native repairs were being handled separately. This seat did not verify those repairs in this initial review.

## Integrations, browser and packaging — FINDINGS

Scope: complete 4,463-line `panel/integrations-r1.txt`, including workflows, libarchive/MC/WCX adapters, browser implementation and supplied tests.

Confirmed findings:

1. `.github/workflows/build.yml`: both optional-libarchive script invocations omit `--seekable-only`. The generated adapter rejects pipes while the scripts otherwise expect full-adapter behaviour. Add the flag to the optional-reader calls only; retain standalone pipe coverage.
2. `scripts/package_cli.py`: generated release README incorrectly says Turbo and multi-volume input remain unsupported. Update for current Turbo/SAS/DOS UC2SEA support and qualify legacy compatibility when `--chunked` is used.
3. `contrib/totalcommander/uc2_wcx.c::open_file`: quadratic previous-file scans for version assignment, one-item allocation growth and repeated linear parent lookup. Use indexed/grouped version assignment and geometric allocation while retaining ordering and visible-name collision checks. This was a consequential complexity finding, not a measured timing result.

No additional confirmed browser/service-worker defect was reported. Input-spool differences and decoded-entry output spooling were treated as intentional contracts.

## CLI and Python runtime — FINDINGS

Scope: complete 7,942-line `panel/interfaces-r1.txt`, including compatibility code, ingestion, Python native wrappers, extraction, mount, preservation, snapshots, selected regressions, headers and contracts.

MUST_FIX findings reported:

1. `cli/src/main.c::visit_selected`/`extract_cb`: skipping `LeaveDir` for unmarked directories leaves `path->ptr` unbalanced. Existing or selection-only parent directories can cause later sibling entries to be extracted beneath the wrong directory. Always balance entry/exit events; gate only metadata application.
2. `main.c::ots_info_cb`: `consumed + uri_len` can wrap and permit an out-of-bounds `fwrite` length. Validate using subtraction after checking `consumed`.
3. `main.c::ingest_restore_v3`: omits `uc2_finish_cdir`, accepting a missing directory trailer that the ordinary CLI path now rejects. Finish validation before emitting file bytes.
4. CLI output completion: ordinary `-p` and ingest restore omit checked final stdout flushing; OTS extraction ignores close errors; attachment ignores flush/final-close errors. Buffered failures can return success. Check completion before success, and stop attachment before truncation after a failed flush.
5. Windows CLI offsets: `my_read`, `tell32` and OTS position handling use signed 32-bit `long` interfaces under MSVC, despite unsigned 32-bit format offsets. Use wide platform-aware seek/tell and validate before narrowing; retain DOS constraints separately.
6. `python/uc2/snapshot.py`: `_directory`, `_regular` and source traversal omit Windows reparse-point checks. Junctions can redirect source/repository traversal. Reject reparse points consistently and add Windows junction regressions.

WORTH_SIMPLIFYING findings reported:

- `main.c::new_entry`: quadratic version-name scans and repeated linear directory lookup; index parents and group versions while preserving traversal/order semantics.
- `verify_trailer_if_present`/`load_trailer`: every ordinary integrity test reads the complete archive into memory solely to detect an optional OTS trailer. Inspect the footer first, read only a validated proof, and keep prefix hashing streamed.

No preference-only changes were recommended. The supplied Python directory-manifest repair fix was considered and not flagged. Later working-tree fixes were explicitly outside this frozen review.

## Maintained prose and maintainer reply — PASS

Scope: complete 4,015-line `panel/prose-r1.txt`, including dated release reports and `contrib/libarchive/upstream/response-2026-11-13.md`.

No concrete correction was identified. The maintainer reply accurately distinguished the checked CLI fallback, seekable-only proposal, retained decoded-entry spool, format references, limited evidence of demand and maintenance burden. It made no acceptance claim; British English was suitable. Explicitly dated historical statements were retained as historical, and no broad style rewrite was recommended.

External publication/validation claims were assessed against supplied evidence, not independently reverified. PASS is scoped to this frozen prose review, not proof of external claims or approval of later revisions.
