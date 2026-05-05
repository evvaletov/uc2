#!/bin/bash
# Smoke test for the DJGPP-built uc2.exe via DOSBox-X.
#
# Verifies the cross-compiled DOS binary actually loads under a real
# DPMI host and produces expected output for `uc2 -h` and `uc2 -l`.
#
# Usage:
#   tests/scripts/dos_smoke.sh <uc2.exe> <CWSDPMI.EXE> [<list-archive>]
#
# Where:
#   <uc2.exe>          DJGPP-built DOS binary
#                      (e.g. build-djgpp/cli/uc2.exe)
#   <CWSDPMI.EXE>      DPMI extender from csdpmi7b.zip
#                      (http://www.delorie.com/pub/djgpp/current/v2misc/csdpmi7b.zip)
#   <list-archive>     Optional: small UC2 archive to test 'uc2 -l' against
#                      (e.g. tests/archives/basic.uc2)

set -euo pipefail

UC2_EXE="${1:?usage: dos_smoke.sh <uc2.exe> <CWSDPMI.EXE> [<list-archive>]}"
CWSDPMI="${2:?usage: dos_smoke.sh <uc2.exe> <CWSDPMI.EXE> [<list-archive>]}"
LIST_ARCHIVE="${3:-}"

if [ ! -f "$UC2_EXE" ]; then
    echo "SKIP: uc2.exe not found at $UC2_EXE (run the DJGPP build first)"
    exit 0
fi
if [ ! -f "$CWSDPMI" ]; then
    echo "SKIP: CWSDPMI.EXE not found at $CWSDPMI"
    exit 0
fi
if ! flatpak info com.dosbox_x.DOSBox-X &>/dev/null; then
    echo "SKIP: DOSBox-X not installed (flatpak com.dosbox_x.DOSBox-X)"
    exit 0
fi

WORK="$(mktemp -d "$HOME/.cache/uc2-dos-smoke.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

cp "$UC2_EXE" "$WORK/UC2.EXE"
cp "$CWSDPMI" "$WORK/CWSDPMI.EXE"

ARCHIVE_NAME=""
if [ -n "$LIST_ARCHIVE" ] && [ -f "$LIST_ARCHIVE" ]; then
    ARCHIVE_NAME="$(basename "$LIST_ARCHIVE" | tr '[:lower:]' '[:upper:]')"
    cp "$LIST_ARCHIVE" "$WORK/$ARCHIVE_NAME"
fi

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
UC2 -h > HELP.TXT
echo HELPDONE > HELPMRK.TXT
${ARCHIVE_NAME:+UC2 -l $ARCHIVE_NAME > LIST.TXT}
${ARCHIVE_NAME:+echo LISTDONE > LISTMRK.TXT}
exit
DOSBOXCFG

echo "=== Running uc2.exe under DOSBox-X ==="
timeout 60 flatpak run com.dosbox_x.DOSBox-X \
    -conf "$WORK/dosbox.conf" -nopromptfolder 2>/dev/null || true

FAIL=0

# --- Validate uc2 -h ---
if [ ! -f "$WORK/HELPMRK.TXT" ]; then
    echo "  FAIL: DOSBox session did not complete (no HELPMRK.TXT)"
    FAIL=1
elif [ ! -f "$WORK/HELP.TXT" ]; then
    echo "  FAIL: uc2 -h produced no output"
    FAIL=1
elif ! grep -qi "UltraCompressor\|UC2" "$WORK/HELP.TXT"; then
    echo "  FAIL: uc2 -h output missing expected text"
    head -20 "$WORK/HELP.TXT"
    FAIL=1
else
    echo "  OK: uc2 -h"
fi

# --- Optional: validate uc2 -l ---
if [ -n "$ARCHIVE_NAME" ]; then
    if [ ! -f "$WORK/LISTMRK.TXT" ]; then
        echo "  FAIL: uc2 -l did not complete"
        FAIL=1
    elif [ ! -s "$WORK/LIST.TXT" ]; then
        echo "  FAIL: uc2 -l produced empty output"
        FAIL=1
    else
        echo "  OK: uc2 -l $ARCHIVE_NAME"
    fi
fi

if [ $FAIL -ne 0 ]; then
    echo "FAILED: DOS smoke test"
    echo "Work directory preserved at: $WORK"
    trap - EXIT
    exit 1
fi

echo "PASSED: DJGPP-built uc2.exe runs under DOSBox-X"
