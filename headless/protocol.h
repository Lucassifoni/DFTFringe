#ifndef HEADLESS_PROTOCOL_H
#define HEADLESS_PROTOCOL_H

#include <string>
#include <map>
#include <iostream>

struct Message {
    std::map<std::string, std::string> fields;

    std::string get(const std::string& key, const std::string& defaultVal = "") const;
    double getDouble(const std::string& key, double defaultVal = 0.0) const;
    int getInt(const std::string& key, int defaultVal = 0) const;
    bool getBool(const std::string& key, bool defaultVal = false) const;
    bool has(const std::string& key) const;
};

enum class ReadResult {
    Ok,
    Eof,
    Error
};

ReadResult readMessage(std::istream& in, Message& msg);

void writeField(std::ostream& out, const std::string& key, const std::string& value);
void writeField(std::ostream& out, const std::string& key, const char* value);
void writeField(std::ostream& out, const std::string& key, double value);
void writeField(std::ostream& out, const std::string& key, int value);
void writeField(std::ostream& out, const std::string& key, bool value);
void writeTerminator(std::ostream& out);

#endif
