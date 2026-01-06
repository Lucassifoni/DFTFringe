#include "vortex.h"
#include <cmath>

using namespace cv;

static const int numType = CV_64F;

cv::Mat makeMask(const CircleOutline &outside, const CircleOutline &center,
                 int width, int height) {
    double radm = ceil(outside.radius) + 1;
    double rado = center.radius;
    double cx = outside.center.x;
    double cy = outside.center.y;

    cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double dx = (double)(x - cx) / radm;
            double dy = (double)(y - cy) / radm;
            if (sqrt(dx * dx + dy * dy) <= 1.)
                mask.at<uchar>(y, x) = 255;
        }
    }

    if (rado > 0) {
        double ocx = center.center.x;
        double ocy = center.center.y;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                double dx = (double)(x - ocx);
                double dy = (double)(y - ocy);
                if (sqrt(dx * dx + dy * dy) <= rado)
                    mask.at<uchar>(y, x) = 0;
            }
        }
    }

    return mask;
}

static void shiftDFT(cv::Mat &mag) {
    int cx = mag.cols / 2;
    int cy = mag.rows / 2;

    cv::Mat q0(mag, cv::Rect(0, 0, cx, cy));
    cv::Mat q1(mag, cv::Rect(cx, 0, cx, cy));
    cv::Mat q2(mag, cv::Rect(0, cy, cx, cy));
    cv::Mat q3(mag, cv::Rect(cx, cy, cx, cy));

    cv::Mat tmp;
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);

    q1.copyTo(tmp);
    q2.copyTo(q1);
    tmp.copyTo(q2);
}

cv::Mat computeDftMagnitude(const cv::Mat &gray, const cv::Mat &mask) {
    cv::Mat floatImg;
    gray.convertTo(floatImg, CV_32F);

    cv::Scalar mean = cv::mean(floatImg, mask);
    floatImg -= mean[0];

    cv::Mat planes[] = {floatImg, cv::Mat::zeros(floatImg.size(), CV_32F)};
    cv::Mat complexI;
    cv::merge(planes, 2, complexI);

    cv::dft(complexI, complexI);

    cv::split(complexI, planes);
    cv::magnitude(planes[0], planes[1], planes[0]);

    cv::Mat magI = planes[0];
    magI += cv::Scalar::all(1);
    cv::log(magI, magI);

    shiftDFT(magI);

    cv::normalize(magI, magI, 0, 255, cv::NORM_MINMAX);
    magI.convertTo(magI, CV_8U);

    return magI;
}

cv::Mat extractPhase(const cv::Mat &gray, const cv::Mat &mask,
                     double centerFilter, double smoothFactor) {
    cv::Mat floatImg;
    gray.convertTo(floatImg, CV_32F);

    cv::Scalar mean = cv::mean(floatImg, mask);
    floatImg -= mean[0];

    cv::Mat planes[] = {floatImg, cv::Mat::zeros(floatImg.size(), CV_32F)};
    cv::Mat imageMat;
    cv::merge(planes, 2, imageMat);

    imageMat.convertTo(imageMat, numType);

    int xsize = imageMat.cols;
    int ysize = imageMat.rows;
    int size = xsize * ysize;

    double smooth = 0.01 * smoothFactor * xsize / 2.;

    int *ix = new int[xsize];
    int *iy = new int[ysize];
    for (int i = 0; i <= xsize / 2; ++i) ix[i] = -i;
    for (int i = 1; i <= xsize / 2; ++i) ix[xsize - i] = i;
    for (int i = 0; i <= ysize / 2; ++i) iy[i] = -i;
    for (int i = 1; i <= ysize / 2; ++i) iy[ysize - i] = i;

    double *rho = new double[size];
    double *theta = new double[size];

    for (int j = 0; j < ysize; ++j) {
        int base = j * xsize;
        for (int i = 0; i < xsize; ++i) {
            rho[base + i] = sqrt(ix[i] * ix[i] + iy[j] * iy[j]);
            theta[base + i] = atan2(iy[j], ix[i]);
        }
    }
    delete[] ix;
    delete[] iy;

    cv::Mat fdomMat;
    cv::Mat fdomPlanes[2];

    cv::dft(imageMat, fdomMat);
    cv::split(fdomMat, fdomPlanes);

    double *planesRe = fdomPlanes[0].ptr<double>(0);
    double *planesIm = fdomPlanes[1].ptr<double>(0);

    if (centerFilter > 0) {
        for (int i = 0; i < size; ++i) {
            double a = 1.0 - exp(-(rho[i] * rho[i]) / (centerFilter * centerFilter));
            planesRe[i] *= a;
            planesIm[i] *= a;
        }
    }

    cv::merge(fdomPlanes, 2, fdomMat);
    cv::dft(fdomMat, imageMat, DFT_INVERSE);

    cv::Mat imPlanes[2];
    cv::split(imageMat, imPlanes);
    imPlanes[0] /= size;

    cv::Mat tmp;
    imPlanes[0].copyTo(tmp, mask);
    imPlanes[0] = tmp.clone();

    cv::Scalar meanVal, stdVal;
    cv::meanStdDev(imPlanes[0], meanVal, stdVal, mask);

    double sum = 0;
    int count = 0;
    double *q = imPlanes[0].ptr<double>(0);
    for (int i = 0; i < size; ++i) {
        if (mask.data[i] != 0) {
            sum += q[i];
            if (q[i] != 0.0) ++count;
        }
    }
    double m2 = (count > 0) ? sum / count : 0;
    imPlanes[0] -= m2;
    imPlanes[1] *= 0.;

    double *imRe = imPlanes[0].ptr<double>(0);

    cv::merge(imPlanes, 2, imageMat);
    cv::dft(imageMat, fdomMat);

    double *spiralRe = new double[size];
    double *spiralIm = new double[size];
    for (int i = 0; i < size; ++i) {
        spiralRe[i] = cos(theta[i]);
        spiralIm[i] = sin(theta[i]);
    }

    cv::split(fdomMat, fdomPlanes);
    double *fdomRe = fdomPlanes[0].ptr<double>(0);
    double *fdomIm = fdomPlanes[1].ptr<double>(0);

    for (int i = 0; i < size; ++i) {
        double re = fdomRe[i] * spiralRe[i] - fdomIm[i] * spiralIm[i];
        double im = fdomRe[i] * spiralIm[i] + fdomIm[i] * spiralRe[i];
        fdomRe[i] = re;
        fdomIm[i] = im;
    }

    cv::merge(fdomPlanes, 2, fdomMat);
    cv::Mat d1Mat;
    cv::dft(fdomMat, d1Mat, DFT_INVERSE);
    d1Mat /= size;

    for (int i = 0; i < size; ++i) {
        double re = fdomRe[i] * spiralRe[i] - fdomIm[i] * spiralIm[i];
        double im = fdomRe[i] * spiralIm[i] + fdomIm[i] * spiralRe[i];
        fdomRe[i] = re;
        fdomIm[i] = im;
    }

    cv::Mat d2Mat;
    cv::merge(fdomPlanes, 2, fdomMat);
    cv::dft(fdomMat, d2Mat, DFT_INVERSE);
    d2Mat /= size;

    cv::Mat rPlanes[2] = {cv::Mat::zeros(Size(xsize, ysize), numType),
                          cv::Mat::zeros(Size(xsize, ysize), numType)};
    cv::Mat d1Planes[2];
    cv::Mat d2Planes[2];

    cv::split(d1Mat, d1Planes);
    cv::split(d2Mat, d2Planes);

    double *d1Re = d1Planes[0].ptr<double>(0);
    double *d1Im = d1Planes[1].ptr<double>(0);
    double *d2Re = d2Planes[0].ptr<double>(0);
    double *d2Im = d2Planes[1].ptr<double>(0);
    double *rRe = rPlanes[0].ptr<double>(0);
    double *rIm = rPlanes[1].ptr<double>(0);

    for (int i = 0; i < size; ++i) {
        rRe[i] = d1Re[i] * d1Re[i] - d1Im[i] * d1Im[i] - imRe[i] * d2Re[i];
        rIm[i] = d1Re[i] * d1Im[i] + d1Im[i] * d1Re[i] - imRe[i] * d2Im[i];
    }

    if (smooth > 0) {
        cv::Mat rMat;
        cv::merge(rPlanes, 2, rMat);

        cv::Mat tempMat;
        cv::Mat tempPlanes[2];
        cv::dft(rMat, tempMat);
        cv::split(tempMat, tempPlanes);

        double *tempRe = tempPlanes[0].ptr<double>(0);
        double *tempIm = tempPlanes[1].ptr<double>(0);

        for (int i = 0; i < size; ++i) {
            double a = exp(-(rho[i] * rho[i]) / (smooth * smooth));
            tempRe[i] *= a;
            tempIm[i] *= a;
        }

        cv::merge(tempPlanes, 2, tempMat);
        cv::dft(tempMat, rMat, DFT_INVERSE);
        rMat /= size;
        cv::split(rMat, rPlanes);
        rRe = rPlanes[0].ptr<double>(0);
        rIm = rPlanes[1].ptr<double>(0);
    }

    delete[] rho;

    double *orient = new double[size];
    double *qmap = new double[size];

    for (int i = 0; i < size; ++i)
        orient[i] = atan2(rIm[i], rRe[i]);

    for (int i = 0; i < size; ++i) {
        qmap[i] = sqrt(rRe[i] * rRe[i] + rIm[i] * rIm[i]);
        orient[i] /= (2. * M_PI);
    }

    double *dir = new double[size];
    double *path = new double[size];

    extern void qg_path_follower_vortex(cv::Size size, double *orient, double *qmap,
                                        double *dir, double *path);

    char *flags = new char[size];
    memset(flags, 0, size);
    for (int i = 0; i < size; ++i) {
        if (mask.data[i] == 0) flags[i] = 0x1;
    }

    int *todo = new int[size];
    int end = 0;
    int order = 0;

    auto todo_push = [&](int ndx) {
        int child;
        todo[end] = ndx;
        child = end++;
        while (child > 0) {
            int parent = (child - 1) / 2;
            if (qmap[todo[parent]] < qmap[todo[child]]) {
                std::swap(todo[parent], todo[child]);
                child = parent;
            } else break;
        }
    };

    auto todo_pop = [&]() -> int {
        int result = todo[0];
        --end;
        std::swap(todo[0], todo[end]);
        int root = 0;
        while (root * 2 + 1 < end) {
            int child = root * 2 + 1;
            if (child + 1 < end && qmap[todo[child]] < qmap[todo[child + 1]])
                ++child;
            if (qmap[todo[root]] < qmap[todo[child]]) {
                std::swap(todo[root], todo[child]);
                root = child;
            } else break;
        }
        return result;
    };

    auto WRAP = [](double x) -> double {
        return (x > 0.5) ? (x - 1.0) : ((x <= -0.5) ? (x + 1.0) : x);
    };

    auto unwrap_insert = [&](int ndx, double val) {
        dir[ndx] = val;
        flags[ndx] |= 0x2;
        path[ndx] = order++;
        todo_push(ndx);
    };

    while (1) {
        double m = -HUGE_VAL;
        int mndx = 0;
        for (int k = 0; k < size; ++k)
            if (qmap[k] > m && !flags[k])
                m = qmap[mndx = k];
        if (m == -HUGE_VAL) break;

        unwrap_insert(mndx, orient[mndx]);

        while (end) {
            int ndx = todo_pop();
            int x = ndx % xsize;
            int y = ndx / xsize;
            double val = dir[ndx];
            if (x > 0 && !flags[ndx - 1])
                unwrap_insert(ndx - 1, val + WRAP(orient[ndx - 1] - orient[ndx]));
            if (x < xsize - 1 && !flags[ndx + 1])
                unwrap_insert(ndx + 1, val + WRAP(orient[ndx + 1] - orient[ndx]));
            if (y > 0 && !flags[ndx - xsize])
                unwrap_insert(ndx - xsize, val + WRAP(orient[ndx - xsize] - orient[ndx]));
            if (y < ysize - 1 && !flags[ndx + xsize])
                unwrap_insert(ndx + xsize, val + WRAP(orient[ndx + xsize] - orient[ndx]));
        }
    }

    for (int i = 0; i < size; ++i)
        dir[i] = fmod(dir[i] * M_PI + M_PI, 2 * M_PI) - M_PI;

    imPlanes[1] = cv::Mat::zeros(Size(xsize, ysize), numType);
    double *imIm = imPlanes[1].ptr<double>(0);
    for (int i = 0; i < size; ++i)
        imIm[i] = d1Re[i] * cos(-dir[i]) - d1Im[i] * sin(-dir[i]);

    cv::Mat phase(Size(xsize, ysize), numType);
    double *p = phase.ptr<double>(0);
    for (int i = 0; i < size; ++i) {
        p[i] = atan2(imIm[i], imRe[i]);
    }

    cv::Mat result;
    phase.copyTo(result, mask);

    delete[] orient;
    delete[] qmap;
    delete[] dir;
    delete[] path;
    delete[] flags;
    delete[] todo;
    delete[] spiralRe;
    delete[] spiralIm;
    delete[] theta;

    return result;
}

PreparedImage prepareImage(const cv::Mat &input,
                           const CircleOutline &outside,
                           const CircleOutline &center,
                           int dftSize) {
    PreparedImage result;

    cv::Mat gray;
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = input.clone();
    }

    double cx = outside.center.x;
    double cy = outside.center.y;
    double rad = outside.radius;

    int left = std::max(0, (int)(cx - rad));
    int top = std::max(0, (int)(cy - rad));
    int width = std::min((int)(2 * rad), gray.cols - left);
    int height = std::min((int)(2 * rad), gray.rows - top);

    cv::Mat roi = gray(cv::Rect(left, top, width, height)).clone();

    double newCx = cx - left;
    double newCy = cy - top;
    double centerDx = cx - center.center.x;
    double centerDy = cy - center.center.y;

    result.scaleFactor = (double)dftSize / roi.cols;

    if (result.scaleFactor < 1.0) {
        cv::resize(roi, roi, cv::Size(dftSize, dftSize), 0, 0, cv::INTER_AREA);
        double roicx = (roi.cols - 1) / 2.0;
        double roicy = (roi.rows - 1) / 2.0;

        result.outside.center.x = roicx;
        result.outside.center.y = roicy;
        result.outside.radius = roicx;

        result.center.center.x = roicx - centerDx * result.scaleFactor;
        result.center.center.y = roicy - centerDy * result.scaleFactor;
        result.center.radius = center.radius * result.scaleFactor;
    } else {
        result.scaleFactor = 1.0;
        result.outside.center.x = newCx;
        result.outside.center.y = newCy;
        result.outside.radius = rad;

        result.center.center.x = newCx - centerDx;
        result.center.center.y = newCy - centerDy;
        result.center.radius = center.radius;
    }

    result.image = roi;
    result.mask = makeMask(result.outside, result.center, roi.cols, roi.rows);

    return result;
}
