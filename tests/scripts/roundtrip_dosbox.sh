#!/bin/bash
# Cross-tool round-trip test: original UC2 Pro -> UC2 v3 via DOSBox-X
#
# Tests that archives created by the original 1992 UC2 Pro can be correctly
# extracted by UC2 v3 (Direction 2).  Single-file UC2 v3 archives are also
# readable by the original, but multi-file archives still hang — the exact
# cause is under investigation (treegen + cdir encoding match the original
# for single-file cdirs but diverge for multi-file cdirs).
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

# SFX decompression takes 3-8 minutes depending on host CPU speed
timeout 600 flatpak run com.dosbox_x.DOSBox-X \
    -conf "$WORK/dosbox.conf" -nopromptfolder 2>/dev/null || true

UC2DIST_COUNT=$(ls "$WORK/UC2DIST/" 2>/dev/null | wc -l)
if [ ! -f "$WORK/UC2DIST/UC.EXE" ] || [ "$UC2DIST_COUNT" -lt 22 ]; then
    echo "FAIL: UC2 Pro SFX extraction incomplete ($UC2DIST_COUNT/22 files)"
    exit 1
fi
echo "  UC.EXE extracted ($(wc -c < "$WORK/UC2DIST/UC.EXE") bytes, $UC2DIST_COUNT files)"

# --- Direction 1: UC2 v3 creates, original extracts (single file) ---
echo "=== Direction 1: UC2 v3 creates -> original extracts ==="
"$UC2_CLI" -w "$WORK/v3single.uc2" "$WORK/corpus/hello.txt"
mkdir -p "$WORK/dir1_out"
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
cd C:\\DIR1_OUT
C:\\UC2DIST\\UC eF C:\\V3SINGLE *.*
echo DIR1 > C:\\DIR1.TXT
exit
DOSBOXCFG

timeout 60 flatpak run com.dosbox_x.DOSBox-X \
    -conf "$WORK/dosbox.conf" -nopromptfolder 2>/dev/null || true

# --- Session 2: original creates archive ---
echo "=== Session 2 (Direction 2): UC2 Pro creates archive ==="
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
    echo "FAIL: DOSBox session did not complete"
    exit 1
fi

DOS_ARCHIVE=""
for candidate in "$WORK/out/DOSTEST.UC2" "$WORK/out/dostest.uc2"; do
    [ -f "$candidate" ] && DOS_ARCHIVE="$candidate" && break
done
if [ -z "$DOS_ARCHIVE" ]; then
    echo "FAIL: UC2 Pro did not create DOSTEST.UC2"
    exit 1
fi
echo "  Archive created: $(wc -c < "$DOS_ARCHIVE") bytes"

# --- Extract with UC2 v3 and verify ---
echo "=== Extracting with UC2 v3 ==="
"$UC2_CLI" -d "$WORK/output" "$DOS_ARCHIVE"

FAIL=0
for f in "${FILES[@]}"; do
    upper=$(echo "$f" | tr '[:lower:]' '[:upper:]')
    extracted=""
    for candidate in "$WORK/output/$f" "$WORK/output/$upper"; do
        [ -f "$candidate" ] && extracted="$candidate" && break
    done
    if [ -z "$extracted" ]; then
        echo "  FAIL: $f not extracted"
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

# --- Verify Direction 1 (single-file) ---
echo "--- Verifying Direction 1 (UC2 v3 -> original, single file) ---"
if [ -f "$WORK/DIR1.TXT" ]; then
    extracted=""
    for candidate in "$WORK/dir1_out/HELLO.TXT" "$WORK/dir1_out/hello.txt"; do
        [ -f "$candidate" ] && extracted="$candidate" && break
    done
    if [ -n "$extracted" ] && cmp -s "$WORK/corpus/hello.txt" "$extracted"; then
        echo "  OK: hello.txt (Direction 1)"
    else
        echo "  FAIL: hello.txt content mismatch (Direction 1)"
        FAIL=1
    fi
else
    echo "  FAIL: Direction 1 DOSBox session incomplete"
    FAIL=1
fi

if [ $FAIL -ne 0 ]; then
    echo "FAILED: some files did not survive cross-tool round-trip"
    echo "Work directory preserved at: $WORK"
    trap - EXIT
    exit 1
fi

echo "PASSED: all files verified (both directions)"
