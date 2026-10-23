// Daemon-related sub-commands: run, devices, install/uninstall, start/stop/restart, status, reload.
#import <Foundation/Foundation.h>

#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "cli/Commands.h"
#include "daemon/CoreAudioUtil.h"
#include "daemon/Daemon.h"
#include "daemon/Paths.h"
#include "daemon/StatusFile.h"
#include "engine/StringHelper.h"

#ifndef EQAPO_VERSION
#define EQAPO_VERSION "0.1.0"
#endif

namespace eqapo::cli {

void installImpulseLoader();

int cmdRun(const Args& args) {
    DaemonOptions opts;
    opts.bufferFrames = static_cast<unsigned>(std::strtoul(optionValue(args, "--buffer", "0").c_str(), nullptr, 10));
    opts.trace = hasFlag(args, "--trace") || hasFlag(args, "-v");
    opts.configFile = optionValue(args, "--config", "");
    opts.version = EQAPO_VERSION;
    installImpulseLoader();
    Daemon daemon(opts);
    return daemon.run();
}

int cmdDevices(const Args&) {
    AudioObjectID def = ca::defaultOutputDevice();
    std::printf("%-3s %-36s %-12s %8s %4s  %s\n", "", "name", "transport", "rate", "out", "uid");
    for (AudioObjectID id : ca::allDevices()) {
        unsigned out = ca::channelCount(id, kAudioObjectPropertyScopeOutput);
        if (out == 0) continue;
        std::string name = ca::deviceName(id);
        if (name.rfind("eqapo", 0) == 0) continue;  // our private aggregate
        std::printf("%-3s %-36.36s %-12s %8.0f %4u  %s\n", id == def ? "*" : "", name.c_str(),
                    ca::transportTypeName(ca::transportType(id)).c_str(), ca::nominalSampleRate(id), out,
                    ca::deviceUID(id).c_str());
    }
    std::printf("\n* = default output device (the device eqapo processes)\n");
    return 0;
}

namespace {
std::string shell(const std::string& cmd, int* exitCode = nullptr) {
    std::string out;
    FILE* p = popen((cmd + " 2>&1").c_str(), "r");
    if (!p) return out;
    char buf[512];
    while (fgets(buf, sizeof(buf), p)) out += buf;
    int rc = pclose(p);
    if (exitCode) *exitCode = WEXITSTATUS(rc);
    return out;
}
std::string serviceTarget() { return "gui/" + std::to_string(getuid()) + "/" + Paths::launchAgentLabel; }
std::string domainTarget() { return "gui/" + std::to_string(getuid()); }
bool agentLoaded() {
    int rc = 1;
    shell("launchctl print " + serviceTarget(), &rc);
    return rc == 0;
}
std::string selfPath() {
    std::string dir = Paths::executableDir();
    return dir + "/eqapo";
}
}  // namespace

int cmdInstall(const Args& args) {
    bool uninstall = !args.empty() && args[0] == "uninstall";
    std::string plist = Paths::launchAgentPlist();
    if (uninstall) {
        if (agentLoaded()) shell("launchctl bootout " + serviceTarget());
        unlink(plist.c_str());
        std::printf("removed LaunchAgent %s\n", plist.c_str());
        return 0;
    }
    Paths::ensureDirectory(Paths::home() + "/Library/LaunchAgents");
    Paths::ensureDirectory(Paths::home() + "/Library/Logs");
    std::string exe = selfPath();
    std::string content =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n<dict>\n"
        "\t<key>Label</key>\n\t<string>" + std::string(Paths::launchAgentLabel) + "</string>\n"
        "\t<key>ProgramArguments</key>\n\t<array>\n\t\t<string>" + exe + "</string>\n\t\t<string>run</string>\n\t</array>\n"
        "\t<key>RunAtLoad</key>\n\t<true/>\n"
        "\t<key>KeepAlive</key>\n\t<true/>\n"
        "\t<key>ProcessType</key>\n\t<string>Interactive</string>\n"
        "\t<key>LimitLoadToSessionType</key>\n\t<string>Aqua</string>\n"
        "\t<key>StandardOutPath</key>\n\t<string>" + Paths::logFile() + "</string>\n"
        "\t<key>StandardErrorPath</key>\n\t<string>" + Paths::logFile() + "</string>\n"
        "</dict>\n</plist>\n";
    if (agentLoaded()) shell("launchctl bootout " + serviceTarget());
    if (!Paths::writeFile(plist, content)) {
        std::fprintf(stderr, "error: cannot write %s\n", plist.c_str());
        return 1;
    }
    int rc = 0;
    std::string out = shell("launchctl bootstrap " + domainTarget() + " \"" + plist + "\"", &rc);
    if (rc != 0) {
        std::fprintf(stderr, "launchctl bootstrap failed (%d): %s\n", rc, out.c_str());
        return 1;
    }
    std::printf("installed LaunchAgent %s\n  program: %s run\n  log:     %s\n"
                "The daemon now starts at login and restarts if it exits. Use `eqapo status` to check it.\n",
                plist.c_str(), exe.c_str(), Paths::logFile().c_str());
    return 0;
}

int cmdLaunchctl(const Args& args) {
    std::string action = args.empty() ? "start" : args[0];
    std::string plist = Paths::launchAgentPlist();
    if (!Paths::fileExists(plist)) {
        std::fprintf(stderr, "LaunchAgent not installed; run `eqapo install` first (or `eqapo run` for the foreground).\n");
        return 1;
    }
    int rc = 0;
    std::string out;
    if (action == "stop") {
        out = shell("launchctl bootout " + serviceTarget(), &rc);
        std::printf("%s\n", rc == 0 ? "stopped" : ("stop failed: " + out).c_str());
        return rc;
    }
    if (!agentLoaded()) {
        out = shell("launchctl bootstrap " + domainTarget() + " \"" + plist + "\"", &rc);
        if (rc != 0) {
            std::fprintf(stderr, "launchctl bootstrap failed: %s\n", out.c_str());
            return rc;
        }
    }
    out = shell("launchctl kickstart " + std::string(action == "restart" ? "-k " : "") + serviceTarget(), &rc);
    std::printf("%s\n", rc == 0 ? (action == "restart" ? "restarted" : "started") : ("failed: " + out).c_str());
    return rc;
}

int cmdStatus(const Args& args) {
    std::string json = Paths::readFile(Paths::statusFile());
    DaemonStatus s;
    bool have = statusFromJson(json, s);
    bool alive = have && s.pid > 0 && kill(s.pid, 0) == 0;
    if (hasFlag(args, "--json")) {
        std::printf("%s", json.c_str());
        return alive ? 0 : 1;
    }
    if (!have) {
        std::printf("daemon: not running (no status file). Start with `eqapo install` or `eqapo run`.\n");
        return 1;
    }
    std::printf("daemon:    %s (pid %d, %s%s)\n", alive ? (s.state.c_str()) : "not running", s.pid,
                alive ? "started " : "last seen ", alive ? s.startedAt.c_str() : s.updatedAt.c_str());
    std::printf("agent:     %s\n", Paths::fileExists(Paths::launchAgentPlist()) ? (agentLoaded() ? "installed, loaded" : "installed, not loaded") : "not installed");
    if (!s.error.empty()) std::printf("error:     %s\n", s.error.c_str());
    std::printf("device:    %s (%s) %s\n", s.deviceName.c_str(), s.transport.c_str(), s.deviceUid.c_str());
    std::printf("format:    %.0f Hz, %u in / %u out channels, %u frames per block (%.1f ms)\n", s.sampleRate,
                s.inputChannels, s.outputChannels, s.bufferFrames, s.latencyMs);
    std::printf("config:    %s (%zu filters, loaded in %.2f ms)\n", s.configPath.c_str(), s.filters.size(), s.lastLoadMs);
    for (const std::string& f : s.filters) std::printf("             %s\n", f.c_str());
    for (const std::string& e : s.configErrors) std::printf("  config error: %s\n", e.c_str());
    std::printf("load:      %.2f%% of one core (last %.0f us, max %.0f us per block), %llu blocks, %llu overloads\n",
                s.avgLoadPercent, s.lastMicros, s.maxMicros, s.callbacks, s.overloads);
    std::printf("levels:    input peak %.3f, output peak %.3f%s\n", s.inputPeak, s.outputPeak,
                (alive && s.state == "running" && s.callbacks > 500 && s.inputPeak == 0.0)
                    ? "  (no input seen yet: either nothing is playing, or System Audio Recording permission is missing - "
                      "see `eqapo permission`)"
                    : "");
    std::printf("updated:   %s\n", s.updatedAt.c_str());
    return alive && s.state == "running" ? 0 : 1;
}

int cmdPermission(const Args&) {
    std::printf(
        "eqapo reads the audio that other apps send to the output device through a macOS process tap.\n"
        "macOS asks once for the 'System Audio Recording Only' permission when the daemon first starts.\n"
        "To review or change it: System Settings > Privacy & Security > Screen & System Audio Recording.\n"
        "After changing it run `eqapo restart`. Opening the pane now...\n");
    return std::system("open 'x-apple.systempreferences:com.apple.preference.security?Privacy_AudioCapture'");
}

int cmdReload(const Args&) {
    std::string pidText = Paths::readFile(Paths::pidFile());
    int pid = std::atoi(pidText.c_str());
    if (pid > 0 && kill(pid, 0) == 0) {
        if (kill(pid, SIGHUP) == 0) {
            std::printf("sent reload to daemon (pid %d)\n", pid);
            return 0;
        }
    }
    std::fprintf(stderr, "daemon is not running\n");
    return 1;
}

}  // namespace eqapo::cli
