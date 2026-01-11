#!/bin/bash
set -e

IMAGE="dftfringe-cli:latest"

echo "=== Test: error handling ==="

PASS=true

echo -n "  Unknown command... "
RESPONSE=$(echo -e "cmd\tunknown\n---" | docker run --rm -i "$IMAGE" --sidecar 2>&1 || true)
if echo "$RESPONSE" | grep -q "status	error"; then
    echo "PASS"
else
    echo "FAIL"
    PASS=false
fi

echo -n "  Missing cmd... "
RESPONSE=$(echo -e "foo\tbar\n---" | docker run --rm -i "$IMAGE" --sidecar 2>&1 || true)
if echo "$RESPONSE" | grep -q "status	error"; then
    echo "PASS"
else
    echo "FAIL"
    PASS=false
fi

echo -n "  Invalid image... "
RESPONSE=$(echo -e "cmd\tanalyze\nimage\tnotvalidbase64!!!\noutside_cx\t100\noutside_cy\t100\noutside_r\t50\n---\ncmd\tquit\n---" | docker run --rm -i "$IMAGE" --sidecar 2>&1 || true)
if echo "$RESPONSE" | grep -q "status	error"; then
    echo "PASS"
else
    echo "FAIL"
    PASS=false
fi

echo -n "  Missing outline... "
RESPONSE=$(echo -e "cmd\tanalyze\nimage\tYWJj\n---\ncmd\tquit\n---" | docker run --rm -i "$IMAGE" --sidecar 2>&1 || true)
if echo "$RESPONSE" | grep -q "status	error"; then
    echo "PASS"
else
    echo "FAIL"
    PASS=false
fi

if $PASS; then
    echo "All error tests passed"
    exit 0
else
    echo "Some error tests failed"
    exit 1
fi
