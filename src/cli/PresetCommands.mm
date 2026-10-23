// preset and config sub-commands.
#include <dirent.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "cli/Commands.h"
#include "daemon/Paths.h"
#include "engine/StringHelper.h"

namespace eqapo::cli {

namespace {
std::vector<std::string> listTxt(const std::string& dir) {
    std::vector<std::string> names;
    DIR* d = opendir(dir.c_str());
    if (!d) return names;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (StringHelper::endsWith(n, ".txt")) names.push_back(n.substr(0, n.size() - 4));
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    return names;
}

// Case-insensitive, "[build] " prefix optional, exact-name-then-substring match.
std::string resolvePreset(const std::string& dir, const std::string& query, std::string& error) {
    std::vector<std::string> names = listTxt(dir);
    std::string q = StringHelper::toLowerCase(StringHelper::trim(query));
    for (const std::string& n : names)
        if (StringHelper::toLowerCase(n) == q) return n;
    for (const std::string& n : names)
        if (StringHelper::toLowerCase(n) == "[build] " + q) return n;
    std::vector<std::string> partial;
    for (const std::string& n : names)
        if (StringHelper::toLowerCase(n).find(q) != std::string::npos) partial.push_back(n);
    if (partial.size() == 1) return partial[0];
    if (partial.empty()) error = "no preset matches \"" + query + "\"";
    else error = "ambiguous preset \"" + query + "\": " + StringHelper::join(partial, ", ");
    return "";
}
}  // namespace

int cmdPreset(const Args& args) {
    std::vector<std::string> pos = positional(args);
    std::string sub = pos.empty() ? "list" : pos[0];
    std::string eqDir = Paths::presetsDir() + "/eq";
    std::string delayDir = Paths::presetsDir() + "/delay";
    if (sub == "list") {
        std::printf("EQ presets (%s):\n", eqDir.c_str());
        for (const std::string& n : listTxt(eqDir)) std::printf("  %s\n", n.c_str());
        std::printf("Delay/echo effects (%s):\n", delayDir.c_str());
        for (const std::string& n : listTxt(delayDir)) std::printf("  %s\n", n.c_str());
        return 0;
    }
    if (pos.size() < 2) {
        std::fprintf(stderr, "usage: eqapo preset list | show NAME | apply NAME [--delay EFFECT] | off\n");
        return 2;
    }
    std::string error;
    if (sub == "show") {
        std::string name = resolvePreset(eqDir, pos[1], error);
        if (name.empty()) name = resolvePreset(delayDir, pos[1], error);
        if (name.empty()) {
            std::fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }
        std::string path = Paths::fileExists(eqDir + "/" + name + ".txt") ? eqDir + "/" + name + ".txt"
                                                                           : delayDir + "/" + name + ".txt";
        std::printf("%s", Paths::readFile(path).c_str());
        return 0;
    }
    if (sub == "apply") {
        std::string name = resolvePreset(eqDir, pos[1], error);
        if (name.empty()) {
            std::fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }
        std::string delayName;
        std::string delayQuery = optionValue(args, "--delay", "");
        if (!delayQuery.empty()) {
            delayName = resolvePreset(delayDir, delayQuery, error);
            if (delayName.empty()) {
                std::fprintf(stderr, "error: %s\n", error.c_str());
                return 1;
            }
        }
        std::string config = "# Written by `eqapo preset apply`. Edit freely; the daemon reloads on save.\n";
        config += "Include: presets/eq/" + name + ".txt\n";
        if (!delayName.empty()) config += "Include: presets/delay/" + delayName + ".txt\n";
        if (!Paths::writeFileAtomic(Paths::configFile(), config)) {
            std::fprintf(stderr, "error: cannot write %s\n", Paths::configFile().c_str());
            return 1;
        }
        std::printf("applied preset \"%s\"%s -> %s\n", name.c_str(),
                    delayName.empty() ? "" : (" with effect \"" + delayName + "\"").c_str(), Paths::configFile().c_str());
        return 0;
    }
    std::fprintf(stderr, "unknown preset sub-command: %s\n", sub.c_str());
    return 2;
}

int cmdConfig(const Args& args) {
    std::vector<std::string> pos = positional(args);
    std::string sub = pos.empty() ? "path" : pos[0];
    if (sub == "path") {
        std::printf("%s\n", Paths::configFile().c_str());
        return 0;
    }
    if (sub == "show") {
        std::printf("%s", Paths::readFile(Paths::configFile()).c_str());
        return 0;
    }
    if (sub == "edit") {
        std::string cmd = "open -t \"" + Paths::configFile() + "\"";
        return std::system(cmd.c_str());
    }
    if (sub == "reset") {
        unlink(Paths::configFile().c_str());
        Paths::ensureDefaultConfig("");
        std::printf("reset %s to the default configuration\n", Paths::configFile().c_str());
        return 0;
    }
    if (sub == "off") {
        Paths::writeFileAtomic(Paths::configFile(), "# eqapo: processing disabled (empty configuration)\n");
        std::printf("wrote an empty configuration (audio passes through unchanged)\n");
        return 0;
    }
    std::fprintf(stderr, "usage: eqapo config path | show | edit | reset | off\n");
    return 2;
}

}  // namespace eqapo::cli
