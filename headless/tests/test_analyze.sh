#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HEADLESS_DIR="$(dirname "$SCRIPT_DIR")"
IMAGE="dftfringe-cli:latest"

echo "=== Test: full analysis ==="

IMAGE_B64=$(base64 -i "$HEADLESS_DIR/sample.JPG" | tr -d '\n')
OUTLINE_B64=$(base64 -i "$HEADLESS_DIR/sample.oln" | tr -d '\n')

REQUEST=$(cat <<EOF
cmd	config
diameter	200
roc	1600
lambda	550
conic	-1.0
dft_size	640
zernike_terms	37
---
cmd	analyze
image	$IMAGE_B64
outline	$OUTLINE_B64
---
cmd	quit
---
EOF
)

RESPONSE=$(echo "$REQUEST" | docker run --rm -i "$IMAGE" --sidecar)

check_field() {
    if echo "$RESPONSE" | grep -q "^$1	"; then
        VALUE=$(echo "$RESPONSE" | grep "^$1	" | cut -f2)
        echo "  $1 = $VALUE"
        return 0
    else
        echo "  MISSING: $1"
        return 1
    fi
}

PASS=true

if echo "$RESPONSE" | grep -q "status	ok"; then
    echo "Analysis completed:"
    check_field "rms" || PASS=false
    check_field "pv" || PASS=false
    check_field "strehl" || PASS=false
    check_field "z0" || PASS=false
    check_field "z8" || PASS=false
    check_field "wft" || PASS=false

    if $PASS; then
        echo "PASS: all expected fields present"
        exit 0
    else
        echo "FAIL: missing fields"
        exit 1
    fi
else
    echo "FAIL: analysis failed"
    echo "$RESPONSE" | head -30
    exit 1
fi
