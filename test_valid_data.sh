#!/bin/bash
mkdir -p game/assets
# Create valid S3E: XE3U header (58453355) + 1MB payload
printf "XE3U\x00" | dd of=game/game.s3e.unpacked bs=1 count=5 2>/dev/null
dd if=/dev/zero of=game/game.s3e.unpacked bs=1 count=1048571 conv=notrunc 2>/dev/null
touch game/assets/dummy.dat

echo "=== Created valid test data ==="
ls -la game/