#ifndef HEADLESS_ZERNFIT_H
#define HEADLESS_ZERNFIT_H

#include <opencv2/opencv.hpp>
#include <vector>
#include "types.h"

std::vector<double> fitZernikes(const cv::Mat &surface,
                                const cv::Mat &mask,
                                const CircleOutline &outside,
                                int numTerms = 37,
                                bool useSvd = false);

cv::Mat computeNulledSurface(const cv::Mat &surface,
                             const cv::Mat &mask,
                             const CircleOutline &outside,
                             const std::vector<double> &zernikes,
                             const std::vector<bool> &enables,
                             int startTerm = 0,
                             int lastTerm = 37);

struct SurfaceMetrics {
    double rms;
    double pv;
    double strehl;
};

SurfaceMetrics computeMetrics(const cv::Mat &surface, const cv::Mat &mask, double lambda);

#endif
