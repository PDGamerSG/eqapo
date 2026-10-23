#include "engine/dsp/MinimumPhase.h"

#include <cmath>
#include <complex>
#include <stdexcept>

#include "engine/dsp/RealFFT.h"

namespace eqapo {

namespace {
// Complex FFT (radix-2, double precision) used only at configuration time.
void complexFFT(std::vector<std::complex<double>>& a, bool inverse) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2 * M_PI / static_cast<double>(len) * (inverse ? 1 : -1);
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; j++) {
                std::complex<double> u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}
}  // namespace

std::vector<float> minimumPhaseImpulse(const std::vector<float>& mag, size_t filterLength) {
    if (mag.size() != filterLength) throw std::invalid_argument("magnitude size must equal filterLength");
    const size_t n = 2 * filterLength;
    const double threshold = std::pow(10.0, -100.0 / 20.0);
    const double logThreshold = std::log(threshold);

    // full symmetric log-magnitude spectrum
    std::vector<std::complex<double>> freq(n);
    for (size_t i = 0; i < filterLength; i++) {
        double g = mag[i];
        double lg = g < threshold ? logThreshold : std::log(g);
        freq[i] = {lg, 0.0};
        freq[n - i - 1] = {lg, 0.0};
    }

    // real cepstrum
    std::vector<std::complex<double>> time = freq;
    complexFFT(time, true);
    for (auto& v : time) v /= static_cast<double>(n);

    // fold: keep causal part, double the positive quefrencies
    for (size_t i = 1; i < filterLength; i++) {
        time[i] = {time[i].real() + time[n - i].real(), time[i].imag() - time[n - i].imag()};
        time[n - i] = {0.0, 0.0};
    }
    time[filterLength] = {time[filterLength].real(), -time[filterLength].imag()};

    // back to frequency domain and exponentiate -> minimum-phase spectrum
    freq = time;
    complexFFT(freq, false);
    for (auto& v : freq) {
        double eR = std::exp(v.real());
        v = {eR * std::cos(v.imag()), eR * std::sin(v.imag())};
    }

    // impulse response
    time = freq;
    complexFFT(time, true);
    std::vector<float> h(filterLength);
    for (size_t i = 0; i < filterLength; i++) {
        double factor = 0.5 * (1.0 + std::cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(n)));
        h[i] = static_cast<float>(time[i].real() / static_cast<double>(n) * factor);
    }
    return h;
}

}  // namespace eqapo
