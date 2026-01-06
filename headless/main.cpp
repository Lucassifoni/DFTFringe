#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
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
              << "\nProcessing:\n"
              << "  --dft-size <pixels>      DFT processing size (default: 640)\n"
              << "  --center-filter <val>    Center filter radius (default: 0)\n"
              << "  --smooth <val>           Smoothing factor 0-100 (default: 50)\n"
              << "  --zernike-terms <n>      Number of Zernike terms (default: 37)\n"
              << "\nModes:\n"
              << "  --dft-preview            Output DFT magnitude image only\n"
              << "\nOutput:\n"
              << "  --output <file.wft>      Output wavefront file\n"
              << "  --zernikes <file.csv>    Output Zernike coefficients\n"
              << "  --dft-output <file.png>  Output DFT preview image\n"
              << "  --verbose                Verbose output\n"
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
    for (int y = 0; y < wf.data.rows; ++y) {
        for (int x = 0; x < wf.data.cols; ++x) {
            f << wf.data.at<double>(y, x);
            if (x < wf.data.cols - 1) f << " ";
        }
        f << "\n";
    }

    f << "outside " << wf.outside.center.x << " " << wf.outside.center.y
      << " " << wf.outside.radius << "\n";
    f << "obstruction " << wf.obstruction.center.x << " " << wf.obstruction.center.y
      << " " << wf.obstruction.radius << "\n";
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
        } else {
            std::cout << "DFT preview computed. Use --dft-output to save.\n";
        }
        return 0;
    }

    if (args.verbose) {
        std::cerr << "Extracting phase...\n";
    }

    cv::Mat phase = extractPhase(prep.image, prep.mask,
                                 args.process.centerFilter, args.process.smoothFactor);

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

    SurfaceMetrics metrics = computeMetrics(unwrapped, finalMask, args.mirror.lambda);

    std::cout << "RMS: " << metrics.rms << " waves\n";
    std::cout << "PV: " << metrics.pv << " waves\n";
    std::cout << "Strehl: " << metrics.strehl << "\n";

    if (!args.outputWft.empty()) {
        Wavefront wf;
        wf.data = unwrapped;
        wf.mask = finalMask;
        wf.zernikes = zernikes;
        wf.outside = prep.outside;
        wf.obstruction = prep.center;
        wf.mirror = args.mirror;
        wf.rms = metrics.rms;
        wf.pv = metrics.pv;
        wf.strehl = metrics.strehl;

        writeWavefront(args.outputWft, wf);
        if (args.verbose) {
            std::cerr << "Wrote wavefront to " << args.outputWft << "\n";
        }
    }

    if (!args.outputCsv.empty()) {
        writeZernikes(args.outputCsv, zernikes);
        if (args.verbose) {
            std::cerr << "Wrote Zernikes to " << args.outputCsv << "\n";
        }
    }

    return 0;
}
