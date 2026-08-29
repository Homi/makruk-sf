#!/bin/bash
# deploy_net.sh — Export a trained .pt checkpoint, CRC-name it, and embed it.
# Usage: ./tools/training/deploy_net.sh tools/training/makruk_v4.pt
# Must be run from the project root (Makruk-SF/).

set -e

PT=${1:-tools/training/makruk_v4.pt}

if [ ! -f "$PT" ]; then
    echo "ERROR: checkpoint not found: $PT"
    exit 1
fi

# Export to temp binary
TMP_BIN=$(mktemp /tmp/makruk_export_XXXXXX.bin)
echo "=== Exporting $PT ==="
python3 tools/training/export.py --input "$PT" --output "$TMP_BIN"

# Compute CRC32 and rename
CRC=$(python3 -c "
import zlib, struct, sys
data = open('$TMP_BIN','rb').read()
crc = zlib.crc32(data) & 0xFFFFFFFF
print(crc)
")
NET_NAME="${CRC}.bin"
NET_PATH="src/${NET_NAME}"

cp "$TMP_BIN" "$NET_PATH"
rm "$TMP_BIN"

SIZE_MB=$(python3 -c "import os; print(f'{os.path.getsize(\"$NET_PATH\")/1024/1024:.2f}')")
echo "Net file : $NET_PATH  ($SIZE_MB MB, CRC32=$CRC)"

# Update evaluate.h
OLD=$(grep 'NnueNetDefaultName' src/evaluate.h | grep -o '"[^"]*"')
sed -i "s|NnueNetDefaultName.*|NnueNetDefaultName   \"${NET_NAME}\"|" src/evaluate.h
echo "Updated  : src/evaluate.h  $OLD → \"${NET_NAME}\""

echo ""
echo "=== Rebuild now ==="
echo "  make -C src -j\$(nproc) ARCH=x86-64-avx2 sf-kernel"
