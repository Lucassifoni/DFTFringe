#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

PASSED=0
FAILED=0

run_test() {
    echo ""
    if bash "$1"; then
        ((PASSED++))
    else
        ((FAILED++))
    fi
}

echo "=========================================="
echo "Running sidecar tests"
echo "=========================================="

run_test test_config_quit.sh
run_test test_errors.sh
run_test test_preview.sh
run_test test_manual_outline.sh
run_test test_analyze.sh

echo ""
echo "=========================================="
echo "Results: $PASSED passed, $FAILED failed"
echo "=========================================="

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
