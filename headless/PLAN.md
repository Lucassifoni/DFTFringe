# DFTFringe Headless CLI

Minimal command-line interferogram processor without Qt GUI dependencies.

## Goal

Process interferograms from the command line, passing mirror configuration as arguments.
Orchestrate from external scripts/programs.

## Dependencies

- OpenCV (image I/O, DFT, matrix operations)
- Armadillo (Zernike LSF fitting)
- C++17 standard library

No Qt.

## Input

- JPEG interferogram image
- Outline file (.oln) or auto-detection parameters
- Mirror configuration (diameter, ROC, lambda, conic, obstruction)
- Processing options (flip, fringe spacing, DFT size)

## Output

- Wavefront file (.wft)
- Zernike coefficients (CSV or stdout)
- Metrics (RMS, PV, Strehl)

## Processing Pipeline

```
1. Load JPEG → cv::Mat (grayscale)
2. Load/compute outline (CircleOutline)
3. Create mask from outline
4. vortex() → wrapped phase
5. unwrap() → unwrapped phase
6. Apply flips, fringe spacing scaling
7. fit_zernikes() → Zernike coefficients
8. Compute metrics
9. Write outputs
```

## Files

| File | Source | Changes |
|------|--------|---------|
| punwrap.cpp/h | Copy from main | None |
| zernikepolar.cpp/h | Copy from main | Remove QDebug |
| types.h | New | MirrorConfig, Outline, Wavefront structs |
| vortex.cpp/h | Extract from dftarea.cpp | Remove QImage, debug callbacks |
| zernfit.cpp/h | Extract from zernikeprocess.cpp | Remove Qt singletons, pass params |
| main.cpp | New | CLI arg parsing, orchestration |
| CMakeLists.txt | New | OpenCV + Armadillo only |

## CLI Usage

```bash
dftfringe-cli \
  --input image.jpg \
  --outline outside.oln \
  --diameter 200 \
  --roc 2000 \
  --lambda 550 \
  --conic 0 \
  --obstruction 0.3 \
  --flip-v \
  --fringe-spacing 1.0 \
  --dft-size 640 \
  --output result.wft \
  --zernikes result.csv
```

## Build

### Docker (recommended)

```bash
cd headless
./build.sh
```

Or manually:

```bash
docker build -t dftfringe-cli:latest .
```

### Run

```bash
docker run --rm -v $(pwd)/data:/data dftfringe-cli \
  --input /data/interferogram.jpg \
  --circle 320,240,200 \
  --diameter 200 \
  --roc 2000 \
  --output /data/result.wft \
  --zernikes /data/result.csv
```

### Native build (requires OpenCV)

```bash
cd headless
mkdir build && cd build
cmake ..
make
```
