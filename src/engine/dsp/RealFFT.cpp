#include "engine/dsp/RealFFT.h"

#include <Accelerate/Accelerate.h>

#include <cstring>
#include <stdexcept>

namespace eqapo {

RealFFT::RealFFT(size_t n) : n_(n) {
    if (n < 2 || (n & (n - 1)) != 0) throw std::invalid_argument("RealFFT size must be a power of two");
    log2n_ = 0;
    while ((size_t{1} << log2n_) < n) log2n_++;
    setup_ = vDSP_create_fftsetup(log2n_, kFFTRadix2);
    if (!setup_) throw std::runtime_error("vDSP_create_fftsetup failed");
    scratchRe_.resize(n / 2);
    scratchIm_.resize(n / 2);
}

RealFFT::~RealFFT() { vDSP_destroy_fftsetup(static_cast<FFTSetup>(setup_)); }

void RealFFT::forward(const float* time, float* re, float* im) const {
    const vDSP_Length half = n_ / 2;
    DSPSplitComplex split{scratchRe_.data(), scratchIm_.data()};
    // even samples -> real, odd samples -> imag
    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(time), 2, &split, 1, half);
    vDSP_fft_zrip(static_cast<FFTSetup>(setup_), &split, 1, log2n_, kFFTDirection_Forward);
    // vDSP packs: re[0] = 2*DC, im[0] = 2*Nyquist, others scaled by 2
    const float scale = 0.5f;
    re[0] = split.realp[0] * scale;
    im[0] = 0.0f;
    re[half] = split.imagp[0] * scale;
    im[half] = 0.0f;
    for (size_t k = 1; k < half; k++) {
        re[k] = split.realp[k] * scale;
        im[k] = split.imagp[k] * scale;
    }
}

void RealFFT::inverse(const float* re, float* im, float* time) const {
    const vDSP_Length half = n_ / 2;
    DSPSplitComplex split{scratchRe_.data(), scratchIm_.data()};
    split.realp[0] = re[0];
    split.imagp[0] = re[half];
    for (size_t k = 1; k < half; k++) {
        split.realp[k] = re[k];
        split.imagp[k] = im[k];
    }
    vDSP_fft_zrip(static_cast<FFTSetup>(setup_), &split, 1, log2n_, kFFTDirection_Inverse);
    // With the forward output halved above, zrip inverse yields N * x[n] (FFTW convention).
    vDSP_ztoc(&split, 1, reinterpret_cast<DSPComplex*>(time), 2, half);
}

}  // namespace eqapo
