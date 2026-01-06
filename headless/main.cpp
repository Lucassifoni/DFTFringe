#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>
#include <opencv2/opencv.hpp>

#include "types.h"
#include "vortex.h"
#include "punwrap.h"
#include "zernfit.h"

struct Args {
    std::string inputFile;
    std::string outlineFile;
    std::string outputWft;
    std::string outputCsv;
    std::string outputDft;

    MirrorConfig mirror;
    ProcessConfig process;

    CircleOutline outside;
    CircleOutline center;

    bool hasOutline = false;
    bool verbose = false;
    bool structuredOutput = false;
};

static void printUsage(const char *prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "\nRequired:\n"
              << "  --input <file.jpg>       Input interferogram (JPEG)\n"
              << "\nOutline (one required):\n"
              << "  --outline <file.oln>     Outline file from DFTFringe\n"
              << "  --circle <cx,cy,r>       Manual outline: center x, center y, radius\n"
              << "\nMirror config:\n"
              << "  --diameter <mm>          Mirror diameter (default: from outline radius)\n"
              << "  --roc <mm>               Radius of curvature\n"
              << "  --lambda <nm>            Wavelength (default: 550)\n"
              << "  --conic <val>            Conic constant (default: 0)\n"
              << "  --obstruction <ratio>    Central obstruction ratio 0-1 (default: 0)\n"
              << "  --fringe-spacing <val>   Fringe spacing multiplier (default: 1.0)\n"
              << "  --flip-v                 Flip vertically\n"
              << "  --flip-h                 Flip horizontally\n"
              << "  --no-null                Disable software null (for spheres)\n"
              << "\nProcessing:\n"
              << "  --dft-size <pixels>      DFT processing size (default: 640)\n"
              << "  --center-filter <val>    Center filter radius (default: 10)\n"
              << "  --smooth <val>           Smoothing factor 0-100 (default: 9)\n"
              << "  --zernike-terms <n>      Number of Zernike terms (default: 37)\n"
              << "\nModes:\n"
              << "  --dft-preview            Output DFT magnitude image only\n"
              << "\nOutput:\n"
              << "  --output <file.wft>      Output wavefront file\n"
              << "  --zernikes <file.csv>    Output Zernike coefficients\n"
              << "  --dft-output <file.png>  Output DFT preview image\n"
              << "  --verbose                Verbose output\n"
              << "  --structured-output      Output results as key<TAB>value pairs\n"
              << "  --help                   Show this help\n";
}

static bool parseOutlineFile(const std::string &path, CircleOutline &outside, CircleOutline &center) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    double ox, oy, orad, cx, cy, crad;
    f.read(reinterpret_cast<char*>(&ox), sizeof(double));
    f.read(reinterpret_cast<char*>(&oy), sizeof(double));
    f.read(reinterpret_cast<char*>(&orad), sizeof(double));
    f.read(reinterpret_cast<char*>(&cx), sizeof(double));
    f.read(reinterpret_cast<char*>(&cy), sizeof(double));
    f.read(reinterpret_cast<char*>(&crad), sizeof(double));

    outside.center.x = ox;
    outside.center.y = oy;
    outside.radius = orad;
    center.center.x = cx;
    center.center.y = cy;
    center.radius = crad;

    return true;
}

static bool parseCircle(const std::string &s, CircleOutline &c) {
    double cx, cy, r;
    if (sscanf(s.c_str(), "%lf,%lf,%lf", &cx, &cy, &r) != 3) return false;
    c.center.x = cx;
    c.center.y = cy;
    c.radius = r;
    return true;
}

static Args parseArgs(int argc, char **argv) {
    Args args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            exit(0);
        } else if (arg == "--input" && i + 1 < argc) {
            args.inputFile = argv[++i];
        } else if (arg == "--outline" && i + 1 < argc) {
            args.outlineFile = argv[++i];
        } else if (arg == "--circle" && i + 1 < argc) {
            if (parseCircle(argv[++i], args.outside)) {
                args.hasOutline = true;
            }
        } else if (arg == "--diameter" && i + 1 < argc) {
            args.mirror.diameter = std::stod(argv[++i]);
        } else if (arg == "--roc" && i + 1 < argc) {
            args.mirror.roc = std::stod(argv[++i]);
        } else if (arg == "--lambda" && i + 1 < argc) {
            args.mirror.lambda = std::stod(argv[++i]);
        } else if (arg == "--conic" && i + 1 < argc) {
            args.mirror.conic = std::stod(argv[++i]);
        } else if (arg == "--obstruction" && i + 1 < argc) {
            args.mirror.obstruction = std::stod(argv[++i]);
        } else if (arg == "--fringe-spacing" && i + 1 < argc) {
            args.mirror.fringeSpacing = std::stod(argv[++i]);
        } else if (arg == "--flip-v") {
            args.mirror.flipV = true;
        } else if (arg == "--flip-h") {
            args.mirror.flipH = true;
        } else if (arg == "--no-null") {
            args.mirror.doNull = false;
        } else if (arg == "--dft-size" && i + 1 < argc) {
            args.process.dftSize = std::stoi(argv[++i]);
        } else if (arg == "--center-filter" && i + 1 < argc) {
            args.process.centerFilter = std::stod(argv[++i]);
        } else if (arg == "--smooth" && i + 1 < argc) {
            args.process.smoothFactor = std::stod(argv[++i]);
        } else if (arg == "--zernike-terms" && i + 1 < argc) {
            args.process.zernikeTerms = std::stoi(argv[++i]);
        } else if (arg == "--dft-preview") {
            args.process.mode = ProcessMode::DftPreview;
        } else if (arg == "--output" && i + 1 < argc) {
            args.outputWft = argv[++i];
        } else if (arg == "--zernikes" && i + 1 < argc) {
            args.outputCsv = argv[++i];
        } else if (arg == "--dft-output" && i + 1 < argc) {
            args.outputDft = argv[++i];
        } else if (arg == "--verbose") {
            args.verbose = true;
        } else if (arg == "--structured-output") {
            args.structuredOutput = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            exit(1);
        }
    }

    return args;
}

static void writeWavefront(const std::string &path, const Wavefront &wf) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "Cannot write to " << path << "\n";
        return;
    }

    f << wf.data.cols << " " << wf.data.rows << "\n";
    for (int y = wf.data.rows - 1; y >= 0; --y) {
        for (int x = 0; x < wf.data.cols; ++x) {
            f << wf.data.at<double>(y, x) << "\n";
        }
    }

    f << "outside ellipse " << wf.outside.center.x << " " << wf.outside.center.y
      << " " << wf.outside.radius << " " << wf.outside.radius << "\n";
    if (wf.obstruction.radius > 0) {
        f << "obstruction ellipse " << wf.obstruction.center.x << " " << wf.obstruction.center.y
          << " " << wf.obstruction.radius << " " << wf.obstruction.radius << "\n";
    }
    f << "DIAM " << wf.mirror.diameter << "\n";
    f << "ROC " << wf.mirror.roc << "\n";
    f << "Lambda " << wf.mirror.lambda << "\n";
}

static void writeZernikes(const std::string &path, const std::vector<double> &zernikes) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "Cannot write to " << path << "\n";
        return;
    }

    f << "term,value\n";
    for (size_t i = 0; i < zernikes.size(); ++i) {
        f << i << "," << zernikes[i] << "\n";
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    Args args = parseArgs(argc, argv);

    if (args.inputFile.empty()) {
        std::cerr << "Error: --input is required\n";
        return 1;
    }

    cv::Mat input = cv::imread(args.inputFile, cv::IMREAD_COLOR);
    if (input.empty()) {
        std::cerr << "Error: Cannot read " << args.inputFile << "\n";
        return 1;
    }

    if (args.verbose) {
        std::cerr << "Loaded image: " << input.cols << "x" << input.rows << "\n";
    }

    if (!args.outlineFile.empty()) {
        if (!parseOutlineFile(args.outlineFile, args.outside, args.center)) {
            std::cerr << "Error: Cannot read outline file " << args.outlineFile << "\n";
            return 1;
        }
        args.hasOutline = true;
    }

    if (!args.hasOutline) {
        std::cerr << "Error: --outline or --circle is required\n";
        return 1;
    }

    if (args.verbose) {
        std::cerr << "Outline: center=(" << args.outside.center.x << ","
                  << args.outside.center.y << ") r=" << args.outside.radius << "\n";
    }

    if (args.mirror.diameter == 0) {
        args.mirror.diameter = args.outside.radius * 2;
    }

    PreparedImage prep = prepareImage(input, args.outside, args.center, args.process.dftSize);

    if (args.verbose) {
        std::cerr << "Prepared image: " << prep.image.cols << "x" << prep.image.rows
                  << " scale=" << prep.scaleFactor << "\n";
    }

    if (args.process.mode == ProcessMode::DftPreview) {
        cv::Mat dftMag = computeDftMagnitude(prep.image, prep.mask);

        if (!args.outputDft.empty()) {
            cv::imwrite(args.outputDft, dftMag);
            if (args.verbose) {
                std::cerr << "Wrote DFT preview to " << args.outputDft << "\n";
            }
        }

        if (args.structuredOutput) {
            std::cout << "mode\tdft_preview\n";
            std::cout << "input_file\t" << args.inputFile << "\n";
            std::cout << "input_width\t" << input.cols << "\n";
            std::cout << "input_height\t" << input.rows << "\n";
            std::cout << "outline_center_x\t" << args.outside.center.x << "\n";
            std::cout << "outline_center_y\t" << args.outside.center.y << "\n";
            std::cout << "outline_radius\t" << args.outside.radius << "\n";
            std::cout << "processed_size\t" << prep.image.cols << "\n";
            std::cout << "processed_scale_factor\t" << prep.scaleFactor << "\n";
            std::cout << "output_dft_preview\t" << (args.outputDft.empty() ? "" : args.outputDft) << "\n";
        } else if (args.outputDft.empty()) {
            std::cout << "DFT preview computed. Use --dft-output to save.\n";
        }
        return 0;
    }

    if (args.verbose) {
        std::cerr << "Extracting phase...\n";
    }

    cv::Mat phase = extractPhase(prep.image, prep.mask,
                                 args.process.centerFilter, args.process.smoothFactor);

    phase *= -1.0;

    cv::Mat result = cv::Mat::zeros(phase.size(), CV_64F);
    phase.copyTo(result, prep.mask);

    cv::normalize(result, result, 0, 1., cv::NORM_MINMAX, CV_64F, prep.mask);

    cv::Mat unwrapped = cv::Mat::zeros(result.size(), CV_64F);
    cv::Mat mask8u = (255 - prep.mask) / 255;

    unwrap(result.ptr<double>(0), unwrapped.ptr<double>(0),
           reinterpret_cast<char*>(mask8u.data), result.cols, result.rows);

    if (!args.mirror.flipV) {
        cv::flip(unwrapped, unwrapped, 0);
        prep.outside.center.y = (unwrapped.rows - 1) - prep.outside.center.y;
        prep.center.center.y = (unwrapped.rows - 1) - prep.center.center.y;
    }

    if (args.mirror.flipH) {
        cv::flip(unwrapped, unwrapped, 1);
        prep.outside.center.x = (unwrapped.cols - 1) - prep.outside.center.x;
        prep.center.center.x = (unwrapped.cols - 1) - prep.center.center.x;
    }

    if (args.mirror.fringeSpacing != 1.0) {
        unwrapped *= args.mirror.fringeSpacing;
    }

    cv::Mat finalMask = makeMask(prep.outside, prep.center, unwrapped.cols, unwrapped.rows);

    if (args.verbose) {
        std::cerr << "Fitting Zernikes (" << args.process.zernikeTerms << " terms)...\n";
    }

    std::vector<double> zernikes = fitZernikes(unwrapped, finalMask, prep.outside,
                                               args.process.zernikeTerms, false);

    double nullValue = 0.0;
    if (args.mirror.doNull && args.mirror.conic != 0.0) {
        nullValue = args.mirror.nullValue();
        if (args.verbose) {
            std::cerr << "Software null: z8=" << args.mirror.computeZ8()
                      << " × cc=" << args.mirror.conic
                      << " = " << nullValue << "\n";
        }
    }

    std::vector<double> finalZernikes = zernikes;
    if (nullValue != 0.0 && finalZernikes.size() > 8) {
        finalZernikes[8] -= nullValue;
    }

    std::vector<bool> enables = getDefaultEnables(args.process.zernikeTerms);

    cv::Mat wavefrontSurface = reconstructFromZernikes(finalMask, prep.outside,
                                                        finalZernikes, enables);

    SurfaceMetrics wfMetrics = computeMetrics(wavefrontSurface, finalMask, args.mirror.lambda);

    if (!args.outputWft.empty()) {
        Wavefront wf;
        wf.data = wavefrontSurface;
        wf.mask = finalMask;
        wf.zernikes = finalZernikes;
        wf.outside = prep.outside;
        wf.obstruction = prep.center;
        wf.mirror = args.mirror;
        wf.rms = wfMetrics.rms;
        wf.pv = wfMetrics.pv;
        wf.strehl = wfMetrics.strehl;

        writeWavefront(args.outputWft, wf);
        if (args.verbose) {
            std::cerr << "Wrote wavefront to " << args.outputWft << "\n";
        }
    }

    if (!args.outputCsv.empty()) {
        writeZernikes(args.outputCsv, finalZernikes);
        if (args.verbose) {
            std::cerr << "Wrote Zernikes to " << args.outputCsv << "\n";
        }
    }

    if (args.structuredOutput) {
        std::cout << std::setprecision(8);
        std::cout << "mode\tfull\n";
        std::cout << "input_file\t" << args.inputFile << "\n";
        std::cout << "input_width\t" << input.cols << "\n";
        std::cout << "input_height\t" << input.rows << "\n";
        std::cout << "outline_center_x\t" << args.outside.center.x << "\n";
        std::cout << "outline_center_y\t" << args.outside.center.y << "\n";
        std::cout << "outline_radius\t" << args.outside.radius << "\n";
        std::cout << "mirror_diameter\t" << args.mirror.diameter << "\n";
        std::cout << "mirror_roc\t" << args.mirror.roc << "\n";
        std::cout << "mirror_lambda\t" << args.mirror.lambda << "\n";
        std::cout << "mirror_conic\t" << args.mirror.conic << "\n";
        std::cout << "mirror_obstruction\t" << args.mirror.obstruction << "\n";
        std::cout << "mirror_fringe_spacing\t" << args.mirror.fringeSpacing << "\n";
        std::cout << "processing_dft_size\t" << args.process.dftSize << "\n";
        std::cout << "processing_center_filter\t" << args.process.centerFilter << "\n";
        std::cout << "processing_smooth_factor\t" << args.process.smoothFactor << "\n";
        std::cout << "processing_zernike_terms\t" << args.process.zernikeTerms << "\n";
        std::cout << "null_z8_computed\t" << args.mirror.computeZ8() << "\n";
        std::cout << "null_value\t" << nullValue << "\n";
        std::cout << "null_applied\t" << (args.mirror.doNull && args.mirror.conic != 0.0 ? "true" : "false") << "\n";
        for (size_t i = 0; i < zernikes.size(); ++i) {
            std::cout << "zernike_raw_" << i << "\t" << zernikes[i] << "\n";
        }
        for (size_t i = 0; i < finalZernikes.size(); ++i) {
            std::cout << "zernike_nulled_" << i << "\t" << finalZernikes[i] << "\n";
        }
        std::cout << "rms_waves\t" << wfMetrics.rms << "\n";
        std::cout << "pv_waves\t" << wfMetrics.pv << "\n";
        std::cout << "strehl\t" << wfMetrics.strehl << "\n";
        std::cout << "output_wavefront\t" << (args.outputWft.empty() ? "" : args.outputWft) << "\n";
        std::cout << "output_zernikes_csv\t" << (args.outputCsv.empty() ? "" : args.outputCsv) << "\n";
    } else {
        std::cout << "=== Raw Zernikes ===\n";
        std::cout << "Z0 (piston): " << (zernikes.size() > 0 ? zernikes[0] : 0) << "\n";
        std::cout << "Z1 (X tilt): " << (zernikes.size() > 1 ? zernikes[1] : 0) << "\n";
        std::cout << "Z2 (Y tilt): " << (zernikes.size() > 2 ? zernikes[2] : 0) << "\n";
        std::cout << "Z8 (spherical): " << (zernikes.size() > 8 ? zernikes[8] : 0) << "\n";

        if (nullValue != 0.0) {
            std::cout << "\nSoftware null: " << nullValue << "\n";
            std::cout << "Z8 (nulled): " << (finalZernikes.size() > 8 ? finalZernikes[8] : 0) << "\n";
        }

        std::cout << "\n=== Wavefront (piston/tilt/defocus/coma subtracted) ===\n";
        std::cout << "RMS: " << wfMetrics.rms << " waves\n";
        std::cout << "PV: " << wfMetrics.pv << " waves\n";
        std::cout << "Strehl: " << wfMetrics.strehl << "\n";
    }

    return 0;
}
