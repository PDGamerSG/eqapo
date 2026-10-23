// CLI sub-commands. Each returns a process exit code.
#pragma once
#include <string>
#include <vector>

namespace eqapo::cli {

using Args = std::vector<std::string>;

int cmdProcess(const Args& args);   // process in out [--config FILE]
int cmdBench(const Args& args);     // bench [--config FILE] [--rate HZ] [--frames N] [--channels N] [--seconds S]
int cmdGen(const Args& args);       // gen out.wav [--rate HZ] [--seconds S] [--tone HZ:AMP]...
int cmdAnalyze(const Args& args);   // analyze file.wav FREQ [FREQ...]  -> dBFS per tone per channel
int cmdPreset(const Args& args);    // preset list | show NAME | apply NAME [--delay NAME]
int cmdConfig(const Args& args);    // config path | edit | reset | show
int cmdDevices(const Args& args);   // devices
int cmdRun(const Args& args);       // run [--buffer N] [--trace]
int cmdInstall(const Args& args);   // install | uninstall
int cmdLaunchctl(const Args& args); // start | stop | restart
int cmdStatus(const Args& args);    // status
int cmdReload(const Args& args);    // reload
int cmdPermission(const Args& args); // permission
int cmdSelftest(const Args& args);  // selftest [--seconds S]

// shared helpers
std::string optionValue(const Args& args, const std::string& name, const std::string& fallback);
bool hasFlag(const Args& args, const std::string& name);
std::vector<std::string> positional(const Args& args);

}  // namespace eqapo::cli
