#include "engine/filters/PreampFilter.h"

#include <cmath>

#include "engine/Log.h"

namespace eqapo {

PreampFilter::PreampFilter(double dbGain) : dbGain_(dbGain), gain_(static_cast<float>(std::pow(10.0, dbGain / 20.0))) {}

Channels PreampFilter::initialize(float, unsigned, const Channels& channelNames) {
    channelCount_ = channelNames.size();
    return channelNames;
}

void PreampFilter::process(float** output, float** input, unsigned frameCount) {
    const float g = gain_;
    for (size_t c = 0; c < channelCount_; c++) {
        const float* in = input[c];
        float* out = output[c];
        for (unsigned j = 0; j < frameCount; j++) out[j] = in[j] * g;
    }
}

std::string PreampFilter::describe() const { return format("Preamp %g dB", dbGain_); }

}  // namespace eqapo
