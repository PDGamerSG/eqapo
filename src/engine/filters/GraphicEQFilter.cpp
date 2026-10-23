#include "engine/filters/GraphicEQFilter.h"

#include <cmath>
#include <cstring>

#include "engine/Log.h"
#include "engine/dsp/MinimumPhase.h"

namespace eqapo {

GraphicEQFilter::GraphicEQFilter(std::vector<FilterNode> sortedNodes, unsigned filterLength)
    : nodes_(std::move(sortedNodes)), filterLength_(filterLength) {}

Channels GraphicEQFilter::initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) {
    // block length: largest power of two <= maxFrameCount (at least 32)
    unsigned block = 32;
    while (block * 2 <= maxFrameCount) block *= 2;
    blockLength_ = block;

    GainIterator gains(nodes_);
    std::vector<float> magnitude(filterLength_);
    for (unsigned i = 0; i < filterLength_; i++) {
        double freq = static_cast<double>(i) * sampleRate / (2.0 * filterLength_);
        magnitude[i] = static_cast<float>(std::pow(10.0, gains.gainAt(freq) / 20.0));
    }
    impulse_ = minimumPhaseImpulse(magnitude, filterLength_);

    convolvers_.clear();
    convolvers_.resize(channelNames.size());
    for (auto& c : convolvers_) c.init(impulse_.data(), impulse_.size(), blockLength_);
    pendingIn_.assign(channelNames.size(), std::vector<float>(blockLength_, 0.0f));
    pendingOut_.assign(channelNames.size(), std::vector<float>(blockLength_, 0.0f));
    pendingFill_ = 0;
    delayedMode_ = false;
    return channelNames;
}

void GraphicEQFilter::process(float** output, float** input, unsigned frameCount) {
    // Fast path: frameCount is a multiple of the block length and no partial block pending.
    const unsigned block = blockLength_;
    unsigned done = 0;
    while (done < frameCount) {
        if (!delayedMode_ && frameCount - done >= block) {
            for (size_t c = 0; c < convolvers_.size(); c++) convolvers_[c].process(input[c] + done, output[c] + done);
            done += block;
            continue;
        }
        // Partial block: switch permanently to buffered mode (one block of extra latency)
        // so the output stream stays continuous whatever frame counts follow.
        delayedMode_ = true;
        unsigned n = std::min(block - pendingFill_, frameCount - done);
        for (size_t c = 0; c < convolvers_.size(); c++) {
            std::memcpy(pendingIn_[c].data() + pendingFill_, input[c] + done, n * sizeof(float));
            std::memcpy(output[c] + done, pendingOut_[c].data() + pendingFill_, n * sizeof(float));
        }
        pendingFill_ += n;
        done += n;
        if (pendingFill_ == block) {
            for (size_t c = 0; c < convolvers_.size(); c++)
                convolvers_[c].process(pendingIn_[c].data(), pendingOut_[c].data());
            pendingFill_ = 0;
        }
    }
}

std::string GraphicEQFilter::describe() const { return format("GraphicEQ %zu nodes", nodes_.size()); }

}  // namespace eqapo
