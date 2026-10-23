#pragma once
#include <vector>

#include "engine/IFilter.h"
#include "engine/filters/BiQuad.h"

namespace eqapo {

// `Filter n: ON <type> ...` — one BiQuad per selected channel.
class BiQuadFilter : public IFilter {
public:
    BiQuadFilter(BiQuad::Type type, double dbGain, double freq, double bandwidthOrQOrS, bool isBandwidthOrS,
                 bool isCornerFreq);

    Channels initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) override;
    void process(float** output, float** input, unsigned frameCount) override;
    std::string describe() const override;

    BiQuad::Type type() const { return type_; }
    double dbGain() const { return dbGain_; }
    double freq() const { return freq_; }
    double bandwidthOrQOrS() const { return bandwidthOrQOrS_; }
    bool isBandwidthOrS() const { return isBandwidthOrS_; }
    bool isCornerFreq() const { return isCornerFreq_; }
    // Frequency actually used for the biquad after corner-frequency adjustment.
    double effectiveFreq() const { return effectiveFreq_; }
    const BiQuad& biquad(size_t channel) const { return biquads_[channel]; }

private:
    BiQuad::Type type_;
    double dbGain_;
    double freq_;
    double bandwidthOrQOrS_;
    bool isBandwidthOrS_;
    bool isCornerFreq_;
    double effectiveFreq_ = 0;
    std::vector<BiQuad> biquads_;
};

}  // namespace eqapo
