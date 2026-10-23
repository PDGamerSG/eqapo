#include "daemon/StatusFile.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace eqapo {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string nowIso8601() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", std::localtime(&t));
    return buf;
}

namespace {
std::string arr(const std::vector<std::string>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); i++) {
        if (i) s += ", ";
        s += "\"" + jsonEscape(v[i]) + "\"";
    }
    return s + "]";
}
std::string num(double d) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g", d);
    return buf;
}
}  // namespace

std::string statusToJson(const DaemonStatus& s) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"pid\": " << s.pid << ",\n";
    o << "  \"version\": \"" << jsonEscape(s.version) << "\",\n";
    o << "  \"state\": \"" << jsonEscape(s.state) << "\",\n";
    o << "  \"error\": \"" << jsonEscape(s.error) << "\",\n";
    o << "  \"device\": {\"name\": \"" << jsonEscape(s.deviceName) << "\", \"uid\": \"" << jsonEscape(s.deviceUid)
      << "\", \"transport\": \"" << jsonEscape(s.transport) << "\"},\n";
    o << "  \"sampleRate\": " << num(s.sampleRate) << ",\n";
    o << "  \"inputChannels\": " << s.inputChannels << ",\n";
    o << "  \"outputChannels\": " << s.outputChannels << ",\n";
    o << "  \"bufferFrames\": " << s.bufferFrames << ",\n";
    o << "  \"inputFormat\": \"" << jsonEscape(s.inputFormat) << "\",\n";
    o << "  \"outputFormat\": \"" << jsonEscape(s.outputFormat) << "\",\n";
    o << "  \"latencyMs\": " << num(s.latencyMs) << ",\n";
    o << "  \"configPath\": \"" << jsonEscape(s.configPath) << "\",\n";
    o << "  \"filters\": " << arr(s.filters) << ",\n";
    o << "  \"configErrors\": " << arr(s.configErrors) << ",\n";
    o << "  \"lastLoadMs\": " << num(s.lastLoadMs) << ",\n";
    o << "  \"callbacks\": " << s.callbacks << ",\n";
    o << "  \"overloads\": " << s.overloads << ",\n";
    o << "  \"frameMismatches\": " << s.frameMismatches << ",\n";
    o << "  \"lastMicros\": " << num(s.lastMicros) << ",\n";
    o << "  \"maxMicros\": " << num(s.maxMicros) << ",\n";
    o << "  \"avgLoadPercent\": " << num(s.avgLoadPercent) << ",\n";
    o << "  \"inputPeak\": " << num(s.inputPeak) << ",\n";
    o << "  \"outputPeak\": " << num(s.outputPeak) << ",\n";
    o << "  \"startedAt\": \"" << jsonEscape(s.startedAt) << "\",\n";
    o << "  \"updatedAt\": \"" << jsonEscape(s.updatedAt) << "\"\n";
    o << "}\n";
    return o.str();
}

namespace {
// Tiny forgiving JSON scanner: finds "key": and returns the raw value token.
std::string rawValue(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n')) pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        std::string out;
        pos++;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                char n = json[pos + 1];
                out += n == 'n' ? '\n' : n == 't' ? '\t' : n;
                pos += 2;
            } else {
                out += json[pos++];
            }
        }
        return out;
    }
    if (json[pos] == '[') {
        size_t end = json.find(']', pos);
        return json.substr(pos, end == std::string::npos ? std::string::npos : end - pos + 1);
    }
    if (json[pos] == '{') {
        size_t end = json.find('}', pos);
        return json.substr(pos, end == std::string::npos ? std::string::npos : end - pos + 1);
    }
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '\n' && json[end] != '}') end++;
    return json.substr(pos, end - pos);
}
std::vector<std::string> parseArray(const std::string& raw) {
    std::vector<std::string> out;
    size_t pos = 0;
    while ((pos = raw.find('"', pos)) != std::string::npos) {
        size_t end = pos + 1;
        std::string s;
        while (end < raw.size() && raw[end] != '"') {
            if (raw[end] == '\\' && end + 1 < raw.size()) {
                s += raw[end + 1];
                end += 2;
            } else {
                s += raw[end++];
            }
        }
        out.push_back(s);
        pos = end + 1;
    }
    return out;
}
}  // namespace

bool statusFromJson(const std::string& json, DaemonStatus& s) {
    if (json.find("\"pid\"") == std::string::npos) return false;
    s.pid = std::atoi(rawValue(json, "pid").c_str());
    s.version = rawValue(json, "version");
    s.state = rawValue(json, "state");
    s.error = rawValue(json, "error");
    std::string device = rawValue(json, "device");
    s.deviceName = rawValue(device, "name");
    s.deviceUid = rawValue(device, "uid");
    s.transport = rawValue(device, "transport");
    s.sampleRate = std::atof(rawValue(json, "sampleRate").c_str());
    s.inputChannels = static_cast<unsigned>(std::atoi(rawValue(json, "inputChannels").c_str()));
    s.outputChannels = static_cast<unsigned>(std::atoi(rawValue(json, "outputChannels").c_str()));
    s.bufferFrames = static_cast<unsigned>(std::atoi(rawValue(json, "bufferFrames").c_str()));
    s.inputFormat = rawValue(json, "inputFormat");
    s.outputFormat = rawValue(json, "outputFormat");
    s.latencyMs = std::atof(rawValue(json, "latencyMs").c_str());
    s.configPath = rawValue(json, "configPath");
    s.filters = parseArray(rawValue(json, "filters"));
    s.configErrors = parseArray(rawValue(json, "configErrors"));
    s.lastLoadMs = std::atof(rawValue(json, "lastLoadMs").c_str());
    s.callbacks = std::strtoull(rawValue(json, "callbacks").c_str(), nullptr, 10);
    s.overloads = std::strtoull(rawValue(json, "overloads").c_str(), nullptr, 10);
    s.frameMismatches = std::strtoull(rawValue(json, "frameMismatches").c_str(), nullptr, 10);
    s.lastMicros = std::atof(rawValue(json, "lastMicros").c_str());
    s.maxMicros = std::atof(rawValue(json, "maxMicros").c_str());
    s.avgLoadPercent = std::atof(rawValue(json, "avgLoadPercent").c_str());
    s.inputPeak = std::atof(rawValue(json, "inputPeak").c_str());
    s.outputPeak = std::atof(rawValue(json, "outputPeak").c_str());
    s.startedAt = rawValue(json, "startedAt");
    s.updatedAt = rawValue(json, "updatedAt");
    return true;
}

}  // namespace eqapo
