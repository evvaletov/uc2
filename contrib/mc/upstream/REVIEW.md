# Proposed Midnight Commander UC2 helper

Prepared November 7, 2026; revalidated November 14 for alpha12. Submitted as
[MidnightCommander/mc#5225](https://github.com/MidnightCommander/mc/issues/5225)
on November 19 at 21:44 EST (November 20, 02:44 UTC), verified November 20.
QA, prose and elegance review were complete before submission. The earlier
account restriction is superseded by the verified issue receipt.

[Patch](mc-3228c23-uc2.patch), based on MidnightCommander/mc master
`3228c23fd57db28c20c35db8e0e1c6d3b087b3c7`, adds a read-only Python extfs
helper, configure/install entries, a `.uc2` association, and listing fixtures.
It calls the separately installed native `uc2` CLI; it adds no linked library
dependency to MC. Tested CLI release: [3.0.0a12](https://github.com/evvaletov/uc2/releases/tag/v3.0.0-alpha.12).
Python 3 is required. The helper lists stored versions and
copies files out with literal path selection and staged replacement. Writes
are refused. The configured helper retains the upstream test command hook.

Issue title: **Add read-only UltraCompressor II extfs support**

Description: [reviewed issue text](ISSUE-DRAFT.md). The posted issue changes
only the upstream-revision sentence: it names the original patch base and
records clean applicability to `fc236523376ef3e66f597759ea36380df6ef121e`.

Validation:

- Patch applies cleanly to the pinned upstream commit.
- Upstream MC builds and installs with ncurses, Check 0.15.2, `--enable-tests`
  and `--enable-werror`. The full `make check` passes: 71 tests, no skips or
  failures. Its actual `mc_parse_ls_l` and `test_all` harness accept the added
  listing fixture.
- Fixture covers directories, spaces, CP850, literal wildcard/bracket/semicolon
  names and stored versions. Direct helper copyout checks compare exact bytes
  for literal names and both stored versions.
- The installed helper passes the parser and copyout checks with the downloaded
  public alpha12 Linux CLI. Other upstream platform configurations were not
  rerun for this submission.
- The patch downloaded from the public alpha12 tag is byte-identical to the
  reviewed patch. See the [submission review record](../../../docs/qa/commander-submissions-2026-11-14.md).

Reproduce the patch check in a clean upstream checkout:

```sh
git checkout 3228c23fd57db28c20c35db8e0e1c6d3b087b3c7
git apply --check /path/to/mc-3228c23-uc2.patch
git apply /path/to/mc-3228c23-uc2.patch
./autogen.sh
./configure --enable-tests --with-screen=ncurses --enable-werror --prefix="$PWD/_install"
make -j4
make -j4 check
make install
```

The [current contribution instructions](https://midnight-commander.org/source-code/)
require an issue or PR and review. The prepared feature issue links the patch;
it does not submit a signed code commit. A later code contribution needs the
contributor's DCO acknowledgement and an initial `Ticket #<issue>: ...` commit
title. This patch has no invented sign-off. Track maintainer feedback on issue
5225 and recheck upstream changes before preparing a code commit. See the
[submission receipt](../../../docs/qa/commander-submissions-2026-11-20.md).
