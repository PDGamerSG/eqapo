#include "engine/filters/DelayFilter.h"

#include <cstring>

#include "engine/Log.h"

namespace eqapo {

DelayFilter::DelayFilter(double delay, bool isMs) : delay_(delay), isMs_(isMs) {}

Channels DelayFilter::initialize(float sampleRate, unsigned, const Channels& channelNames) {
    if (isMs_) bufferLength_ = static_cast<unsigned>(sampleRate * delay_ / 1000.0 + 0.5);
    else bufferLength_ = static_cast<unsigned>(delay_ + 0.5);
    buffers_.assign(channelNames.size(), std::vector<float>(bufferLength_, 0.0f));
    bufferOffset_ = 0;
    return channelNames;
}

void DelayFilter::process(float** output, float** input, unsigned frameCount) {
    const unsigned len = bufferLength_;
    const unsigned off = bufferOffset_;
    for (size_t c = 0; c < buffers_.size(); c++) {
        const float* in = input[c];
        float* out = output[c];
        float* buf = buffers_[c].data();
        if (len <= frameCount) {
            // whole delay line is flushed every block
            std::memcpy(out, buf + off, (len - off) * sizeof(float));
            std::memcpy(out + len - off, buf, off * sizeof(float));
            std::memmove(out + len, in, (frameCount - len) * sizeof(float));
            std::memcpy(buf, in + frameCount - len, len * sizeof(float));
        } else if (len < off + frameCount) {
            // wrap-around inside the ring buffer
            unsigned first = len - off;
            std::memcpy(out, buf + off, first * sizeof(float));
            std::memcpy(out + first, buf, (frameCount - first) * sizeof(float));
            std::memcpy(buf + off, in, first * sizeof(float));
            std::memcpy(buf, in + first, (frameCount - first) * sizeof(float));
        } else {
            std::memcpy(out, buf + off, frameCount * sizeof(float));
            std::memcpy(buf + off, in, frameCount * sizeof(float));
        }
    }
    if (len <= frameCount) bufferOffset_ = 0;
    else bufferOffset_ = (off + frameCount) % len;
}

std::string DelayFilter::describe() const { return format("Delay %g %s", delay_, isMs_ ? "ms" : "samples"); }

}  // namespace eqapo
