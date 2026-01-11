#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HEADLESS_DIR="$(dirname "$SCRIPT_DIR")"
IMAGE="dftfringe-cli:latest"

echo "=== Test: analysis with manual outline ==="

IMAGE_B64=$(base64 -i "$HEADLESS_DIR/sample.JPG" | tr -d '\n')

REQUEST=$(cat <<EOF
cmd	config
diameter	200
roc	1600
lambda	550
---
cmd	analyze
image	$IMAGE_B64
outside_cx	1029.06
outside_cy	951.79
outside_r	887.89
---
cmd	quit
---
EOF
)

RESPONSE=$(echo "$REQUEST" | docker run --rm -i "$IMAGE" --sidecar)

if echo "$RESPONSE" | grep -q "status	ok" && echo "$RESPONSE" | grep -q "^rms	"; then
    RMS=$(echo "$RESPONSE" | grep "^rms	" | cut -f2)
    echo "PASS: manual outline works, RMS = $RMS"
    exit 0
else
    echo "FAIL: unexpected response:"
    echo "$RESPONSE" | head -20
    exit 1
fi
