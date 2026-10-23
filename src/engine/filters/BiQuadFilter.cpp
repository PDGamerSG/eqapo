#include "engine/filters/BiQuadFilter.h"

#include <cmath>

#include "engine/Log.h"

namespace eqapo {

BiQuadFilter::BiQuadFilter(BiQuad::Type type, double dbGain, double freq, double bandwidthOrQOrS,
                           bool isBandwidthOrS, bool isCornerFreq)
    : type_(type),
      dbGain_(dbGain),
      freq_(freq),
      bandwidthOrQOrS_(bandwidthOrQOrS),
      isBandwidthOrS_(isBandwidthOrS),
      isCornerFreq_(isCornerFreq) {}

Channels BiQuadFilter::initialize(float sampleRate, unsigned, const Channels& channelNames) {
    double biquadFreq = freq_;
    if (isCornerFreq_ && (type_ == BiQuad::LOW_SHELF || type_ == BiQuad::HIGH_SHELF)) {
        double s = bandwidthOrQOrS_;
        if (!isBandwidthOrS_) {  // Q given: derive S
            double q = bandwidthOrQOrS_;
            double a = std::pow(10.0, dbGain_ / 40.0);
            s = 1.0 / ((1.0 / (q * q) - 2.0) / (a + 1.0 / a) + 1.0);
        }
        // frequency adjustment so that Fc is the corner frequency (as on the DCX2496)
        double centerFreqFactor = std::pow(10.0, std::fabs(dbGain_) / 80.0 / s);
        if (type_ == BiQuad::LOW_SHELF) biquadFreq *= centerFreqFactor;
        else biquadFreq /= centerFreqFactor;
    }
    effectiveFreq_ = biquadFreq;

    biquads_.clear();
    biquads_.reserve(channelNames.size());
    for (size_t i = 0; i < channelNames.size(); i++)
        biquads_.emplace_back(type_, dbGain_, biquadFreq, sampleRate, bandwidthOrQOrS_, isBandwidthOrS_);
    return channelNames;
}

void BiQuadFilter::process(float** output, float** input, unsigned frameCount) {
    for (size_t i = 0; i < biquads_.size(); i++) {
        BiQuad bq = biquads_[i];
        const float* in = input[i];
        float* out = output[i];
        for (unsigned j = 0; j < frameCount; j++) out[j] = static_cast<float>(bq.process(in[j]));
        bq.removeDenormals();
        biquads_[i] = bq;
    }
}

std::string BiQuadFilter::describe() const {
    static const char* names[] = {"LP", "HP", "BP", "NO", "AP", "PK", "LS", "HS"};
    return format("%s Fc %g Hz Gain %g dB %s %g", names[type_], freq_, dbGain_,
                  isBandwidthOrS_ ? ((type_ == BiQuad::LOW_SHELF || type_ == BiQuad::HIGH_SHELF) ? "S" : "BW")
                                  : "Q",
                  bandwidthOrQOrS_);
}

}  // namespace eqapo
