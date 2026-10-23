#include "engine/filters/ConvolutionFilter.h"

#include <cmath>
#include <cstring>

#include "engine/Log.h"

namespace eqapo {

namespace {
ImpulseLoader g_loader = nullptr;
}

void setImpulseLoader(ImpulseLoader loader) { g_loader = loader; }

ConvolutionFilter::ConvolutionFilter(std::string filename) : filename_(std::move(filename)) {}

Channels ConvolutionFilter::initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) {
    convolvers_.clear();
    if (!g_loader) {
        logMessage("No impulse response loader available for " + filename_);
        return channelNames;
    }
    std::string error;
    ImpulseResponse ir = g_loader(filename_, error);
    if (ir.channels.empty()) {
        logMessage("Error while reading impulse response file " + filename_ + ": " + error);
        return channelNames;
    }
    if (std::fabs(sampleRate - ir.sampleRate) > 1.0) {
        logMessage(format("Impulse response sample rate (%g Hz) does not match device sample rate (%g Hz)", ir.sampleRate,
                          static_cast<double>(sampleRate)));
        return channelNames;
    }
    unsigned block = 32;
    while (block * 2 <= maxFrameCount) block *= 2;
    blockLength_ = block;

    convolvers_.clear();
    convolvers_.resize(channelNames.size());
    for (size_t c = 0; c < channelNames.size(); c++) {
        const std::vector<float>& h = ir.channels[c % ir.channels.size()];
        convolvers_[c].init(h.data(), h.size(), blockLength_);
    }
    pendingIn_.assign(channelNames.size(), std::vector<float>(blockLength_, 0.0f));
    pendingOut_.assign(channelNames.size(), std::vector<float>(blockLength_, 0.0f));
    pendingFill_ = 0;
    delayedMode_ = false;
    traceMessage("Convolving using impulse response file " + filename_);
    return channelNames;
}

void ConvolutionFilter::process(float** output, float** input, unsigned frameCount) {
    if (convolvers_.empty()) {
        for (size_t c = 0; c < pendingIn_.size(); c++)
            if (output[c] != input[c]) std::memcpy(output[c], input[c], frameCount * sizeof(float));
        return;
    }
    const unsigned block = blockLength_;
    unsigned done = 0;
    while (done < frameCount) {
        if (!delayedMode_ && frameCount - done >= block) {
            for (size_t c = 0; c < convolvers_.size(); c++) convolvers_[c].process(input[c] + done, output[c] + done);
            done += block;
            continue;
        }
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

std::string ConvolutionFilter::describe() const { return "Convolution " + filename_ + (active() ? "" : " (inactive)"); }

}  // namespace eqapo
