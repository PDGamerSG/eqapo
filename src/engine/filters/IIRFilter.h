#pragma once
#include <vector>

#include "engine/IFilter.h"

namespace eqapo {

// `Filter: ON IIR Order m Coefficients b0..bm a0..am` — generic direct form I IIR.
class IIRFilter : public IFilter {
public:
    explicit IIRFilter(const std::vector<double>& coefficients);  // size == 2*(order+1)
    Channels initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) override;
    void process(float** output, float** input, unsigned frameCount) override;
    std::string describe() const override;
    unsigned order() const { return order_; }

private:
    unsigned order_;
    double b0_;
    std::vector<double> a_;  // negated, normalised a1..am
    std::vector<double> b_;  // normalised b1..bm
    size_t channelCount_ = 0;
    std::vector<double> x_;  // order_ * channels
    std::vector<double> y_;
};

}  // namespace eqapo
