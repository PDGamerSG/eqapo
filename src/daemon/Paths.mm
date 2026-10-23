#include "daemon/Paths.h"

#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <dirent.h>

namespace eqapo {

std::string Paths::home() {
    const char* h = std::getenv("HOME");
    return h ? h : "/tmp";
}

std::string Paths::root() {
    const char* override = std::getenv("EQAPO_HOME");
    if (override && *override) return override;
    return home() + "/Library/Application Support/eqapo";
}

std::string Paths::configDir() { return root() + "/config"; }
std::string Paths::configFile() { return configDir() + "/config.txt"; }
std::string Paths::presetsDir() { return configDir() + "/presets"; }
std::string Paths::statusFile() { return root() + "/status.json"; }
std::string Paths::pidFile() { return root() + "/eqapo.pid"; }
std::string Paths::logFile() { return home() + "/Library/Logs/eqapo.log"; }
std::string Paths::launchAgentPlist() { return home() + "/Library/LaunchAgents/" + std::string(launchAgentLabel) + ".plist"; }

bool Paths::ensureDirectory(const std::string& path) {
    std::string current;
    std::istringstream parts(path);
    std::string part;
    while (std::getline(parts, part, '/')) {
        if (part.empty()) {
            current += "/";
            continue;
        }
        current += part + "/";
        struct stat st{};
        if (stat(current.c_str(), &st) == 0) continue;
        if (mkdir(current.c_str(), 0755) != 0) return false;
    }
    return true;
}

bool Paths::fileExists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
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
    return rename(tmp.c_str(), path.c_str()) == 0;
}

static void copyTree(const std::string& from, const std::string& to) {
    DIR* d = opendir(from.c_str());
    if (!d) return;
    Paths::ensureDirectory(to);
    while (dirent* e = readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string src = from + "/" + name, dst = to + "/" + name;
        struct stat st{};
        if (stat(src.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) copyTree(src, dst);
        else if (!Paths::fileExists(dst)) Paths::writeFile(dst, Paths::readFile(src));
    }
    closedir(d);
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
        writeFile(configDir() + "/example.txt",
                  "Filter Settings file\n\nRoom EQ V5,01\nDated: 29.02.2012 20:04:50\n\nNotes:Simple bass boost\n\n"
                  "Equaliser: Generic\nNo measurement\n"
                  "Filter  1: ON  PK       Fc    20,0 Hz  Gain   4,0 dB  Q  1,00\n"
                  "Filter  2: ON  PK       Fc    45,0 Hz  Gain   2,0 dB  Q  1,00\n"
                  "Filter  3: OFF None   \n");
    }
    if (!bundledPresetsDir.empty() && fileExists(bundledPresetsDir)) copyTree(bundledPresetsDir, presetsDir());
    return true;
}

std::string Paths::executableDir() {
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return ".";
    char resolved[PATH_MAX];
    std::string path = realpath(buf, resolved) ? resolved : buf;
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

}  // namespace eqapo
