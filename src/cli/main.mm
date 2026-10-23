// eqapo — Equalizer APO + PC Equalizer for macOS. Command line entry point.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cli/Commands.h"
#include "daemon/Paths.h"

#ifndef EQAPO_VERSION
#define EQAPO_VERSION "0.1.0"
#endif
#ifndef EQAPO_SOURCE_PRESETS_DIR
#define EQAPO_SOURCE_PRESETS_DIR ""
#endif

namespace eqapo::cli {
std::string bundledPresetsDir() {
    std::string exe = Paths::executableDir();
    const std::string candidates[] = {exe + "/../share/eqapo/presets", exe + "/presets", exe + "/../presets",
                                      EQAPO_SOURCE_PRESETS_DIR};
    for (const std::string& c : candidates)
        if (!c.empty() && Paths::fileExists(c + "/eq")) return c;
    return "";
}
}  // namespace eqapo::cli

namespace {
void usage() {
    std::printf(
        "eqapo %s — system-wide parametric equalizer for macOS (Equalizer APO configuration syntax)\n\n"
        "Daemon:\n"
        "  eqapo run [--buffer N] [--trace]      run in the foreground (Ctrl-C stops and restores audio)\n"
        "  eqapo install | uninstall             install/remove the login LaunchAgent (background service)\n"
        "  eqapo start | stop | restart          control the LaunchAgent\n"
        "  eqapo status                          show device, sample rate, filters, CPU, errors\n"
        "  eqapo reload                          force a configuration reload\n"
        "  eqapo permission                      explain/open the System Audio Recording permission\n"
        "  eqapo devices                         list output devices\n"
        "Configuration (%s):\n"
        "  eqapo config path | show | edit | reset | off\n"
        "  eqapo preset list | show NAME | apply NAME [--delay EFFECT]\n"
        "Offline tools:\n"
        "  eqapo process IN OUT.wav [--config FILE] [--frames N]\n"
        "  eqapo bench [--config FILE] [--rate HZ] [--frames N] [--channels N]\n"
        "  eqapo gen OUT.wav [--rate HZ] [--seconds S] [--tone HZ:AMP ...]\n"
        "  eqapo analyze FILE FREQ [FREQ ...]\n"
        "  eqapo selftest                        live end-to-end check of the running pipeline\n",
        EQAPO_VERSION, eqapo::Paths::configFile().c_str());
}
}  // namespace

int main(int argc, char** argv) {
    using namespace eqapo::cli;
    if (argc < 2) {
        usage();
        return 2;
    }
    std::string cmd = argv[1];
    Args args(argv + 2, argv + argc);
    eqapo::Paths::ensureDefaultConfig(bundledPresetsDir());

    if (cmd == "process") return cmdProcess(args);
    if (cmd == "bench") return cmdBench(args);
    if (cmd == "gen") return cmdGen(args);
    if (cmd == "analyze") return cmdAnalyze(args);
    if (cmd == "preset") return cmdPreset(args);
    if (cmd == "config") return cmdConfig(args);
    if (cmd == "devices") return cmdDevices(args);
    if (cmd == "run") return cmdRun(args);
    if (cmd == "install" || cmd == "uninstall") return cmdInstall(Args{cmd});
    if (cmd == "start" || cmd == "stop" || cmd == "restart") return cmdLaunchctl(Args{cmd});
    if (cmd == "status") return cmdStatus(args);
    if (cmd == "reload") return cmdReload(args);
    if (cmd == "permission") return cmdPermission(args);
    if (cmd == "selftest") return cmdSelftest(args);
    if (cmd == "version" || cmd == "--version") {
        std::printf("eqapo %s\n", EQAPO_VERSION);
        return 0;
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        usage();
        return 0;
    }
    std::fprintf(stderr, "unknown command: %s\n\n", cmd.c_str());
    usage();
    return 2;
}
