#pragma once
#include <string>
#include <vector>

namespace eqapo {

struct DaemonStatus {
    int pid = 0;
    std::string version;
    std::string state;  // "running" | "error" | "starting" | "stopped"
    std::string error;
    std::string deviceName, deviceUid, transport;
    double sampleRate = 0;
    unsigned inputChannels = 0, outputChannels = 0, bufferFrames = 0;
    std::string inputFormat, outputFormat;
    std::string configPath;
    std::vector<std::string> filters;
    std::vector<std::string> configErrors;
    double lastLoadMs = 0;
    unsigned long long callbacks = 0, overloads = 0, frameMismatches = 0;
    double lastMicros = 0, maxMicros = 0, avgLoadPercent = 0;
    double latencyMs = 0;
    double inputPeak = 0, outputPeak = 0;  // peak |sample| seen since start
    std::string startedAt, updatedAt;
};

std::string statusToJson(const DaemonStatus& status);
// Minimal reader for the fields the CLI prints (tolerant of unknown keys).
bool statusFromJson(const std::string& json, DaemonStatus& status);
std::string nowIso8601();
std::string jsonEscape(const std::string& s);

}  // namespace eqapo
