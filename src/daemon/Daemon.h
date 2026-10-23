#pragma once
#include <dispatch/dispatch.h>

#include <string>

#include "daemon/AudioPipeline.h"
#include "daemon/ConfigWatcher.h"
#include "daemon/DeviceMonitor.h"
#include "daemon/StatusFile.h"
#include "engine/FilterEngine.h"

namespace eqapo {

struct DaemonOptions {
    unsigned bufferFrames = 0;  // 0 = device default
    bool trace = false;
    std::string configFile;     // empty = Paths::configFile()
    std::string version = "0.1.0";
};

// Owns the engine, the audio pipeline and all watchers. run() blocks until
// SIGTERM/SIGINT.
class Daemon {
public:
    explicit Daemon(const DaemonOptions& options);
    ~Daemon();
    int run();

private:
    void rebuild(const std::string& reason);  // control queue
    void reloadConfig();                      // control queue
    void writeStatus();                       // control queue
    void scheduleRetry();
    void shutdown();

    DaemonOptions options_;
    dispatch_queue_t queue_;
    FilterEngine engine_;
    AudioPipeline pipeline_;
    DeviceMonitor monitor_;
    ConfigWatcher watcher_;
    DaemonStatus status_;
    AudioObjectID currentDevice_ = 0;
    unsigned retryDelayMs_ = 1000;
    dispatch_source_t retryTimer_ = nullptr;
    dispatch_source_t statusTimer_ = nullptr;
    dispatch_source_t sigterm_ = nullptr;
    dispatch_source_t sigint_ = nullptr;
    dispatch_source_t sighup_ = nullptr;
    bool stopping_ = false;
};

}  // namespace eqapo
