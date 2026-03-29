#!/bin/bash
# Cross-tool round-trip test: original UC2 Pro -> UC2 v3 via DOSBox-X
#
# Tests that archives created by the original 1992 UC2 Pro can be correctly
# extracted by UC2 v3 (Direction 2).
#
# Direction 1 (UC2 v3 -> original) is not yet implemented — the original
# UC2 Pro hangs reading UC2 v3 archives.  Root cause is under investigation:
# tree generation and data encoding match the original, but some bitstream-
# level difference remains (likely in the ASM decompressor's expectations
# around tree serialization or block structure).
#
# Usage: roundtrip_dosbox.sh <uc2-cli> <uc2pro.exe> <corpus-dir>

set -euo pipefail

UC2_CLI="$1"
UC2PRO="$2"
CORPUS="$3"

FILES=(hello.txt textfile.txt allbytes.bin random.bin zeros.bin)

if ! flatpak info com.dosbox_x.DOSBox-X &>/dev/null; then
    echo "SKIP: DOSBox-X not installed (flatpak com.dosbox_x.DOSBox-X)"
    exit 0
fi

WORK="$(mktemp -d "$HOME/.cache/uc2-dosbox-test.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/corpus" "$WORK/out" "$WORK/output"
for f in "${FILES[@]}"; do
    cp "$CORPUS/$f" "$WORK/corpus/"
done
cp "$UC2PRO" "$WORK/uc2pro.exe"

# --- Session 1: Extract UC2 Pro distribution from SFX ---
echo "=== Session 1: Extracting UC2 Pro tools from SFX ==="
cat > "$WORK/dosbox.conf" <<DOSBOXCFG
[sdl]
output=none
fullscreen=false
[dosbox]
memsize=16
machine=svga_s3
[cpu]
cycles=max
[autoexec]
mount c: $WORK
c:
uc2pro UC2DIST
exit
DOSBOXCFG

timeout 180 flatpak run com.dosbox_x.DOSBox-X \
    -conf "$WORK/dosbox.conf" -nopromptfolder 2>/dev/null || true

if [ ! -f "$WORK/UC2DIST/UC.EXE" ]; then
    echo "FAIL: UC2 Pro SFX extraction did not produce UC.EXE"
    ls -la "$WORK/UC2DIST/" 2>/dev/null || echo "UC2DIST directory missing"
    exit 1
fi
echo "  UC.EXE extracted ($(wc -c < "$WORK/UC2DIST/UC.EXE") bytes)"

# --- Session 2: original creates archive ---
echo "=== Session 2: UC2 Pro creates archive ==="
cat > "$WORK/dosbox.conf" <<DOSBOXCFG
[sdl]
output=none
fullscreen=false
[dosbox]
memsize=16
machine=svga_s3
[cpu]
cycles=max
[autoexec]
mount c: $WORK
c:
cd C:\\UC2DIST
UC a C:\\OUT\\DOSTEST C:\\CORPUS\\*.*
echo DONE > C:\\MARKER.TXT
exit
DOSBOXCFG

timeout 300 flatpak run com.dosbox_x.DOSBox-X \
    -conf "$WORK/dosbox.conf" -nopromptfolder 2>/dev/null || true

if [ ! -f "$WORK/MARKER.TXT" ]; then
    echo "FAIL: DOSBox session did not complete (archive creation may have timed out)"
    exit 1
fi

# --- Verify Direction 2 (original -> UC2 v3) ---
echo "--- Verifying Direction 2 ---"
FAIL=0
DOS_ARCHIVE=""
for candidate in "$WORK/out/DOSTEST.UC2" "$WORK/out/dostest.uc2"; do
    [ -f "$candidate" ] && DOS_ARCHIVE="$candidate" && break
done
if [ -z "$DOS_ARCHIVE" ]; then
    echo "FAIL: UC2 Pro did not create DOSTEST.UC2"
    ls -la "$WORK/out/" 2>/dev/null
    exit 1
fi

"$UC2_CLI" -d "$WORK/output" "$DOS_ARCHIVE"

for f in "${FILES[@]}"; do
    upper=$(echo "$f" | tr '[:lower:]' '[:upper:]')
    extracted=""
    for candidate in "$WORK/output/$f" "$WORK/output/$upper"; do
        [ -f "$candidate" ] && extracted="$candidate" && break
    done
    if [ -z "$extracted" ]; then
        echo "  FAIL: $f not extracted by UC2 v3"
        FAIL=1
        continue
    fi
    if cmp -s "$CORPUS/$f" "$extracted"; then
        echo "  OK: $f"
    else
        echo "  FAIL: $f content mismatch"
        FAIL=1
    fi
done

if [ $FAIL -ne 0 ]; then
    echo "FAILED: some files did not survive cross-tool round-trip"
    echo "Work directory preserved at: $WORK"
    trap - EXIT
    exit 1
fi

echo "PASSED: all files verified (original UC2 Pro -> UC2 v3)"
