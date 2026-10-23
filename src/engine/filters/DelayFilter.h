#pragma once
#include <vector>

#include "engine/IFilter.h"

namespace eqapo {

// `Delay: t ms` or `Delay: n samples` — integer sample delay per selected channel.
class DelayFilter : public IFilter {
public:
    DelayFilter(double delay, bool isMs);
    Channels initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) override;
    void process(float** output, float** input, unsigned frameCount) override;
    std::string describe() const override;
    unsigned delaySamples() const { return bufferLength_; }

private:
    double delay_;
    bool isMs_;
    unsigned bufferLength_ = 0;
    unsigned bufferOffset_ = 0;
    std::vector<std::vector<float>> buffers_;
};

}  // namespace eqapo
