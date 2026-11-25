#include "engine/dsp/RealFFT.h"

#if defined(__APPLE__)
#include <Accelerate/Accelerate.h>
#endif

#include <complex>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace eqapo {

#if !defined(__APPLE__)
namespace {
void fft(std::vector<std::complex<float>>& values, bool inverse) {
    const size_t n = values.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }
    constexpr float pi = 3.14159265358979323846f;
    for (size_t length = 2; length <= n; length <<= 1) {
        const float angle = (inverse ? 2.0f : -2.0f) * pi / static_cast<float>(length);
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for (size_t start = 0; start < n; start += length) {
            std::complex<float> factor(1.0f, 0.0f);
            for (size_t i = 0; i < length / 2; i++) {
                const std::complex<float> even = values[start + i];
                const std::complex<float> odd = values[start + i + length / 2] * factor;
                values[start + i] = even + odd;
                values[start + i + length / 2] = even - odd;
                factor *= step;
            }
        }
    }
}
}  // namespace
#endif

RealFFT::RealFFT(size_t n) : n_(n) {
    if (n < 2 || (n & (n - 1)) != 0) throw std::invalid_argument("RealFFT size must be a power of two");
    log2n_ = 0;
    while ((size_t{1} << log2n_) < n) log2n_++;
#if defined(__APPLE__)
    setup_ = vDSP_create_fftsetup(log2n_, kFFTRadix2);
    if (!setup_) throw std::runtime_error("vDSP_create_fftsetup failed");
    scratchRe_.resize(n / 2);
    scratchIm_.resize(n / 2);
#else
    scratch_.resize(n_);
#endif
}

#if defined(__APPLE__)
RealFFT::~RealFFT() { vDSP_destroy_fftsetup(static_cast<FFTSetup>(setup_)); }
#else
RealFFT::~RealFFT() = default;
#endif

void RealFFT::forward(const float* time, float* re, float* im) const {
#if defined(__APPLE__)
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
#else
    for (size_t i = 0; i < n_; i++) scratch_[i] = std::complex<float>(time[i], 0.0f);
    fft(scratch_, false);
    for (size_t k = 0; k <= n_ / 2; k++) {
        re[k] = scratch_[k].real();
        im[k] = scratch_[k].imag();
    }
#endif
}

void RealFFT::inverse(const float* re, float* im, float* time) const {
#if defined(__APPLE__)
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
#else
    scratch_[0] = std::complex<float>(re[0], 0.0f);
    scratch_[n_ / 2] = std::complex<float>(re[n_ / 2], 0.0f);
    for (size_t k = 1; k < n_ / 2; k++) {
        scratch_[k] = std::complex<float>(re[k], im[k]);
        scratch_[n_ - k] = std::conj(scratch_[k]);
    }
    fft(scratch_, true);
    for (size_t i = 0; i < n_; i++) time[i] = scratch_[i].real();
#endif
}

}  // namespace eqapo
