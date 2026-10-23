#include "daemon/Daemon.h"

#import <Foundation/Foundation.h>

#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <ctime>

#include "daemon/Paths.h"
#include "engine/Log.h"
#include "engine/StringHelper.h"

namespace eqapo {

namespace {
const char* kPermissionHint =
    "\n  If macOS denied the permission: System Settings > Privacy & Security > Screen & System Audio Recording"
    " > enable eqapo (System Audio Recording Only), then run `eqapo restart`. `eqapo permission` opens that pane.";

void stderrLog(LogLevel level, const std::string& message) {
    std::time_t t = std::time(nullptr);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    const char* tag = level == LogLevel::Error ? "ERROR" : level == LogLevel::Trace ? "trace" : "info ";
    std::fprintf(stderr, "%s %s %s\n", stamp, tag, message.c_str());
    std::fflush(stderr);
}
}  // namespace

Daemon::Daemon(const DaemonOptions& options)
    : options_(options),
      queue_(dispatch_queue_create("com.eqapo.daemon.control", DISPATCH_QUEUE_SERIAL)),
      monitor_(queue_),
      watcher_(queue_) {
    if (options_.configFile.empty()) options_.configFile = Paths::configFile();
    setLogSink(stderrLog);
    setTraceEnabled(options_.trace);
}

Daemon::~Daemon() = default;

int Daemon::run() {
    status_.pid = getpid();
    status_.version = options_.version;
    status_.state = "starting";
    status_.startedAt = nowIso8601();
    status_.configPath = options_.configFile;
    Paths::ensureDirectory(Paths::root());
    Paths::writeFile(Paths::pidFile(), std::to_string(getpid()) + "\n");
    infoMessage(format("eqapo %s starting (pid %d), config %s", options_.version.c_str(), getpid(),
                       options_.configFile.c_str()));

    // signals via dispatch sources (safe, no async-signal-unsafe work in handlers)
    signal(SIGTERM, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    // Signal sources live on the main queue so they still fire while the control queue is
    // blocked (e.g. inside Core Audio waiting for the permission dialog). The orderly shutdown
    // runs on the control queue; if it cannot run within 2 s the process exits hard, which makes
    // coreaudiod drop our tap and aggregate device and restores normal audio.
    dispatch_queue_t mainQueue = dispatch_get_main_queue();
    sigterm_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, mainQueue);
    sigint_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, mainQueue);
    sighup_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGHUP, 0, queue_);
    void (^requestStop)(void) = ^{
      dispatch_async(queue_, ^{ shutdown(); });
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC), mainQueue, ^{
        std::fprintf(stderr, "shutdown did not complete in time, exiting\n");
        unlink(Paths::pidFile().c_str());
        _exit(0);
      });
    };
    dispatch_source_set_event_handler(sigterm_, requestStop);
    dispatch_source_set_event_handler(sigint_, requestStop);
    dispatch_source_set_event_handler(sighup_, ^{
      infoMessage("SIGHUP: reloading configuration");
      reloadConfig();
    });
    dispatch_resume(sigterm_);
    dispatch_resume(sigint_);
    dispatch_resume(sighup_);

    dispatch_async(queue_, ^{
      monitor_.start([this](const std::string& reason) {
          infoMessage("device change: " + reason);
          rebuild(reason);
      });
      std::string configDir = options_.configFile.substr(0, options_.configFile.find_last_of('/'));
      if (!watcher_.start(configDir, [this] { reloadConfig(); }))
          logMessage("could not watch " + configDir + " for changes; use `eqapo reload`");
      rebuild("startup");
    });

    // periodic status/statistics refresh
    statusTimer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue_);
    dispatch_source_set_timer(statusTimer_, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), 5 * NSEC_PER_SEC,
                              NSEC_PER_SEC);
    dispatch_source_set_event_handler(statusTimer_, ^{
      engine_.collectGarbage();
      writeStatus();
    });
    dispatch_resume(statusTimer_);

    dispatch_main();
    return 0;
}

void Daemon::rebuild(const std::string& reason) {
    if (stopping_) return;
    if (retryTimer_) {
        dispatch_source_cancel(retryTimer_);
        retryTimer_ = nullptr;
    }
    pipeline_.stop();
    engine_.setAudioRunning(false);

    AudioObjectID device = ca::defaultOutputDevice();
    if (device == kAudioObjectUnknown || device == 0) {
        status_.state = "error";
        status_.error = "no default output device";
        logMessage(status_.error);
        writeStatus();
        scheduleRetry();
        return;
    }
    currentDevice_ = device;
    monitor_.watchDevice(device);
    status_.deviceName = ca::deviceName(device);
    status_.deviceUid = ca::deviceUID(device);
    status_.transport = ca::transportTypeName(ca::transportType(device));
    engine_.setDeviceInfo(status_.deviceName, status_.deviceUid);

    AudioPipeline::Format fmt;
    std::string error;
    if (!pipeline_.prepare(device, options_.bufferFrames, fmt, error)) {
        status_.state = "error";
        status_.error = error;
        logMessage("pipeline setup failed on \"" + status_.deviceName + "\": " + error);
        writeStatus();
        scheduleRetry();
        return;
    }
    status_.sampleRate = fmt.sampleRate;
    status_.inputChannels = fmt.inputChannels;
    status_.outputChannels = fmt.outputChannels;
    status_.bufferFrames = fmt.bufferFrames;
    status_.inputFormat = fmt.inputFormat;
    status_.outputFormat = fmt.outputFormat;
    status_.latencyMs = fmt.bufferFrames / fmt.sampleRate * 1000.0;

    engine_.initialize(static_cast<float>(fmt.sampleRate), fmt.inputChannels, fmt.outputChannels, fmt.bufferFrames);
    engine_.loadConfig(options_.configFile);
    status_.filters = engine_.describeLoaded();
    status_.configErrors = engine_.lastLoadErrors();
    status_.lastLoadMs = engine_.lastLoadMilliseconds();

    infoMessage("starting audio I/O (on the first run macOS asks for System Audio Recording permission; "
                "eqapo waits until you answer)");
    if (!pipeline_.startIO(engine_, error)) {
        status_.state = "error";
        status_.error = error;
        logMessage("could not start audio: " + error + kPermissionHint);
        writeStatus();
        scheduleRetry();
        return;
    }
    engine_.setAudioRunning(true);
    retryDelayMs_ = 1000;
    status_.state = "running";
    status_.error.clear();
    infoMessage(format("processing \"%s\" (%s): %g Hz, %u in / %u out channels, %u frames (%.1f ms), %zu filters [%s]",
                       status_.deviceName.c_str(), status_.transport.c_str(), fmt.sampleRate, fmt.inputChannels,
                       fmt.outputChannels, fmt.bufferFrames, status_.latencyMs, status_.filters.size(), reason.c_str()));
    writeStatus();
}

void Daemon::scheduleRetry() {
    if (stopping_) return;
    unsigned delay = retryDelayMs_;
    retryDelayMs_ = std::min(retryDelayMs_ * 2, 30000u);
    infoMessage(format("retrying in %u ms", delay));
    retryTimer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue_);
    dispatch_source_set_timer(retryTimer_, dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(delay) * NSEC_PER_MSEC),
                              DISPATCH_TIME_FOREVER, 50 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(retryTimer_, ^{
      if (retryTimer_) {
          dispatch_source_cancel(retryTimer_);
          retryTimer_ = nullptr;
      }
      rebuild("retry");
    });
    dispatch_resume(retryTimer_);
}

void Daemon::reloadConfig() {
    if (stopping_ || status_.state != "running") {
        if (!stopping_ && status_.state != "running") rebuild("config changed while not running");
        return;
    }
    infoMessage("configuration changed, reloading");
    engine_.loadConfig(options_.configFile);
    status_.filters = engine_.describeLoaded();
    status_.configErrors = engine_.lastLoadErrors();
    status_.lastLoadMs = engine_.lastLoadMilliseconds();
    infoMessage(format("loaded %zu filters in %.2f ms%s", status_.filters.size(), status_.lastLoadMs,
                       status_.configErrors.empty() ? "" : " (with errors)"));
    writeStatus();
    // free the retired configuration once the crossfade is over
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 200 * NSEC_PER_MSEC), queue_, ^{ engine_.collectGarbage(); });
}

void Daemon::writeStatus() {
    AudioPipeline::Stats s = pipeline_.stats();
    status_.callbacks = s.callbacks;
    status_.inputPeak = s.inputPeak;
    status_.outputPeak = s.outputPeak;
    status_.overloads = s.overloads;
    status_.frameMismatches = s.frameMismatches;
    status_.lastMicros = s.lastMicros;
    status_.maxMicros = s.maxMicros;
    status_.avgLoadPercent = s.avgLoadPercent;
    status_.updatedAt = nowIso8601();
    Paths::writeFileAtomic(Paths::statusFile(), statusToJson(status_));
}

void Daemon::shutdown() {
    if (stopping_) return;
    stopping_ = true;
    infoMessage("stopping: restoring direct audio output");
    watcher_.stop();
    monitor_.stop();
    pipeline_.stop();
    engine_.setAudioRunning(false);
    status_.state = "stopped";
    writeStatus();
    unlink(Paths::pidFile().c_str());
    exit(0);
}

}  // namespace eqapo
