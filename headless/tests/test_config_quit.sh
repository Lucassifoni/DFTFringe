#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE="dftfringe-cli:latest"

echo "=== Test: config and quit ==="

RESPONSE=$(echo -e "cmd\tconfig\ndiameter\t200\nroc\t1600\nlambda\t550\nconic\t-1.0\n---\ncmd\tquit\n---" | docker run --rm -i "$IMAGE" --sidecar)

if echo "$RESPONSE" | grep -q "status	ok"; then
    echo "PASS: config/quit cycle works"
    exit 0
else
    echo "FAIL: unexpected response:"
    echo "$RESPONSE"
    exit 1
fi
