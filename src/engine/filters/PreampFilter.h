#pragma once
#include "engine/IFilter.h"

namespace eqapo {

// `Preamp: x dB` — gain on selected channels.
class PreampFilter : public IFilter {
public:
    explicit PreampFilter(double dbGain);
    Channels initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) override;
    void process(float** output, float** input, unsigned frameCount) override;
    std::string describe() const override;
    double dbGain() const { return dbGain_; }

private:
    double dbGain_;
    float gain_;
    size_t channelCount_ = 0;
};

}  // namespace eqapo
