#include "sidecar.h"
#include "base64.h"
#include "vortex.h"
#include "punwrap.h"
#include "zernfit.h"
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int Sidecar::run() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    while (running) {
        Message msg;
        ReadResult result = readMessage(std::cin, msg);

        if (result == ReadResult::Eof) {
            break;
        }

        if (result == ReadResult::Error) {
            writeError(std::cout, "Malformed request");
            continue;
        }

        std::string cmd = msg.get("cmd");

        if (cmd == "config") {
            handleConfig(msg, std::cout);
        } else if (cmd == "analyze") {
            handleAnalyze(msg, std::cout);
        } else if (cmd == "preview") {
            handlePreview(msg, std::cout);
        } else if (cmd == "quit") {
            handleQuit(msg, std::cout);
        } else if (cmd.empty()) {
            writeError(std::cout, "Missing cmd field");
        } else {
            writeError(std::cout, "Unknown command: " + cmd);
        }
    }

    return 0;
}

void Sidecar::handleConfig(const Message& msg, std::ostream& out) {
    if (msg.has("diameter")) mirrorConfig.diameter = msg.getDouble("diameter");
    if (msg.has("roc")) mirrorConfig.roc = msg.getDouble("roc");
    if (msg.has("lambda")) mirrorConfig.lambda = msg.getDouble("lambda");
    if (msg.has("conic")) mirrorConfig.conic = msg.getDouble("conic");
    if (msg.has("obstruction")) mirrorConfig.obstruction = msg.getDouble("obstruction");
    if (msg.has("fringe_spacing")) mirrorConfig.fringeSpacing = msg.getDouble("fringe_spacing");
    if (msg.has("flip_v")) mirrorConfig.flipV = msg.getBool("flip_v");
    if (msg.has("flip_h")) mirrorConfig.flipH = msg.getBool("flip_h");
    if (msg.has("do_null")) mirrorConfig.doNull = msg.getBool("do_null");
    if (msg.has("auto_invert")) autoInvert = msg.getBool("auto_invert");
    if (msg.has("dft_size")) processConfig.dftSize = msg.getInt("dft_size");
    if (msg.has("center_filter")) processConfig.centerFilter = msg.getDouble("center_filter");
    if (msg.has("smooth")) processConfig.smoothFactor = msg.getDouble("smooth");
    if (msg.has("zernike_terms")) processConfig.zernikeTerms = msg.getInt("zernike_terms");

    writeField(out, "status", "ok");
    writeTerminator(out);
}

void Sidecar::handleAnalyze(const Message& msg, std::ostream& out) {
    cv::Mat input;
    if (!decodeImage(msg.get("image"), input)) {
        writeError(out, "Cannot decode image");
        return;
    }

    CircleOutline outside, center;
    if (!parseOutline(msg, outside, center)) {
        writeError(out, "Invalid outline data");
        return;
    }

    MirrorConfig mirror = mirrorConfig;
    if (mirror.diameter == 0) {
        mirror.diameter = outside.radius * 2;
    }

    PreparedImage prep = prepareImage(input, outside, center, processConfig.dftSize);

    cv::Mat phase = extractPhase(prep.image, prep.mask,
                                 processConfig.centerFilter, processConfig.smoothFactor);

    cv::Mat result = cv::Mat::zeros(phase.size(), CV_64F);
    phase.copyTo(result, prep.mask);
    cv::normalize(result, result, 0, 1., cv::NORM_MINMAX, CV_64F, prep.mask);

    cv::Mat unwrapped = cv::Mat::zeros(result.size(), CV_64F);
    cv::Mat mask8u = (255 - prep.mask) / 255;

    unwrap(result.ptr<double>(0), unwrapped.ptr<double>(0),
           reinterpret_cast<char*>(mask8u.data), result.cols, result.rows);

    if (!mirror.flipV) {
        cv::flip(unwrapped, unwrapped, 0);
        prep.outside.center.y = (unwrapped.rows - 1) - prep.outside.center.y;
        prep.center.center.y = (unwrapped.rows - 1) - prep.center.center.y;
    }

    if (mirror.flipH) {
        cv::flip(unwrapped, unwrapped, 1);
        prep.outside.center.x = (unwrapped.cols - 1) - prep.outside.center.x;
        prep.center.center.x = (unwrapped.cols - 1) - prep.center.center.x;
    }

    if (mirror.fringeSpacing != 1.0) {
        unwrapped *= mirror.fringeSpacing;
    }

    cv::Mat finalMask = makeMask(prep.outside, prep.center, unwrapped.cols, unwrapped.rows);

    std::vector<double> zernikes = fitZernikes(unwrapped, finalMask, prep.outside,
                                               processConfig.zernikeTerms, false);

    bool wasInverted = false;
    bool inversionDetected = false;

    if (mirror.conic != 0.0 && zernikes.size() > 8) {
        double z8 = zernikes[8];
        if (mirror.conic * z8 < 0.0) {
            inversionDetected = true;
        }
    }

    if (autoInvert && inversionDetected) {
        unwrapped *= -1.0;
        zernikes = fitZernikes(unwrapped, finalMask, prep.outside,
                               processConfig.zernikeTerms, false);
        wasInverted = true;
    }

    double nullValue = 0.0;
    if (mirror.doNull && mirror.conic != 0.0) {
        nullValue = mirror.nullValue();
    }

    std::vector<double> finalZernikes = zernikes;
    if (nullValue != 0.0 && finalZernikes.size() > 8) {
        finalZernikes[8] -= nullValue;
    }

    std::vector<bool> enables = getDefaultEnables(processConfig.zernikeTerms);

    cv::Mat wavefrontSurface = reconstructFromZernikes(finalMask, prep.outside,
                                                        finalZernikes, enables);

    SurfaceMetrics metrics = computeMetrics(wavefrontSurface, finalMask, mirror.lambda);

    cv::Mat maskedUnwrapped = cv::Mat::zeros(unwrapped.size(), CV_64F);
    unwrapped.copyTo(maskedUnwrapped, finalMask);

    Wavefront wf;
    wf.data = maskedUnwrapped;
    wf.mask = finalMask;
    wf.zernikes = zernikes;
    wf.outside = prep.outside;
    wf.obstruction = prep.center;
    wf.mirror = mirror;
    wf.rms = metrics.rms;
    wf.pv = metrics.pv;
    wf.strehl = metrics.strehl;

    writeField(out, "status", "ok");
    writeField(out, "rms", metrics.rms);
    writeField(out, "pv", metrics.pv);
    writeField(out, "strehl", metrics.strehl);
    writeField(out, "inverted", wasInverted);
    writeField(out, "null_applied", mirror.doNull && mirror.conic != 0.0);
    writeField(out, "null_value", nullValue);

    for (size_t i = 0; i < finalZernikes.size(); ++i) {
        writeField(out, "z" + std::to_string(i), finalZernikes[i]);
    }

    writeField(out, "wft", encodeWft(wf));
    writeTerminator(out);
}

void Sidecar::handlePreview(const Message& msg, std::ostream& out) {
    cv::Mat input;
    if (!decodeImage(msg.get("image"), input)) {
        writeError(out, "Cannot decode image");
        return;
    }

    CircleOutline outside, center;
    if (!parseOutline(msg, outside, center)) {
        writeError(out, "Invalid outline data");
        return;
    }

    PreparedImage prep = prepareImage(input, outside, center, processConfig.dftSize);
    cv::Mat dftMag = computeDftMagnitude(prep.image, prep.mask);

    std::vector<uint8_t> pngBuf;
    cv::imencode(".png", dftMag, pngBuf);

    writeField(out, "status", "ok");
    writeField(out, "dft", base64::encode(pngBuf));
    writeTerminator(out);
}

void Sidecar::handleQuit(const Message& msg, std::ostream& out) {
    (void)msg;
    writeField(out, "status", "ok");
    writeTerminator(out);
    running = false;
}

bool Sidecar::decodeImage(const std::string& b64, cv::Mat& image) {
    if (b64.empty()) return false;

    std::vector<uint8_t> data = base64::decode(b64);
    if (data.empty()) return false;

    image = cv::imdecode(data, cv::IMREAD_COLOR);
    return !image.empty();
}

bool Sidecar::parseOutline(const Message& msg, CircleOutline& outside, CircleOutline& center) {
    if (msg.has("outline")) {
        std::vector<uint8_t> data = base64::decode(msg.get("outline"));
        if (data.empty()) return false;
        return parseOutlineFromBinary(data, outside, center);
    }

    if (msg.has("outside_cx") && msg.has("outside_cy") && msg.has("outside_r")) {
        outside.center.x = msg.getDouble("outside_cx");
        outside.center.y = msg.getDouble("outside_cy");
        outside.radius = msg.getDouble("outside_r");

        if (msg.has("center_cx") && msg.has("center_cy") && msg.has("center_r")) {
            center.center.x = msg.getDouble("center_cx");
            center.center.y = msg.getDouble("center_cy");
            center.radius = msg.getDouble("center_r");
        } else {
            center.radius = 0;
        }

        return outside.radius > 0;
    }

    return false;
}

bool Sidecar::parseOutlineFromBinary(const std::vector<uint8_t>& data, CircleOutline& outside, CircleOutline& center) {
    if (data.size() < 36) return false;

    const double* dp = reinterpret_cast<const double*>(data.data());
    outside.center.x = dp[0];
    outside.center.y = dp[1];
    outside.radius = dp[2];

    int pointCount = *reinterpret_cast<const int*>(data.data() + 24);
    if (pointCount < 0 || pointCount > 100) return false;

    size_t offset = 36 + pointCount * 16;

    if (data.size() >= offset + 36) {
        offset += 36;
        const int* filterCount = reinterpret_cast<const int*>(data.data() + offset - 12);
        if (*filterCount >= 0 && *filterCount <= 100) {
            offset += *filterCount * 16;
        }
    }

    if (data.size() >= offset + 36) {
        const double* cp = reinterpret_cast<const double*>(data.data() + offset);
        center.center.x = cp[0];
        center.center.y = cp[1];
        center.radius = cp[2];
    } else {
        center.radius = 0;
    }

    return outside.radius > 0;
}

std::string Sidecar::encodeWft(const Wavefront& wf) {
    std::ostringstream ss;

    ss << wf.data.cols << "\n" << wf.data.rows << "\n";
    for (int y = wf.data.rows - 1; y >= 0; --y) {
        for (int x = 0; x < wf.data.cols; ++x) {
            ss << wf.data.at<double>(y, x) << "\n";
        }
    }

    ss << "outside ellipse " << wf.outside.center.x << " " << wf.outside.center.y
       << " " << wf.outside.radius << " " << wf.outside.radius << "\n";
    if (wf.obstruction.radius > 0) {
        ss << "obstruction ellipse " << wf.obstruction.center.x << " " << wf.obstruction.center.y
           << " " << wf.obstruction.radius << " " << wf.obstruction.radius << "\n";
    }
    ss << "DIAM " << wf.mirror.diameter << "\n";
    ss << "ROC " << wf.mirror.roc << "\n";
    ss << "Lambda " << wf.mirror.lambda << "\n";

    return base64::encode(ss.str());
}

void Sidecar::writeError(std::ostream& out, const std::string& message) {
    writeField(out, "status", "error");
    writeField(out, "message", message);
    writeTerminator(out);
}
