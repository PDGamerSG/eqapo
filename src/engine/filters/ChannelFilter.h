#pragma once
#include <vector>

#include "engine/IFilter.h"

namespace eqapo {

// `Channel: L R | 1 2 | all` — changes the channel selection, no audio processing.
class ChannelFilter : public IFilter {
public:
    explicit ChannelFilter(std::vector<std::string> words);
    bool getSelectChannels() const override { return true; }
    Channels initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) override;
    void process(float** output, float** input, unsigned frameCount) override {}
    std::string describe() const override;

private:
    std::vector<std::string> words_;  // upper-cased
};

}  // namespace eqapo
