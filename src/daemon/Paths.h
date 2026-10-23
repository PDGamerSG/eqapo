#pragma once
#include <string>

namespace eqapo {

// All user-facing file locations. Override the root with EQAPO_HOME for tests.
struct Paths {
    static std::string home();              // $HOME
    static std::string root();              // ~/Library/Application Support/eqapo
    static std::string configDir();         // root/config
    static std::string configFile();        // root/config/config.txt
    static std::string presetsDir();        // root/config/presets
    static std::string statusFile();        // root/status.json
    static std::string pidFile();           // root/eqapo.pid
    static std::string logFile();           // ~/Library/Logs/eqapo.log
    static std::string launchAgentPlist();  // ~/Library/LaunchAgents/com.eqapo.daemon.plist
    static constexpr const char* launchAgentLabel = "com.eqapo.daemon";

    static bool ensureDirectory(const std::string& path);
    static bool fileExists(const std::string& path);
    static std::string readFile(const std::string& path);
    static bool writeFile(const std::string& path, const std::string& content);
    static bool writeFileAtomic(const std::string& path, const std::string& content);
    // Creates config dir, default config.txt and presets if missing. Returns false on failure.
    static bool ensureDefaultConfig(const std::string& bundledPresetsDir);
    // Directory containing the running executable.
    static std::string executableDir();
};

}  // namespace eqapo
