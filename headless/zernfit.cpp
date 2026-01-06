#include "zernfit.h"
#include "zernikepolar.h"
#include <cmath>

static const int SAMPLE_WIDTH = 2;
static const int numType = CV_64F;

std::vector<double> fitZernikes(const cv::Mat &surface,
                                const cv::Mat &mask,
                                const CircleOutline &outside,
                                int numTerms,
                                bool useSvd) {
    int nx = surface.cols;
    int ny = surface.rows;

    cv::Mat_<double> A;
    cv::Mat_<double> B;
    cv::Mat_<double> X(numTerms, 1);

    int count = 0;
    if (useSvd) {
        count = cv::countNonZero(mask);
        A = cv::Mat_<double>::zeros(count, numTerms);
        B = cv::Mat_<double>::zeros(count, 1);
    } else {
        A = cv::Mat_<double>::zeros(numTerms, numTerms);
        B = cv::Mat_<double>::zeros(numTerms, 1);
    }

    int step = SAMPLE_WIDTH;
    while ((nx / step) > 100) {
        ++step;
    }

    double delta = 1.0 / outside.radius;
    int sampleCnt = 0;

    for (int y = 0; y < ny; y += step) {
        for (int x = 0; x < nx; x += step) {
            double ux = (x - outside.center.x) * delta;
            double uy = (y - outside.center.y) * delta;
            double rho = sqrt(ux * ux + uy * uy);

            if (rho <= 1.0 && mask.at<uchar>(y, x) != 0 && surface.at<double>(y, x) != 0.0) {
                double theta = atan2(uy, ux);
                zernikePolar zpolar(rho, theta, numTerms);

                for (int i = 0; i < numTerms; ++i) {
                    if (useSvd) {
                        double t = zpolar.zernike(i);
                        A(sampleCnt, i) = t;
                    } else {
                        double t = zpolar.zernike(i);
                        for (int j = 0; j < numTerms; ++j) {
                            A(i, j) += t * zpolar.zernike(j);
                        }
                        B(i) += surface.at<double>(y, x) * t;
                    }
                }

                if (useSvd) {
                    B(sampleCnt++) = surface.at<double>(y, x);
                    if (sampleCnt > count) {
                        break;
                    }
                }
            }
        }
    }

    cv::solve(A, B, X, useSvd ? cv::DECOMP_SVD : cv::DECOMP_LU);

    std::vector<double> result(numTerms, 0);
    for (int z = 0; z < X.rows; ++z) {
        result[z] = X(z);
    }

    return result;
}

cv::Mat computeNulledSurface(const cv::Mat &surface,
                             const cv::Mat &mask,
                             const CircleOutline &outside,
                             const std::vector<double> &zernikes,
                             const std::vector<bool> &enables,
                             int startTerm,
                             int lastTerm) {
    int nx = surface.cols;
    int ny = surface.rows;

    cv::Mat nulled = cv::Mat::zeros(surface.size(), numType);

    double midx = outside.center.x;
    double midy = outside.center.y;
    double rad = outside.radius;

    for (int y = 0; y < ny; ++y) {
        for (int x = 0; x < nx; ++x) {
            if (mask.at<uchar>(y, x) == 0) continue;

            double ux = (x - midx) / rad;
            double uy = (y - midy) / rad;
            double rho = sqrt(ux * ux + uy * uy);

            if (rho > 1.0) continue;

            double theta = atan2(uy, ux);
            zernikePolar zpolar(rho, theta, lastTerm);

            double surfVal = surface.at<double>(y, x);
            double zernVal = 0.0;

            for (int z = startTerm; z < lastTerm && z < (int)zernikes.size(); ++z) {
                if (z < (int)enables.size() && enables[z]) {
                    zernVal += zernikes[z] * zpolar.zernike(z);
                }
            }

            nulled.at<double>(y, x) = surfVal - zernVal;
        }
    }

    return nulled;
}

SurfaceMetrics computeMetrics(const cv::Mat &surface, const cv::Mat &mask, double lambda) {
    SurfaceMetrics metrics;

    cv::Scalar mean, stddev;
    cv::meanStdDev(surface, mean, stddev, mask);

    double minVal, maxVal;
    cv::minMaxLoc(surface, &minVal, &maxVal, nullptr, nullptr, mask);

    metrics.rms = stddev[0];
    metrics.pv = maxVal - minVal;

    double wavesRms = metrics.rms;
    metrics.strehl = exp(-pow(2.0 * M_PI * wavesRms, 2));

    return metrics;
}
