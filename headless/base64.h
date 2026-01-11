#ifndef HEADLESS_BASE64_H
#define HEADLESS_BASE64_H

#include <string>
#include <vector>
#include <cstdint>

namespace base64 {

std::string encode(const uint8_t* data, size_t len);
std::string encode(const std::vector<uint8_t>& data);
std::string encode(const std::string& str);

std::vector<uint8_t> decode(const std::string& encoded);

}

#endif
