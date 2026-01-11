#include "protocol.h"
#include <sstream>
#include <iomanip>

std::string Message::get(const std::string& key, const std::string& defaultVal) const {
    auto it = fields.find(key);
    if (it == fields.end()) return defaultVal;
    return it->second;
}

double Message::getDouble(const std::string& key, double defaultVal) const {
    auto it = fields.find(key);
    if (it == fields.end()) return defaultVal;
    try {
        return std::stod(it->second);
    } catch (...) {
        return defaultVal;
    }
}

int Message::getInt(const std::string& key, int defaultVal) const {
    auto it = fields.find(key);
    if (it == fields.end()) return defaultVal;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return defaultVal;
    }
}

bool Message::getBool(const std::string& key, bool defaultVal) const {
    auto it = fields.find(key);
    if (it == fields.end()) return defaultVal;
    const std::string& val = it->second;
    if (val == "true" || val == "1" || val == "yes") return true;
    if (val == "false" || val == "0" || val == "no") return false;
    return defaultVal;
}

bool Message::has(const std::string& key) const {
    return fields.find(key) != fields.end();
}

ReadResult readMessage(std::istream& in, Message& msg) {
    msg.fields.clear();
    std::string line;
    bool gotAnyLine = false;

    while (std::getline(in, line)) {
        gotAnyLine = true;

        if (line == "---") {
            return ReadResult::Ok;
        }

        size_t tab = line.find('\t');
        if (tab != std::string::npos) {
            std::string key = line.substr(0, tab);
            std::string value = line.substr(tab + 1);
            msg.fields[key] = value;
        }
    }

    if (!gotAnyLine) {
        return ReadResult::Eof;
    }

    return msg.fields.empty() ? ReadResult::Eof : ReadResult::Error;
}

void writeField(std::ostream& out, const std::string& key, const std::string& value) {
    out << key << '\t' << value << '\n';
}

void writeField(std::ostream& out, const std::string& key, const char* value) {
    out << key << '\t' << value << '\n';
}

void writeField(std::ostream& out, const std::string& key, double value) {
    out << key << '\t' << std::setprecision(10) << value << '\n';
}

void writeField(std::ostream& out, const std::string& key, int value) {
    out << key << '\t' << value << '\n';
}

void writeField(std::ostream& out, const std::string& key, bool value) {
    out << key << '\t' << (value ? "true" : "false") << '\n';
}

void writeTerminator(std::ostream& out) {
    out << "---" << std::endl;
}
