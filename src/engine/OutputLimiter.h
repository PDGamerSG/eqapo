#pragma once

namespace eqapo {

// Final safety stage for the device output. It applies the same gain to every
// channel so stereo imaging is preserved, reduces gain immediately when a
// sample would exceed the digital ceiling, and releases it slowly afterward.
// The limiter is intentionally allocation-free on the audio thread.
class OutputLimiter {
public:
    static constexpr float kCeiling = 0.90f;  // about -0.9 dBFS; leaves DAC/inter-sample headroom

    void initialize(float sampleRate, unsigned channelCount);
    void reset();
    void process(float** channels, unsigned frameCount);

    float gain() const { return gain_; }

private:
    float releaseStep_ = 0;
    float gain_ = 1.0f;
    unsigned channelCount_ = 0;
};

}  // namespace eqapo
