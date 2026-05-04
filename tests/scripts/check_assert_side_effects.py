#!/usr/bin/env python3
# Fails when assert(...) wraps a function call with side effects.
#
# Background: under -DNDEBUG (CMake's default for Release) the assert macro
# expands to ((void)0) and the wrapped expression is not evaluated.  Any work
# done inside assert() is silently dropped.  This has cost the project two
# CI rounds:
#   - dae8a50: int-truncation in test_merkle / test_dict Debug builds
#   - 6d8087f: test_delta double-free under Release / Windows MSVC
#
# Rule: tests must capture the call result first, then assert on it:
#   int rc = call(...);
#   assert(rc == EXPECTED);
#
# This script detects the dangerous form by matching assert() that wraps a
# call to a function whose name contains a side-effect verb.  Pure queries
# (_equal, _match, _verify, _has_, _is_, _root, _id, _hash, _attest_name,
# memcmp, strcmp, ...) are allowed.

import re
import sys
from pathlib import Path

SIDE_EFFECT_VERBS = (
    "encode", "decode", "parse", "serialize", "deserialize",
    "build", "init", "write", "read_file", "attach", "extract",
    "compress", "decompress", "create", "destroy", "open", "close",
    "flush", "push", "pop", "append", "insert", "remove", "update",
    "store", "load", "put", "finalize", "process", "run", "step",
    "alloc", "free", "register", "submit", "commit", "rollback",
)

SCAN_DIRS = ("tests/src", "lib/src", "cli/src", "src")

assert_call_re = re.compile(
    r"assert\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\("
)

verb_re = re.compile(
    r"(?:^|_)(" + "|".join(SIDE_EFFECT_VERBS) + r")(?:_|$)"
)


def scan(root: Path) -> list[tuple[Path, int, str, str]]:
    findings = []
    for d in SCAN_DIRS:
        base = root / d
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*.c")):
            for lineno, line in enumerate(path.read_text(encoding="utf-8",
                                                         errors="replace").splitlines(),
                                          start=1):
                # Skip comments quickly.  Not perfect but adequate here.
                stripped = line.lstrip()
                if stripped.startswith("//") or stripped.startswith("*"):
                    continue
                m = assert_call_re.search(line)
                if not m:
                    continue
                ident = m.group(1)
                if verb_re.search(ident):
                    findings.append((path, lineno, ident, line.rstrip()))
    return findings


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    findings = scan(repo_root)
    if not findings:
        print("OK: no side-effecting asserts found.")
        return 0
    print("ERROR: assert() must not wrap calls with side effects.", file=sys.stderr)
    print("Under -DNDEBUG (Release builds) the call is dropped, leaving", file=sys.stderr)
    print("output parameters uninitialised and the test silently no-op.", file=sys.stderr)
    print("Convert to: int rc = call(...); assert(rc == EXPECTED);", file=sys.stderr)
    print(file=sys.stderr)
    for path, lineno, ident, line in findings:
        rel = path.relative_to(repo_root)
        print(f"{rel}:{lineno}: {ident}", file=sys.stderr)
        print(f"    {line.strip()}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
