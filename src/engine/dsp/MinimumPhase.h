#pragma once
#include <cstddef>
#include <vector>

namespace eqapo {

// Builds a minimum-phase FIR of `filterLength` taps whose magnitude response is
// `linearMagnitude` (filterLength values for bins 0..filterLength-1 of a
// 2*filterLength-point spectrum, i.e. frequencies i*fs/(2*filterLength)).
// Uses the real-cepstrum method (Equalizer APO's GraphicEQ algorithm):
// log|H| (floored at -100 dB) -> cepstrum -> fold -> exp -> impulse,
// then the first filterLength samples are windowed by a half raised cosine.
std::vector<float> minimumPhaseImpulse(const std::vector<float>& linearMagnitude, size_t filterLength);

}  // namespace eqapo
