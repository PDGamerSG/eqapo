#include "engine/OutputLimiter.h"

#include <algorithm>
#include <cmath>

namespace eqapo {

void OutputLimiter::initialize(float sampleRate, unsigned channelCount) {
    channelCount_ = channelCount;
    // About 80 ms to recover 63% of the remaining gain. This prevents a loud
    // passage from pumping when the next few samples are quieter.
    constexpr float releaseSeconds = 0.08f;
    releaseStep_ = sampleRate > 0 ? 1.0f - std::exp(-1.0f / (releaseSeconds * sampleRate)) : 1.0f;
    reset();
}

void OutputLimiter::reset() { gain_ = 1.0f; }

void OutputLimiter::process(float** channels, unsigned frameCount) {
    if (!channels || channelCount_ == 0 || frameCount == 0) return;

    for (unsigned i = 0; i < frameCount; i++) {
        float peak = 0.0f;
        for (unsigned c = 0; c < channelCount_; c++) {
            float sample = channels[c] ? channels[c][i] : 0.0f;
            if (!std::isfinite(sample)) {
                if (channels[c]) channels[c][i] = 0.0f;
                continue;
            }
            peak = std::max(peak, std::fabs(sample));
        }

        const float target = peak > kCeiling ? kCeiling / peak : 1.0f;
        if (target < gain_) {
            // Instantaneous attack guarantees that this sample cannot clip.
            gain_ = target;
        } else {
            // Slow release avoids audible gain pumping after a loud passage.
            gain_ = std::min(target, gain_ + releaseStep_ * (1.0f - gain_));
        }
        gain_ = std::clamp(gain_, 0.0f, 1.0f);

        for (unsigned c = 0; c < channelCount_; c++)
            if (channels[c]) channels[c][i] *= gain_;
    }
}

}  // namespace eqapo
