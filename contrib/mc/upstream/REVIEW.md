# Proposed Midnight Commander UC2 helper

Prepared November 7, 2026. No upstream issue, PR or message has been sent.

[Patch](mc-3228c23-uc2.patch), based on MidnightCommander/mc master
`3228c23fd57db28c20c35db8e0e1c6d3b087b3c7`, adds a read-only Python extfs
helper, configure/install entries, a `.uc2` association, and listing fixtures.
It calls the separately installed native `uc2` CLI; it adds no linked library
dependency to MC. Python 3 is required. The helper lists stored versions and
copies files out with literal path selection and staged replacement. Writes
are refused. The configured helper retains the upstream test command hook.

Proposed issue/PR title: **Add read-only UltraCompressor II extfs support**

Proposed description:

> This adds browsing and extraction of UltraCompressor II (`.uc2`) archives
> through a Python extfs helper and the native `uc2` CLI. It exposes stored
> versions, handles literal wildcard characters in names, and preserves an
> existing destination if extraction fails. The patch includes the extension
> rule, configure/install integration and a fixture for MC's listing parser.
> UC2 is an external runtime dependency; the helper does not modify archives.

Validation:

- Patch applies cleanly to the pinned upstream commit.
- Upstream MC builds locally with ncurses. Its actual `mc_parse_ls_l` and
  `test_all` harness accept the added listing fixture.
- Fixture covers directories, spaces, CP850, literal wildcard/bracket/semicolon
  names and stored versions. Direct helper copyout checks compare exact bytes
  for literal names and both stored versions.
- The machine lacks the Check unit-test dependency. The listing parser was
  built against libmc rebuilt with `CPPFLAGS=-DHAVE_TESTS` to enable MC's test
  mocks. This is scoped helper validation, not a full upstream test-suite pass.

Reproduce the patch check in a clean upstream checkout:

```sh
git checkout 3228c23fd57db28c20c35db8e0e1c6d3b087b3c7
git apply --check /path/to/mc-3228c23-uc2.patch
git apply /path/to/mc-3228c23-uc2.patch
./autogen.sh
./configure --enable-tests
make -j4
make -C tests/src/vfs/extfs/helpers-list check
```

The [current contribution instructions](https://midnight-commander.org/source-code/)
require an issue/PR and review. They also require DCO sign-off and an initial
`Ticket #<issue>: ...` commit title. This patch has no invented issue number
or sign-off. Confirm the contributor's sign-off before submitting, then rebase
on current master and run the normal upstream checks with Check installed.
