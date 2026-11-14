### Is there an existing request for this?

- [x] I have searched the existing issues.

### Is your feature request related to a problem?

I would like to browse and extract UltraCompressor II (`.uc2`) archives in Midnight Commander without a DOS runtime. I maintain the cross-platform UC2 revival and have prepared a read-only extfs helper that uses its native CLI.

### Describe the solution you'd like

The [patch](https://github.com/evvaletov/uc2/blob/v3.0.0-alpha.12/contrib/mc/upstream/mc-3228c23-uc2.patch) adds a Python 3 helper, configure/install entries, a `.uc2` association and a fixture for MC's listing parser. It applies to current master, `3228c23fd57db28c20c35db8e0e1c6d3b087b3c7`.

The helper exposes stored versions, selects filenames containing literal wildcard characters and stages copyout so that a failed extraction preserves an existing destination. It refuses archive writes. The native `uc2` CLI is a separately installed runtime dependency; MC gains no linked library dependency. The helper retains `MC_TEST_EXTFS_LIST_CMD` for the upstream test harness.

### Describe alternatives you've considered

Users can already list and extract these archives with the CLI, or install the [standalone extfs helper](https://github.com/evvaletov/uc2/tree/v3.0.0-alpha.12/contrib/mc) manually. Including the helper and extension rule upstream would make archive browsing available through the normal MC installation when `uc2` is installed.

### Additional context

The patch includes listing fixtures for directories, spaces, CP850 names, literal wildcard/bracket/semicolon names and stored versions. Direct copyout checks compare exact bytes for those names and both stored versions. The Check-enabled ncurses build and upstream `make check` pass with the patch.

The CLI used for validation is [UC2 3.0.0a12](https://github.com/evvaletov/uc2/releases/tag/v3.0.0-alpha.12), an alpha release. It reads original Huffman and Turbo archives and the revival's extensions. The helper is available under GPL-3.0-or-later or BSD-2-Clause. This issue proposes the helper and patch for review; no change to MC's linked dependencies is required.
