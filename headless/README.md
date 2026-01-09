# DFTFringe Headless CLI

Minimal command-line interferogram processor extracted from [DFTFringe](https://github.com/githubdoe/DFTFringe). Processes interferograms without requiring Qt or a graphical environment.

## Features

- Load JPEG interferograms
- Use outline files (.oln) from DFTFringe or specify geometry manually
- DFT-based phase extraction with vortex algorithm
- Quality-guided phase unwrapping
- Zernike polynomial fitting (up to 49 terms)
- Output wavefront files (.wft) compatible with DFTFringe
- Export Zernike coefficients to CSV
- DFT preview mode for tuning center filter

## Dependencies

- OpenCV (image I/O, DFT, matrix operations)
- C++17 compiler

No Qt required.

## Building

### Docker (recommended)

```bash
./build.sh
```

Or manually:

```bash
docker build -t dftfringe-cli:latest .
```

### Native build

Requires OpenCV development libraries installed.

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

### Full processing

```bash
docker run --rm -v $(pwd):/data dftfringe-cli \
  --input /data/sample.jpg \
  --outline /data/sample.oln \
  --diameter 203 \
  --roc 1438 \
  --lambda 518 \
  --output /data/result.wft \
  --zernikes /data/result.csv \
  --verbose
```

### DFT preview (for tuning center filter)

```bash
docker run --rm -v $(pwd):/data dftfringe-cli \
  --input /data/sample.jpg \
  --outline /data/sample.oln \
  --dft-preview \
  --dft-output /data/dft_preview.png
```

### Manual outline specification

If you don't have an .oln file, specify the mirror outline manually:

```bash
docker run --rm -v $(pwd):/data dftfringe-cli \
  --input /data/image.jpg \
  --circle 320,240,200 \
  --diameter 203 \
  --roc 1438 \
  --output /data/result.wft
```

## Command-Line Options

### Required

| Option | Description |
|--------|-------------|
| `--input <file>` | Input interferogram (JPEG) |
| `--outline <file>` | Outline file from DFTFringe |
| `--circle <cx,cy,r>` | Manual outline: center x, center y, radius |

One of `--outline` or `--circle` is required.

### Mirror Configuration

| Option | Default | Description |
|--------|---------|-------------|
| `--diameter <mm>` | from outline | Mirror diameter |
| `--roc <mm>` | - | Radius of curvature |
| `--lambda <nm>` | 550 | Wavelength |
| `--conic <val>` | 0 | Conic constant (-1=parabola, -1.33=hyperbola, 0=sphere) |
| `--no-null` | false | Disable software null subtraction |
| `--obstruction <ratio>` | 0 | Central obstruction (0-1) |
| `--fringe-spacing <val>` | 1.0 | Fringe spacing multiplier |
| `--flip-v` | false | Flip vertically |
| `--flip-h` | false | Flip horizontally |
| `--invert` | false | Force wavefront inversion |
| `--no-auto-invert` | false | Disable automatic inversion detection |

### Processing

| Option | Default | Description |
|--------|---------|-------------|
| `--dft-size <pixels>` | 640 | DFT processing size |
| `--center-filter <val>` | 10 | High-pass filter radius |
| `--smooth <val>` | 9 | Smoothing factor (0-100) |
| `--zernike-terms <n>` | 37 | Number of Zernike terms to fit |

### Modes

| Option | Description |
|--------|-------------|
| `--dft-preview` | Output DFT magnitude image only (skip processing) |

### Output

| Option | Description |
|--------|-------------|
| `--output <file.wft>` | Output wavefront file |
| `--zernikes <file.csv>` | Output Zernike coefficients |
| `--dft-output <file.png>` | Output DFT preview image |
| `--verbose` | Verbose output |
| `--structured-output` | Output results as key-value pairs (tab-separated) |

## Output Formats

### Structured output (--structured-output)

Tab-separated key-value pairs for easy parsing:

```
mode	full
input_file	/path/to/image.jpg
input_width	3456
input_height	2304
outline_center_x	1672.5
outline_center_y	1075
outline_radius	568
mirror_diameter	203
mirror_roc	1438
mirror_lambda	518
mirror_conic	-1.33
...
inversion_detected	false
inversion_applied	false
zernike_raw_0	9.689
zernike_raw_1	17.017
...
zernike_nulled_0	9.689
zernike_nulled_8	-0.003
...
rms_waves	0.058
pv_waves	0.36
strehl	0.87
output_wavefront	/path/to/result.wft
output_zernikes_csv	/path/to/zernikes.csv
```

### Wavefront file (.wft)

Text format compatible with DFTFringe:
```
<width> <height>
<phase data row by row>
outside <cx> <cy> <radius>
obstruction <cx> <cy> <radius>
DIAM <diameter>
ROC <roc>
Lambda <lambda>
```

### Zernike CSV

```csv
term,value
0,0.123456
1,-0.234567
...
```

## Testing

Run the test script with sample data:

```bash
./test-run.sh
```

This processes `sample.jpg` with `sample.oln` and outputs results to `test_outputs/`.

## Software Null for Aspheric Mirrors

When testing aspheric mirrors (parabolas, hyperbolas), the expected spherical aberration must be subtracted to see the residual error. This is the "software null."

The expected Z8 (spherical aberration) for a parabola is:

```
Z8 @550nm = D / (1.1264 × (F/D)³)
```

For other conics, multiply by the absolute conic constant:

```
Null value = Z8 × conic
```

**Example:** 203mm f/3.54 hyperbolic mirror (cc=-1.33) at 518nm:
- Z8 @550nm = 203 / (1.1264 × 3.54³) = 4.06 waves
- Z8 @518nm = 4.06 × (550/518) = 4.31 waves
- Null value = 4.31 × -1.33 = -5.73 waves

The CLI automatically applies this null when `--conic` is specified (unless `--no-null` is used).

## Auto-Inversion Detection

Interferogram phase can be ambiguous in sign. The CLI automatically detects and corrects inverted wavefronts when a non-zero conic constant is provided.

**Detection method:** If `conic × Z8 < 0`, the wavefront is likely inverted. For example:
- A hyperbolic mirror (cc=-1.33) should have *negative* spherical aberration (undercorrected)
- If Z8 comes out positive, the wavefront is inverted

**Behavior:**
- Auto-inversion is **enabled by default** when `--conic` is non-zero
- Use `--no-auto-invert` to disable automatic detection
- Use `--invert` to force inversion regardless of detection

**Output fields:**
- `inversion_detected`: Whether the conic × Z8 test indicated inversion
- `inversion_applied`: Whether the wavefront was actually inverted

For spherical mirrors (conic=0), auto-inversion cannot determine the correct sign. Use `--invert` or `--no-auto-invert` and verify manually.

## Processing Pipeline

1. **Load image** - Read JPEG via OpenCV
2. **Load outline** - Parse .oln binary file or use manual coordinates
3. **Prepare image** - Crop ROI, scale to DFT size, create mask
4. **DFT preview** (optional) - Output magnitude spectrum for filter tuning
5. **Phase extraction** - Vortex algorithm with high-pass filtering
6. **Phase unwrapping** - Quality-guided path follower
7. **Zernike fitting** - Least squares fit to Zernike polynomials
8. **Metrics** - Compute RMS, PV, Strehl ratio
9. **Output** - Write .wft file and/or CSV

## License

GPL v3 (inherited from DFTFringe)

## Credits

Core algorithms extracted from [DFTFringe](https://github.com/githubdoe/DFTFringe) by Dale Eason.
