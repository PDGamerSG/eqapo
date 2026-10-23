#include "engine/ChannelHelper.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "engine/Log.h"

namespace eqapo {

Channels ChannelHelper::getChannelNames(unsigned channelCount) {
    Channels names;
    switch (channelCount) {
        case 1: names = {"C"}; break;
        case 2: names = {"L", "R"}; break;
        case 4: names = {"L", "R", "RL", "RR"}; break;
        case 6: names = {"L", "R", "C", "LFE", "SL", "SR"}; break;
        case 8: names = {"L", "R", "C", "LFE", "RL", "RR", "SL", "SR"}; break;
        default: break;
    }
    for (unsigned c = static_cast<unsigned>(names.size()) + 1; c <= channelCount; c++)
        names.push_back(std::to_string(c));
    return names;
}

int ChannelHelper::getChannelIndex(const std::string& word, const Channels& names, bool allowNew) {
    if (word.empty()) return -1;
    if (std::isdigit(static_cast<unsigned char>(word[0]))) {
        long index = std::strtol(word.c_str(), nullptr, 10) - 1;
        if (index < 0 || index >= static_cast<long>(names.size())) {
            logMessage("Channel number " + word + " out of range (1 - " + std::to_string(names.size()) + ")");
            return -1;
        }
        return static_cast<int>(index);
    }

    auto find = [&](const std::string& n) { return std::find(names.begin(), names.end(), n); };
    auto pos = find(word);
    if (pos == names.end()) {
        if (word == "SL") pos = find("RL");
        else if (word == "SR") pos = find("RR");
        else if (word == "RL") pos = find("SL");
        else if (word == "RR") pos = find("SR");
        else if (word == "SUB") pos = find("LFE");
    }
    if (pos != names.end()) return static_cast<int>(pos - names.begin());
    if (!allowNew) logMessage("Invalid channel position " + word);
    return -1;
}

}  // namespace eqapo
