#pragma once
#include <functional>
#include <string>

namespace eqapo {

enum class LogLevel { Trace, Info, Error };

// Process-wide log sink. Never called from the audio thread.
void setLogSink(std::function<void(LogLevel, const std::string&)> sink);
void setTraceEnabled(bool enabled);
bool traceEnabled();
void logAt(LogLevel level, const std::string& message);

// Errors/warnings worth showing the user (Equalizer APO's LogF).
inline void logMessage(const std::string& message) { logAt(LogLevel::Error, message); }
// Informational (Equalizer APO's TraceF).
inline void traceMessage(const std::string& message) { logAt(LogLevel::Trace, message); }
inline void infoMessage(const std::string& message) { logAt(LogLevel::Info, message); }

// printf-style helpers
std::string format(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace eqapo
