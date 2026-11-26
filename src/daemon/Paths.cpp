#include "daemon/Paths.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace eqapo {

namespace fs = std::filesystem;

std::string Paths::home() {
#if defined(_WIN32)
    const char* h = std::getenv("USERPROFILE");
#else
    const char* h = std::getenv("HOME");
#endif
    return h && *h ? h : fs::temp_directory_path().string();
}

std::string Paths::root() {
    const char* override = std::getenv("EQAPO_HOME");
    if (override && *override) return override;
#if defined(_WIN32)
    const char* config = std::getenv("LOCALAPPDATA");
    const std::string defaultConfig = home() + "/AppData/Local";
#else
    const char* config = std::getenv("XDG_CONFIG_HOME");
    const std::string defaultConfig = home() + "/.config";
#endif
    return (config && *config ? std::string(config) : defaultConfig) + "/eqapo";
}

std::string Paths::configDir() { return root() + "/config"; }
std::string Paths::configFile() { return configDir() + "/config.txt"; }
std::string Paths::presetsDir() { return configDir() + "/presets"; }
std::string Paths::statusFile() { return root() + "/status.json"; }
std::string Paths::pidFile() { return root() + "/eqapo.pid"; }
std::string Paths::logFile() { return root() + "/eqapo.log"; }
std::string Paths::launchAgentPlist() { return root() + "/eqapo.service"; }

bool Paths::ensureDirectory(const std::string& path) {
    std::error_code error;
    fs::create_directories(path, error);
    return !error;
}

bool Paths::fileExists(const std::string& path) {
    std::error_code error;
    return fs::exists(path, error);
}

std::string Paths::readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool Paths::writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << content;
    return f.good();
}

bool Paths::writeFileAtomic(const std::string& path, const std::string& content) {
    std::string tmp = path + ".tmp";
    if (!writeFile(tmp, content)) return false;
    std::error_code error;
    fs::rename(tmp, path, error);
    return !error;
}

static void copyTree(const fs::path& source, const fs::path& destination) {
    std::error_code error;
    fs::create_directories(destination, error);
    if (error) return;
    for (fs::directory_iterator entry(source, error), end; !error && entry != end; entry.increment(error)) {
        const fs::path target = destination / entry->path().filename();
        if (entry->is_directory(error)) {
            copyTree(entry->path(), target);
        } else if (!error) {
            fs::copy_file(entry->path(), target, fs::copy_options::skip_existing, error);
        }
        error.clear();
    }
}

bool Paths::ensureDefaultConfig(const std::string& bundledPresetsDir) {
    if (!ensureDirectory(configDir())) return false;
    if (!fileExists(configFile())) {
        writeFile(configFile(),
                  "# eqapo configuration (Equalizer APO syntax). Saved changes apply instantly.\n"
                  "# Docs: https://sourceforge.net/p/equalizerapo/wiki/Configuration%20reference/\n"
                  "Preamp: -6 dB\n"
                  "Include: example.txt\n"
                  "GraphicEQ: 25 0; 40 0; 63 0; 100 0; 160 0; 250 0; 400 0; 630 0; 1000 0; 1600 0; 2500 0; 4000 0; 6300 0; "
                  "10000 0; 16000 0\n");
    }
    if (!fileExists(configDir() + "/example.txt")) {
        writeFile(configDir() + "/example.txt", "Filter Settings file\n\nRoom EQ V5,01\nDated: 29.02.2012 20:04:50\n\n"
                                                     "Notes:Simple bass boost\n\nEqualiser: Generic\nNo measurement\n"
                                                     "Filter  1: ON  PK       Fc    20,0 Hz  Gain   4,0 dB  Q  1,00\n"
                                                     "Filter  2: ON  PK       Fc    45,0 Hz  Gain   2,0 dB  Q  1,00\n"
                                                     "Filter  3: OFF None   \n");
    }
    if (!bundledPresetsDir.empty()) copyTree(bundledPresetsDir, presetsDir());
    return true;
}

std::string Paths::executableDir() {
#if defined(_WIN32)
    std::string path(32768, '\0');
    const auto length = GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return ".";
    path.resize(length);
#else
    std::error_code error;
    auto path = fs::read_symlink("/proc/self/exe", error);
    if (error) return ".";
#endif
    return fs::path(path).parent_path().generic_string();
}

}  // namespace eqapo
