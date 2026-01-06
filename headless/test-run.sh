#!/bin/bash
set -e

cd "$(dirname "$0")"

SAMPLE_JPG="sample.jpg"
SAMPLE_OLN="sample.oln"
OUTPUT_DIR="test_outputs"

DIAMETER=203
ROC=1438
LAMBDA=518
CONIC=-1.33  # hyperbolic mirror

if [ ! -f "$SAMPLE_JPG" ]; then
    echo "Error: $SAMPLE_JPG not found"
    exit 1
fi

if [ ! -f "$SAMPLE_OLN" ]; then
    echo "Error: $SAMPLE_OLN not found"
    exit 1
fi

if ! docker image inspect dftfringe-cli:latest >/dev/null 2>&1; then
    echo "Docker image not found. Building..."
    ./build.sh
fi

mkdir -p "$OUTPUT_DIR"

echo "=== DFT Preview ==="
docker run --rm -v "$(pwd):/data" dftfringe-cli \
    --input "/data/$SAMPLE_JPG" \
    --outline "/data/$SAMPLE_OLN" \
    --dft-preview \
    --dft-output "/data/$OUTPUT_DIR/dft_preview.png" \
    --verbose 2>&1 | tee "$OUTPUT_DIR/dft_preview.log"

echo ""
echo "=== Full Processing ==="
docker run --rm -v "$(pwd):/data" dftfringe-cli \
    --input "/data/$SAMPLE_JPG" \
    --outline "/data/$SAMPLE_OLN" \
    --diameter $DIAMETER \
    --roc $ROC \
    --lambda $LAMBDA \
    --conic $CONIC \
    --output "/data/$OUTPUT_DIR/result.wft" \
    --zernikes "/data/$OUTPUT_DIR/zernikes.csv" \
    --verbose 2>&1 | tee "$OUTPUT_DIR/processing.log"

echo ""
echo "=== Structured Output Test ==="
docker run --rm -v "$(pwd):/data" dftfringe-cli \
    --input "/data/$SAMPLE_JPG" \
    --outline "/data/$SAMPLE_OLN" \
    --diameter $DIAMETER \
    --roc $ROC \
    --lambda $LAMBDA \
    --conic $CONIC \
    --output "/data/$OUTPUT_DIR/result.wft" \
    --zernikes "/data/$OUTPUT_DIR/zernikes.csv" \
    --structured-output 2>&1 | tee "$OUTPUT_DIR/structured.tsv"

echo ""
echo "=== Results ==="
echo "Output directory: $OUTPUT_DIR/"
ls -la "$OUTPUT_DIR/"

if [ -f "$OUTPUT_DIR/zernikes.csv" ]; then
    echo ""
    echo "=== Zernike Coefficients (first 10) ==="
    head -11 "$OUTPUT_DIR/zernikes.csv"
fi

if [ -f "$OUTPUT_DIR/structured.tsv" ]; then
    echo ""
    echo "=== Structured Output (first 20 lines) ==="
    head -20 "$OUTPUT_DIR/structured.tsv"
fi
