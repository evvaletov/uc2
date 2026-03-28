#!/bin/bash
# Create reference UC2 archives from the test corpus using the original
# UC2 Pro (UC.EXE) in DOSBox-X.
#
# Run from the UC2 project root: bash tests/scripts/create_archives.sh
#
# uc2pro.exe is a UCEXE-compressed self-extracting archive containing the
# UC2 Pro distribution.  We first extract it to get UC.EXE, then use
# UC.EXE to create the reference archives.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
ARCHIVE_DIR="$PROJECT_DIR/tests/archives"

# DOSBox-X flatpak only has filesystem=home access.
WORK_DIR="$(mktemp -d "$HOME/.cache/uc2-create-archives.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "Working in: $WORK_DIR"

cp "$PROJECT_DIR/original/UC2_source/uc2pro.exe" "$WORK_DIR/"
mkdir -p "$WORK_DIR/corpus" "$WORK_DIR/out"
cp "$PROJECT_DIR/tests/corpus/"* "$WORK_DIR/corpus/"

# Session 1: Extract UC2 Pro distribution from SFX
echo "Extracting UC2 Pro tools from uc2pro.exe (this takes ~60s in DOSBox)..."
cat > "$WORK_DIR/dosbox.conf" <<DOSBOXCFG
[sdl]
output=none
fullscreen=false
[dosbox]
memsize=16
machine=svga_s3
[cpu]
cycles=max
[autoexec]
mount c: $WORK_DIR
c:
uc2pro UC2DIST
exit
DOSBOXCFG

timeout 180 flatpak run com.dosbox_x.DOSBox-X \
    -conf "$WORK_DIR/dosbox.conf" -nopromptfolder 2>/dev/null || true

if [ ! -f "$WORK_DIR/UC2DIST/UC.EXE" ]; then
    echo "ERROR: SFX extraction failed (UC.EXE not found)"
    exit 1
fi

# Session 2: Create reference archives
echo "Creating reference archives..."
cat > "$WORK_DIR/dosbox.conf" <<DOSBOXCFG
[sdl]
output=none
fullscreen=false
[dosbox]
memsize=16
machine=svga_s3
[cpu]
cycles=max
[autoexec]
mount c: $WORK_DIR
c:
cd C:\\UC2DIST

rem Basic archive: all corpus files, Method 4 (Ultra, default)
UC a C:\\OUT\\BASIC C:\\CORPUS\\*.*

rem Empty file only
UC a C:\\OUT\\EMPTY C:\\CORPUS\\EMPTY.DAT

rem Single text file
UC a C:\\OUT\\SINGLE C:\\CORPUS\\HELLO.TXT

rem Large compressible file
UC a C:\\OUT\\ZEROS C:\\CORPUS\\ZEROS.BIN

rem Incompressible data
UC a C:\\OUT\\RANDOM C:\\CORPUS\\RANDOM.BIN

echo DONE > C:\\DONE.TXT
exit
DOSBOXCFG

timeout 600 flatpak run com.dosbox_x.DOSBox-X \
    -conf "$WORK_DIR/dosbox.conf" -nopromptfolder 2>/dev/null || true

# Copy generated archives to the project
if ls "$WORK_DIR/out/"*.UC2 >/dev/null 2>&1; then
    mkdir -p "$ARCHIVE_DIR"
    for f in "$WORK_DIR/out/"*.UC2; do
        base=$(basename "$f")
        lower=$(echo "$base" | tr '[:upper:]' '[:lower:]')
        cp "$f" "$ARCHIVE_DIR/$lower"
    done
    echo "Archives created in $ARCHIVE_DIR:"
    ls -la "$ARCHIVE_DIR/"*.uc2
else
    echo "ERROR: No archives were generated. Check DOSBox output."
    exit 1
fi
