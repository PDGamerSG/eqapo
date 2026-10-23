#include "engine/dsp/PartitionedConvolver.h"

#include <Accelerate/Accelerate.h>

#include <algorithm>
#include <cstring>

namespace eqapo {

void PartitionedConvolver::init(const float* h, size_t hlen, size_t flen) {
    flen_ = flen;
    fft_ = std::make_unique<RealFFT>(2 * flen);
    bins_ = fft_->bins();
    numPartitions_ = std::max<size_t>(1, (hlen + flen - 1) / flen);
    mixPos_ = 0;
    timeBuf_.assign(2 * flen, 0.0f);
    inRe_.assign(bins_, 0.0f);
    inIm_.assign(bins_, 0.0f);
    hRe_.assign(numPartitions_, std::vector<float>(bins_, 0.0f));
    hIm_.assign(numPartitions_, std::vector<float>(bins_, 0.0f));
    mixRe_.assign(numPartitions_ + 1, std::vector<float>(bins_, 0.0f));
    mixIm_.assign(numPartitions_ + 1, std::vector<float>(bins_, 0.0f));
    history_.assign(flen, 0.0f);

    // Pre-scale partitions by 1/(2*flen) so the inverse FFT needs no scaling.
    const float gain = 1.0f / static_cast<float>(2 * flen);
    for (size_t p = 0; p < numPartitions_; p++) {
        std::fill(timeBuf_.begin(), timeBuf_.end(), 0.0f);
        size_t start = p * flen;
        size_t count = std::min(flen, hlen - start);
        for (size_t j = 0; j < count; j++) timeBuf_[j] = gain * h[start + j];
        fft_->forward(timeBuf_.data(), hRe_[p].data(), hIm_[p].data());
    }
}

void PartitionedConvolver::process(const float* in, float* out) {
    const size_t flen = flen_;
    // forward transform of [in, zeros]
    std::memcpy(timeBuf_.data(), in, flen * sizeof(float));
    std::memset(timeBuf_.data() + flen, 0, flen * sizeof(float));
    fft_->forward(timeBuf_.data(), inRe_.data(), inIm_.data());

    // multiply-accumulate into the frequency-domain delay line
    const size_t numMix = numPartitions_ + 1;
    const int n = static_cast<int>(bins_);
    for (size_t p = 0; p < numPartitions_; p++) {
        size_t slot = (p + mixPos_) % numMix;
        DSPSplitComplex x{inRe_.data(), inIm_.data()};
        DSPSplitComplex hh{hRe_[p].data(), hIm_[p].data()};
        DSPSplitComplex y{mixRe_[slot].data(), mixIm_[slot].data()};
        // y += x * h  (conjugate flag 1 = normal multiply)
        vDSP_zvma(&x, 1, &hh, 1, &y, 1, &y, 1, static_cast<vDSP_Length>(n));
    }

    // inverse transform of the current slot and overlap-add
    std::vector<float>& re = mixRe_[mixPos_];
    std::vector<float>& im = mixIm_[mixPos_];
    fft_->inverse(re.data(), im.data(), timeBuf_.data());
    std::fill(re.begin(), re.end(), 0.0f);
    std::fill(im.begin(), im.end(), 0.0f);
    vDSP_vadd(timeBuf_.data(), 1, history_.data(), 1, out, 1, flen);
    std::memcpy(history_.data(), timeBuf_.data() + flen, flen * sizeof(float));
    mixPos_ = (mixPos_ + 1) % numMix;
}

}  // namespace eqapo
