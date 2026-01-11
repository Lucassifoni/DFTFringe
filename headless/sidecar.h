#ifndef HEADLESS_SIDECAR_H
#define HEADLESS_SIDECAR_H

#include <iostream>
#include "types.h"
#include "protocol.h"

class Sidecar {
public:
    int run();

private:
    MirrorConfig mirrorConfig;
    ProcessConfig processConfig;
    bool autoInvert = true;
    bool running = true;

    void handleConfig(const Message& msg, std::ostream& out);
    void handleAnalyze(const Message& msg, std::ostream& out);
    void handlePreview(const Message& msg, std::ostream& out);
    void handleQuit(const Message& msg, std::ostream& out);

    bool decodeImage(const std::string& b64, cv::Mat& image);
    bool parseOutline(const Message& msg, CircleOutline& outside, CircleOutline& center);
    bool parseOutlineFromBinary(const std::vector<uint8_t>& data, CircleOutline& outside, CircleOutline& center);
    std::string encodeWft(const Wavefront& wf);

    void writeError(std::ostream& out, const std::string& message);
};

#endif
