#ifndef HEADLESS_VORTEX_H
#define HEADLESS_VORTEX_H

#include <opencv2/opencv.hpp>
#include "types.h"

cv::Mat makeMask(const CircleOutline &outside, const CircleOutline &center,
                 int width, int height);

cv::Mat computeDftMagnitude(const cv::Mat &gray, const cv::Mat &mask);

cv::Mat extractPhase(const cv::Mat &gray, const cv::Mat &mask,
                     double centerFilter, double smoothFactor);

struct PreparedImage {
    cv::Mat image;
    cv::Mat mask;
    CircleOutline outside;
    CircleOutline center;
    double scaleFactor;
};

PreparedImage prepareImage(const cv::Mat &input,
                           const CircleOutline &outside,
                           const CircleOutline &center,
                           int dftSize);

#endif
