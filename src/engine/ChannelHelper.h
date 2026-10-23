#pragma once
#include <string>
#include <vector>

namespace eqapo {

using Channels = std::vector<std::string>;

// Channel naming identical to Equalizer APO's defaults for a given channel count:
// 1: C | 2: L R | 4: L R RL RR | 6: L R C LFE SL SR | 8: L R C LFE RL RR SL SR | else 1..N
struct ChannelHelper {
    static Channels getChannelNames(unsigned channelCount);
    // Resolves a channel word (name or 1-based number) to an index into `names`,
    // -1 when not found. Accepts the aliases SL<->RL, SR<->RR and SUB->LFE.
    static int getChannelIndex(const std::string& word, const Channels& names, bool allowNew = false);
};

}  // namespace eqapo
