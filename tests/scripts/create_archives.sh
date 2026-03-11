#!/bin/bash
# Create reference UC2 archives from the test corpus using uc2pro.exe in DOSBox-X.
# Run from the UC2 project root: bash tests/scripts/create_archives.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK_DIR="$(mktemp -d)"
ARCHIVE_DIR="$PROJECT_DIR/tests/archives"

trap 'rm -rf "$WORK_DIR"' EXIT

echo "Working in: $WORK_DIR"

# Copy uc2pro.exe and corpus files into the work directory
cp "$PROJECT_DIR/original/UC2_source/uc2pro.exe" "$WORK_DIR/"
mkdir -p "$WORK_DIR/corpus"
cp "$PROJECT_DIR/tests/corpus/"* "$WORK_DIR/corpus/"
mkdir -p "$WORK_DIR/out"

# Create DOSBox-X config for headless operation
cat > "$WORK_DIR/dosbox.conf" <<'DOSBOXCFG'
[sdl]
output=none
fullscreen=false

[dosbox]
title=UC2 Test Archive Builder
memsize=16
machine=svga_s3

[cpu]
cycles=max

[autoexec]
@echo off
mount c: WORKDIR
c:

rem Basic archive: all corpus files, Method 4 (Ultra, default)
uc2pro a c:\out\basic.uc2 c:\corpus\*.*

rem Empty file only
uc2pro a c:\out\empty.uc2 c:\corpus\empty.dat

rem Single text file
uc2pro a c:\out\single.uc2 c:\corpus\hello.txt

rem Large compressible file
uc2pro a c:\out\zeros.uc2 c:\corpus\zeros.bin

rem Incompressible data
uc2pro a c:\out\random.uc2 c:\corpus\random.bin

exit
DOSBOXCFG

# Patch WORKDIR placeholder with actual path
sed -i "s|WORKDIR|$WORK_DIR|g" "$WORK_DIR/dosbox.conf"

echo "Launching DOSBox-X to create archives..."
flatpak run com.dosbox_x.DOSBox-X -conf "$WORK_DIR/dosbox.conf" -nopromptfolder 2>/dev/null || true

# Copy generated archives to the project
if ls "$WORK_DIR/out/"*.uc2 >/dev/null 2>&1; then
    mkdir -p "$ARCHIVE_DIR"
    cp "$WORK_DIR/out/"*.uc2 "$ARCHIVE_DIR/"
    echo "Archives created in $ARCHIVE_DIR:"
    ls -la "$ARCHIVE_DIR/"*.uc2
else
    echo "ERROR: No archives were generated. Check DOSBox output."
    exit 1
fi
