#include "engine/Log.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace eqapo {

namespace {
std::mutex& sinkMutex() {
    static std::mutex m;
    return m;
}
std::function<void(LogLevel, const std::string&)>& sinkRef() {
    static std::function<void(LogLevel, const std::string&)> sink;
    return sink;
}
bool g_trace = false;
}  // namespace

void setLogSink(std::function<void(LogLevel, const std::string&)> sink) {
    std::lock_guard<std::mutex> lock(sinkMutex());
    sinkRef() = std::move(sink);
}

void setTraceEnabled(bool enabled) { g_trace = enabled; }
bool traceEnabled() { return g_trace; }

void logAt(LogLevel level, const std::string& message) {
    if (level == LogLevel::Trace && !g_trace) return;
    std::function<void(LogLevel, const std::string&)> sink;
    {
        std::lock_guard<std::mutex> lock(sinkMutex());
        sink = sinkRef();
    }
    if (sink) {
        sink(level, message);
    } else {
        std::fprintf(stderr, "%s%s\n", level == LogLevel::Error ? "error: " : "", message.c_str());
    }
}

std::string format(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int n = std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n < 0) return "";
    if (static_cast<size_t>(n) < sizeof(buf)) return std::string(buf, static_cast<size_t>(n));
    std::string big(static_cast<size_t>(n) + 1, '\0');
    va_start(args, fmt);
    std::vsnprintf(big.data(), big.size(), fmt, args);
    va_end(args);
    big.resize(static_cast<size_t>(n));
    return big;
}

}  // namespace eqapo
