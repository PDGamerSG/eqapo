#pragma once
#include <vector>

#include "engine/IFilter.h"
#include "engine/dsp/PartitionedConvolver.h"

namespace eqapo {

// Impulse response loader: returns planar channels; empty on failure.
struct ImpulseResponse {
    double sampleRate = 0;
    std::vector<std::vector<float>> channels;
};
using ImpulseLoader = ImpulseResponse (*)(const std::string& path, std::string& error);
void setImpulseLoader(ImpulseLoader loader);

// `Convolution: file` — convolves selected channels with the file's channels (round-robin).
class ConvolutionFilter : public IFilter {
public:
    explicit ConvolutionFilter(std::string filename);
    Channels initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) override;
    void process(float** output, float** input, unsigned frameCount) override;
    std::string describe() const override;
    bool active() const { return !convolvers_.empty(); }

private:
    std::string filename_;
    unsigned blockLength_ = 0;
    std::vector<PartitionedConvolver> convolvers_;
    std::vector<std::vector<float>> pendingIn_, pendingOut_;
    unsigned pendingFill_ = 0;
    bool delayedMode_ = false;
};

}  // namespace eqapo
