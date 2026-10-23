#pragma once
#include <cstddef>
#include <vector>

namespace eqapo {

// Real-input FFT of power-of-two size N built on Accelerate vDSP.
// forward(): N real samples -> N/2+1 complex bins (re/im arrays), unnormalised
//            (bin k = sum x[n] e^{-2πi kn/N}).
// inverse(): N/2+1 bins -> N real samples, unnormalised (caller divides by N).
class RealFFT {
public:
    explicit RealFFT(size_t n);
    ~RealFFT();
    RealFFT(const RealFFT&) = delete;
    RealFFT& operator=(const RealFFT&) = delete;

    size_t size() const { return n_; }
    size_t bins() const { return n_ / 2 + 1; }
    void forward(const float* time, float* re, float* im) const;
    void inverse(const float* re, float* im, float* time) const;

private:
    size_t n_;
    unsigned log2n_;
    void* setup_;  // FFTSetup
    mutable std::vector<float> scratchRe_, scratchIm_;
};

}  // namespace eqapo
