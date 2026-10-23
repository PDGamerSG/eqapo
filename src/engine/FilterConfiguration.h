#pragma once
#include <memory>
#include <string>
#include <vector>

#include "engine/IFilter.h"

namespace eqapo {

struct FilterInfo {
    std::unique_ptr<IFilter> filter;
    bool inPlace = true;
    std::vector<size_t> inChannels;   // indices into the all-channel buffer set
    std::vector<size_t> outChannels;
};

// A fully built filter chain with its working buffers. Immutable after
// construction; process()/read()/write() run on the audio thread.
class FilterConfiguration {
public:
    FilterConfiguration(std::vector<FilterInfo> infos, unsigned realChannelCount, unsigned outputChannelCount,
                        unsigned allChannelCount, unsigned maxFrameCount);

    void read(float** input, unsigned frameCount);
    void process(unsigned frameCount);
    // Crossfades this configuration's output towards `next`'s output. Returns the new counter.
    unsigned doTransition(const FilterConfiguration& next, unsigned frameCount, unsigned transitionCounter,
                          unsigned transitionLength);
    void write(float** output, unsigned frameCount) const;

    bool isEmpty() const { return infos_.empty(); }
    size_t filterCount() const { return infos_.size(); }
    unsigned allChannelCount() const { return allChannelCount_; }
    std::vector<std::string> describe() const;

private:
    unsigned realChannelCount_;
    unsigned outputChannelCount_;
    unsigned allChannelCount_;
    std::vector<float> storage_;
    std::vector<float*> allSamples_;
    std::vector<float*> allSamples2_;
    std::vector<float*> currentSamples_;
    std::vector<float*> currentSamples2_;
    std::vector<FilterInfo> infos_;
};

}  // namespace eqapo
