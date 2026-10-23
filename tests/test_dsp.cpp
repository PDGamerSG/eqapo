#include <cmath>
#include <random>
#include <vector>

#include "Test.h"
#include "TestUtil.h"
#include "engine/GainIterator.h"
#include "engine/dsp/MinimumPhase.h"
#include "engine/dsp/PartitionedConvolver.h"
#include "engine/dsp/RealFFT.h"
#include "engine/filters/ConvolutionFilter.h"
#include "engine/filters/GraphicEQFilter.h"

using namespace eqapo;

TEST(realfft_roundtrip_and_known_bins) {
    const size_t n = 64;
    RealFFT fft(n);
    std::vector<float> x(n), re(fft.bins()), im(fft.bins()), y(n);
    for (size_t i = 0; i < n; i++) x[i] = static_cast<float>(std::cos(2 * M_PI * 3 * i / n) + 0.5);
    fft.forward(x.data(), re.data(), im.data());
    CHECK_NEAR(re[0], 0.5 * n, 1e-3);   // DC
    CHECK_NEAR(re[3], 0.5 * n, 1e-3);   // cosine at bin 3 -> N/2
    CHECK_NEAR(im[3], 0.0, 1e-3);
    CHECK_NEAR(re[5], 0.0, 1e-3);
    fft.inverse(re.data(), im.data(), y.data());
    for (size_t i = 0; i < n; i++) CHECK_NEAR(y[i] / n, x[i], 1e-5);
}

namespace {
std::vector<float> directConvolve(const std::vector<float>& x, const std::vector<float>& h) {
    std::vector<float> y(x.size(), 0.0f);
    for (size_t i = 0; i < x.size(); i++) {
        double acc = 0;
        for (size_t k = 0; k < h.size() && k <= i; k++) acc += double(x[i - k]) * h[k];
        y[i] = static_cast<float>(acc);
    }
    return y;
}
}  // namespace

TEST(partitioned_convolver_matches_direct) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (size_t flen : {64u, 128u}) {
        for (size_t hlen : {1u, 63u, 128u, 1000u}) {
            std::vector<float> h(hlen), x(flen * 20);
            for (auto& v : h) v = dist(rng);
            for (auto& v : x) v = dist(rng);
            PartitionedConvolver conv;
            conv.init(h.data(), h.size(), flen);
            CHECK_EQ(conv.partitions(), (hlen + flen - 1) / flen);
            std::vector<float> y(x.size());
            for (size_t pos = 0; pos < x.size(); pos += flen) conv.process(x.data() + pos, y.data() + pos);
            std::vector<float> ref = directConvolve(x, h);
            double maxErr = 0;
            for (size_t i = 0; i < x.size(); i++) maxErr = std::max<double>(maxErr, std::fabs(y[i] - ref[i]));
            CHECK(maxErr < 2e-4);
        }
    }
}

TEST(gain_iterator_interpolates_in_log_frequency) {
    GainIterator g({{100, 0}, {1000, 10}, {10000, -5}});
    CHECK_NEAR(g.gainAt(10), 0, 1e-12);        // below range: flat at first node
    CHECK_NEAR(g.gainAt(100), 0, 1e-12);
    CHECK_NEAR(g.gainAt(1000), 10, 1e-12);
    CHECK_NEAR(g.gainAt(std::sqrt(100.0 * 1000.0)), 5, 1e-9);  // geometric midpoint = half way
    CHECK_NEAR(g.gainAt(10000), -5, 1e-12);
    CHECK_NEAR(g.gainAt(20000), -5, 1e-12);    // above range
    GainIterator empty({});
    CHECK_NEAR(empty.gainAt(440), 0, 1e-12);
}

TEST(minimum_phase_flat_magnitude_is_impulse) {
    std::vector<float> mag(1024, 1.0f);
    std::vector<float> h = minimumPhaseImpulse(mag, 1024);
    CHECK_NEAR(h[0], 1.0, 1e-4);
    double tail = 0;
    for (size_t i = 1; i < h.size(); i++) tail += std::fabs(h[i]);
    CHECK(tail < 1e-3);
}

TEST(graphiceq_response_at_nodes) {
    const double sr = 48000;
    std::vector<FilterNode> nodes = {{25, 6},    {40, 4.5},  {63, 3},    {100, 1.5}, {160, 0},
                                     {250, 0},   {400, 0},   {630, 0},   {1000, 0},  {1600, 0},
                                     {2500, 0},  {4000, 0},  {6300, 1.5}, {10000, 3}, {16000, 3}};
    GraphicEQFilter geq(nodes);
    geq.initialize(static_cast<float>(sr), 512, {"L"});
    for (const FilterNode& n : nodes) {
        double measured = testutil::measureGainDb(geq, n.freq, sr, 2.0, 512);
        CHECK_NEAR(measured, n.dbGain, 0.3);
    }
    // between 100 and 160 Hz: log interpolation
    GraphicEQFilter geq2(nodes);
    geq2.initialize(static_cast<float>(sr), 512, {"L"});
    GainIterator gi(nodes);
    CHECK_NEAR(testutil::measureGainDb(geq2, 126.5, sr, 2.0, 512), gi.gainAt(126.5), 0.3);
}

TEST(graphiceq_all_zero_is_transparent_and_handles_partial_blocks) {
    const double sr = 48000;
    std::vector<FilterNode> nodes = {{25, 0}, {16000, 0}};
    GraphicEQFilter geq(nodes);
    geq.initialize(static_cast<float>(sr), 512, {"L"});
    CHECK_NEAR(testutil::measureGainDb(geq, 1000, sr, 1.0, 512), 0.0, 0.01);
    // partial blocks (frameCount 100 with block 512): output is delayed by one block but lossless
    GraphicEQFilter geq2(nodes);
    geq2.initialize(static_cast<float>(sr), 512, {"L"});
    std::vector<float> in(4096), out(4096);
    for (size_t i = 0; i < in.size(); i++) in[i] = static_cast<float>(std::sin(0.01 * i));
    for (size_t pos = 0; pos < in.size(); pos += 100) {
        unsigned n = static_cast<unsigned>(std::min<size_t>(100, in.size() - pos));
        float* ip = in.data() + pos;
        float* op = out.data() + pos;
        geq2.process(&op, &ip, n);
    }
    for (size_t i = 512; i < in.size(); i++) CHECK_NEAR(out[i], in[i - 512], 2e-3);
}

namespace {
ImpulseResponse fakeLoader(const std::string& path, std::string& error) {
    ImpulseResponse ir;
    if (path == "missing") {
        error = "not found";
        return ir;
    }
    ir.sampleRate = path == "wrongrate" ? 44100 : 48000;
    ir.channels = {{0.0f, 0.5f}, {0.0f, 0.0f, 0.25f}};  // L: half amplitude delayed 1; R: quarter delayed 2
    return ir;
}
}  // namespace

TEST(convolution_filter_round_robin_and_errors) {
    setImpulseLoader(fakeLoader);
    ConvolutionFilter conv("ir.wav");
    conv.initialize(48000, 64, {"L", "R", "C"});
    CHECK(conv.active());
    std::vector<float> a(64, 0.0f), b(64, 0.0f), c(64, 0.0f), oa(64), ob(64), oc(64);
    a[0] = b[0] = c[0] = 1.0f;
    float* in[3] = {a.data(), b.data(), c.data()};
    float* out[3] = {oa.data(), ob.data(), oc.data()};
    conv.process(out, in, 64);
    CHECK_NEAR(oa[1], 0.5, 1e-5);
    CHECK_NEAR(ob[2], 0.25, 1e-5);
    CHECK_NEAR(oc[1], 0.5, 1e-5);  // third channel gets first file channel again
    ConvolutionFilter bad("wrongrate");
    bad.initialize(48000, 64, {"L"});
    CHECK(!bad.active());
    ConvolutionFilter missing("missing");
    missing.initialize(48000, 64, {"L"});
    CHECK(!missing.active());
    setImpulseLoader(nullptr);
}
