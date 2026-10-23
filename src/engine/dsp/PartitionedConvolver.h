#pragma once
#include <memory>
#include <vector>

#include "engine/dsp/RealFFT.h"

namespace eqapo {

// Uniformly partitioned frequency-domain convolution (overlap-add with a
// frequency-domain delay line). Partition length = block length `flen`;
// FFT size = 2*flen. Zero latency beyond the block itself.
class PartitionedConvolver {
public:
    PartitionedConvolver() = default;
    // h: impulse response of hlen samples, flen: block length (power of two).
    void init(const float* h, size_t hlen, size_t flen);
    // Processes exactly flen samples: out = in * h (continuing from previous blocks).
    void process(const float* in, float* out);
    size_t blockLength() const { return flen_; }
    size_t partitions() const { return numPartitions_; }
    bool initialised() const { return fft_ != nullptr; }

private:
    size_t flen_ = 0;
    size_t bins_ = 0;
    size_t numPartitions_ = 0;
    size_t mixPos_ = 0;
    std::unique_ptr<RealFFT> fft_;
    std::vector<float> timeBuf_;      // 2*flen
    std::vector<float> inRe_, inIm_;  // bins
    std::vector<std::vector<float>> hRe_, hIm_;      // numPartitions x bins
    std::vector<std::vector<float>> mixRe_, mixIm_;  // (numPartitions+1) x bins
    std::vector<float> history_;                     // flen
};

}  // namespace eqapo
