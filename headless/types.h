#ifndef HEADLESS_TYPES_H
#define HEADLESS_TYPES_H

#include <opencv2/core.hpp>
#include <vector>
#include <string>

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct CircleOutline {
    Point2D center;
    double radius = 0.0;

    bool isValid() const { return radius > 0.0; }

    void scale(double factor) {
        center.x *= factor;
        center.y *= factor;
        radius *= factor;
    }

    void translate(double dx, double dy) {
        center.x += dx;
        center.y += dy;
    }
};

struct MirrorConfig {
    double diameter = 0.0;
    double roc = 0.0;
    double lambda = 550.0;
    double conic = 0.0;
    double obstruction = 0.0;
    double fringeSpacing = 1.0;
    bool flipV = false;
    bool flipH = false;
};

enum class ProcessMode {
    Full,
    DftPreview
};

struct ProcessConfig {
    ProcessMode mode = ProcessMode::Full;
    int dftSize = 640;
    double smoothFactor = 50.0;
    double centerFilter = 0.0;
    int zernikeTerms = 37;
};

struct Wavefront {
    cv::Mat_<double> data;
    cv::Mat_<double> surface;
    cv::Mat_<uint8_t> mask;
    std::vector<double> zernikes;

    CircleOutline outside;
    CircleOutline obstruction;

    MirrorConfig mirror;

    double rms = 0.0;
    double pv = 0.0;
    double strehl = 0.0;

    std::string name;
};

struct ProcessResult {
    bool success = false;
    std::string error;
    Wavefront wavefront;
    cv::Mat dftPreview;
};

#endif
