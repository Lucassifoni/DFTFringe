#include "base64.h"

namespace base64 {

static const char encodeTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const uint8_t decodeTable[256] = {
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,65,64,64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
    64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
};

std::string encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < len) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        result += encodeTable[(n >> 18) & 0x3F];
        result += encodeTable[(n >> 12) & 0x3F];
        result += encodeTable[(n >> 6) & 0x3F];
        result += encodeTable[n & 0x3F];
        i += 3;
    }

    if (i + 1 == len) {
        uint32_t n = data[i] << 16;
        result += encodeTable[(n >> 18) & 0x3F];
        result += encodeTable[(n >> 12) & 0x3F];
        result += '=';
        result += '=';
    } else if (i + 2 == len) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        result += encodeTable[(n >> 18) & 0x3F];
        result += encodeTable[(n >> 12) & 0x3F];
        result += encodeTable[(n >> 6) & 0x3F];
        result += '=';
    }

    return result;
}

std::string encode(const std::vector<uint8_t>& data) {
    return encode(data.data(), data.size());
}

std::string encode(const std::string& str) {
    return encode(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

std::vector<uint8_t> decode(const std::string& encoded) {
    std::vector<uint8_t> result;
    if (encoded.empty()) return result;

    size_t padding = 0;
    if (encoded.size() >= 1 && encoded[encoded.size() - 1] == '=') padding++;
    if (encoded.size() >= 2 && encoded[encoded.size() - 2] == '=') padding++;

    result.reserve((encoded.size() / 4) * 3 - padding);

    uint32_t buf = 0;
    int bits = 0;

    for (char c : encoded) {
        uint8_t val = decodeTable[static_cast<uint8_t>(c)];
        if (val == 64) continue;
        if (val == 65) break;

        buf = (buf << 6) | val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            result.push_back((buf >> bits) & 0xFF);
        }
    }

    return result;
}

}
