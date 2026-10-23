#pragma once
#include <vector>

#include "engine/GainIterator.h"
#include "engine/IFilter.h"
#include "engine/dsp/PartitionedConvolver.h"

namespace eqapo {

// `GraphicEQ: f g; f g; ...` — minimum-phase FIR (16384 taps) matching the
// log-frequency interpolated node gains, run through a partitioned convolver.
class GraphicEQFilter : public IFilter {
public:
    GraphicEQFilter(std::vector<FilterNode> sortedNodes, unsigned filterLength = 16384);
    Channels initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) override;
    void process(float** output, float** input, unsigned frameCount) override;
    std::string describe() const override;
    const std::vector<FilterNode>& nodes() const { return nodes_; }
    const std::vector<float>& impulse() const { return impulse_; }

private:
    std::vector<FilterNode> nodes_;
    unsigned filterLength_;
    unsigned blockLength_ = 0;
    std::vector<float> impulse_;
    std::vector<PartitionedConvolver> convolvers_;
    std::vector<float> scratchIn_, scratchOut_;
    // sub-block handling when frameCount < blockLength
    std::vector<std::vector<float>> pendingIn_, pendingOut_;
    unsigned pendingFill_ = 0;
    bool delayedMode_ = false;
};

}  // namespace eqapo
