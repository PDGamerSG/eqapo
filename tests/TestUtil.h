// Shared helpers for DSP tests.
#pragma once
#include <cmath>
#include <vector>

#include "engine/IFilter.h"

namespace testutil {

// Steady-state gain (dB) of a mono filter for a sine at `freq`, measured over the
// second half of `seconds` seconds after the transient.
inline double measureGainDb(eqapo::IFilter& filter, double freq, double sampleRate, double seconds = 1.0,
                            unsigned block = 256) {
    // whole number of blocks so block-based filters never see a partial block
    const size_t total = ((static_cast<size_t>(seconds * sampleRate) + block - 1) / block) * block;
    std::vector<float> in(total), out(total);
    for (size_t i = 0; i < total; i++) in[i] = static_cast<float>(std::sin(2.0 * M_PI * freq * i / sampleRate));
    for (size_t pos = 0; pos < total; pos += block) {
        unsigned n = static_cast<unsigned>(std::min<size_t>(block, total - pos));
        float* ip = in.data() + pos;
        float* op = out.data() + pos;
        float* inPtrs[1] = {ip};
        float* outPtrs[1] = {op};
        filter.process(outPtrs, inPtrs, n);
    }
    double sumIn = 0, sumOut = 0;
    for (size_t i = total / 2; i < total; i++) {
        sumIn += double(in[i]) * in[i];
        sumOut += double(out[i]) * out[i];
    }
    return 10.0 * std::log10(sumOut / sumIn);
}

// Goertzel magnitude (linear) of a tone at `freq` in `x`.
inline double toneMagnitude(const float* x, size_t n, double freq, double sampleRate) {
    double w = 2.0 * M_PI * freq / sampleRate;
    double coeff = 2.0 * std::cos(w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) {
        s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    double re = s1 - s2 * std::cos(w);
    double im = s2 * std::sin(w);
    return 2.0 * std::sqrt(re * re + im * im) / n;
}

}  // namespace testutil
