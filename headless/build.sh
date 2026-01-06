#!/bin/bash
set -e

cd "$(dirname "$0")"

echo "Building dftfringe-cli Docker image..."
docker build -t dftfringe-cli:latest .

echo ""
echo "Build complete. Run with:"
echo "  docker run --rm -v \$(pwd)/data:/data dftfringe-cli --help"
echo ""
echo "Example:"
echo "  docker run --rm -v \$(pwd)/data:/data dftfringe-cli \\"
echo "    --input /data/interferogram.jpg \\"
echo "    --circle 320,240,200 \\"
echo "    --diameter 200 \\"
echo "    --roc 2000 \\"
echo "    --output /data/result.wft \\"
echo "    --zernikes /data/result.csv"
