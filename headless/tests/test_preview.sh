#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HEADLESS_DIR="$(dirname "$SCRIPT_DIR")"
IMAGE="dftfringe-cli:latest"

echo "=== Test: DFT preview ==="

IMAGE_B64=$(base64 -i "$HEADLESS_DIR/sample.JPG" | tr -d '\n')
OUTLINE_B64=$(base64 -i "$HEADLESS_DIR/sample.oln" | tr -d '\n')

REQUEST=$(cat <<EOF
cmd	config
dft_size	640
---
cmd	preview
image	$IMAGE_B64
outline	$OUTLINE_B64
---
cmd	quit
---
EOF
)

RESPONSE=$(echo "$REQUEST" | docker run --rm -i "$IMAGE" --sidecar)

if echo "$RESPONSE" | grep -q "status	ok" && echo "$RESPONSE" | grep -q "dft	"; then
    DFT_LINE=$(echo "$RESPONSE" | grep "^dft	")
    DFT_B64="${DFT_LINE#dft	}"
    DFT_SIZE=${#DFT_B64}
    echo "PASS: preview returned DFT data ($DFT_SIZE bytes base64)"
    exit 0
else
    echo "FAIL: unexpected response:"
    echo "$RESPONSE" | head -20
    exit 1
fi
