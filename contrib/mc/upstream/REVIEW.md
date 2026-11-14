# Proposed Midnight Commander UC2 helper

Prepared November 7, 2026; revalidated November 14 for alpha12. Submission is
authorised and QA, prose and elegance review are complete. GitHub rejected the
issue-creation attempt because of an account restriction. No upstream issue,
PR or message has been submitted.

[Patch](mc-3228c23-uc2.patch), based on MidnightCommander/mc master
`3228c23fd57db28c20c35db8e0e1c6d3b087b3c7`, adds a read-only Python extfs
helper, configure/install entries, a `.uc2` association, and listing fixtures.
It calls the separately installed native `uc2` CLI; it adds no linked library
dependency to MC. Tested CLI release: [3.0.0a12](https://github.com/evvaletov/uc2/releases/tag/v3.0.0-alpha.12).
Python 3 is required. The helper lists stored versions and
copies files out with literal path selection and staged replacement. Writes
are refused. The configured helper retains the upstream test command hook.

Proposed issue/PR title: **Add read-only UltraCompressor II extfs support**

Proposed description: [reviewed issue text](ISSUE-DRAFT.md).

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
title. This patch has no invented issue number or sign-off. Once GitHub access
is available, check for an existing submission and any upstream changes before
posting the reviewed issue.
