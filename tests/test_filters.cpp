#include <cmath>
#include <vector>

#include "Test.h"
#include "TestUtil.h"
#include "engine/filters/BiQuad.h"
#include "engine/filters/BiQuadFilter.h"
#include "engine/filters/ChannelFilter.h"
#include "engine/filters/CopyFilter.h"
#include "engine/filters/DelayFilter.h"
#include "engine/filters/IIRFilter.h"
#include "engine/filters/PreampFilter.h"

using namespace eqapo;

TEST(preamp_scales_amplitude) {
    PreampFilter p(-6.0);
    p.initialize(48000, 16, {"L", "R"});
    std::vector<float> l(16, 1.0f), r(16, -1.0f);
    float* io[2] = {l.data(), r.data()};
    p.process(io, io, 16);
    CHECK_NEAR(l[3], 0.501187, 1e-5);
    CHECK_NEAR(r[3], -0.501187, 1e-5);
}

namespace {
// Runs a delay filter with the given frame sizes and checks output == input shifted.
void checkDelay(unsigned delaySamples, const std::vector<unsigned>& frameSizes) {
    DelayFilter d(static_cast<double>(delaySamples), false);
    d.initialize(48000, 64, {"L"});
    CHECK_EQ(d.delaySamples(), delaySamples);
    unsigned total = 0;
    for (unsigned n : frameSizes) total += n;
    std::vector<float> in(total), out(total);
    for (unsigned i = 0; i < total; i++) in[i] = static_cast<float>(i + 1);
    unsigned pos = 0;
    for (unsigned n : frameSizes) {
        float* ip = in.data() + pos;
        float* op = out.data() + pos;
        d.process(&op, &ip, n);
        pos += n;
    }
    for (unsigned i = 0; i < total; i++) {
        float expected = i < delaySamples ? 0.0f : in[i - delaySamples];
        if (out[i] != expected) {
            CHECK_EQ(out[i], expected);
            return;
        }
    }
}
}  // namespace

TEST(delay_sample_accurate_across_blocks) {
    checkDelay(3, {2, 5, 7, 1, 4});       // delay shorter than and longer than blocks
    checkDelay(10, {4, 4, 4, 4, 4, 4});  // delay longer than block, wrap-around
    checkDelay(5, {5, 5, 5});            // delay == block
    checkDelay(0, {3, 8});               // zero delay passes through
    checkDelay(7, {64, 64});             // delay shorter than block
}

TEST(delay_milliseconds_rounding) {
    DelayFilter d(10.0, true);
    d.initialize(48000, 64, {"L"});
    CHECK_EQ(d.delaySamples(), 480u);
    DelayFilter d2(0.5, true);
    d2.initialize(44100, 64, {"L"});
    CHECK_EQ(d2.delaySamples(), 22u);  // 22.05 -> 22
}

TEST(copy_creates_channels_and_sums) {
    // Copy: L=0.5*R+-6dB*L C=0.25 X=R
    std::vector<CopyAssignment> as(3);
    as[0].targetChannel = "L";
    as[0].sourceSum = {{0.5, false, "R"}, {-6.0, true, "L"}};
    as[1].targetChannel = "C";
    as[1].sourceSum = {{0.25, false, ""}};
    as[2].targetChannel = "X";
    as[2].sourceSum = {{1.0, false, "R"}};
    CopyFilter copy(as);
    Channels out = copy.initialize(48000, 8, {"L", "R"});
    CHECK_EQ(out, (Channels{"L", "C", "X"}));

    std::vector<float> l(8, 1.0f), r(8, 2.0f), oL(8), oC(8), oX(8);
    float* in[2] = {l.data(), r.data()};
    float* outp[3] = {oL.data(), oC.data(), oX.data()};
    copy.process(outp, in, 8);
    CHECK_NEAR(oL[5], 0.5 * 2.0 + std::pow(10.0, -6.0 / 20.0) * 1.0, 1e-6);
    CHECK_NEAR(oC[5], 0.25, 1e-9);
    CHECK_NEAR(oX[5], 2.0, 1e-9);
}

TEST(copy_numeric_target_resolves_to_existing_name) {
    std::vector<CopyAssignment> as(1);
    as[0].targetChannel = "1";
    as[0].sourceSum = {{1.0, false, "R"}};
    CopyFilter copy(as);
    Channels out = copy.initialize(48000, 8, {"L", "R"});
    CHECK_EQ(out, (Channels{"L"}));
}

TEST(channel_filter_selection) {
    ChannelFilter cf({"R", "1"});
    Channels sel = cf.initialize(48000, 8, {"L", "R", "C"});
    CHECK_EQ(sel, (Channels{"L", "R"}));
    ChannelFilter all({"ALL"});
    CHECK_EQ(all.initialize(48000, 8, {"L", "R", "C"}), (Channels{"L", "R", "C"}));
    ChannelFilter none({"Q"});
    CHECK_EQ(none.initialize(48000, 8, {"L", "R"}), (Channels{}));
    CHECK(cf.getSelectChannels());
}

TEST(iir_order2_matches_biquad) {
    const double sr = 48000;
    BiQuad ref(BiQuad::LOW_PASS, 0, 3000, sr, M_SQRT1_2, false);
    std::vector<double> coeffs = {ref.b0(), ref.b1(), ref.b2(), 1.0, ref.a1(), ref.a2()};
    IIRFilter iir(coeffs);
    iir.initialize(static_cast<float>(sr), 64, {"L"});
    BiQuadFilter bqf(BiQuad::LOW_PASS, 0, 3000, M_SQRT1_2, false, false);
    bqf.initialize(static_cast<float>(sr), 64, {"L"});
    std::vector<float> in(256), o1(256), o2(256);
    for (size_t i = 0; i < in.size(); i++) in[i] = static_cast<float>(std::sin(0.3 * i) + 0.5 * std::cos(1.7 * i));
    for (size_t pos = 0; pos < in.size(); pos += 64) {
        float* ip = in.data() + pos;
        float* a = o1.data() + pos;
        float* b = o2.data() + pos;
        iir.process(&a, &ip, 64);
        bqf.process(&b, &ip, 64);
    }
    for (size_t i = 0; i < in.size(); i++) CHECK_NEAR(o1[i], o2[i], 1e-6);
}

TEST(iir_order1_and_order3) {
    // order 1: y = 0.5 x + 0.5 x[-1] (moving average), a0 = 1
    IIRFilter ma({0.5, 0.5, 1.0, 0.0});
    ma.initialize(48000, 8, {"L"});
    std::vector<float> in = {1, 0, 0, 0, 2, 2, 2, 2}, out(8);
    float* ip = in.data();
    float* op = out.data();
    ma.process(&op, &ip, 8);
    CHECK_NEAR(out[0], 0.5, 1e-9);
    CHECK_NEAR(out[1], 0.5, 1e-9);
    CHECK_NEAR(out[2], 0.0, 1e-9);
    CHECK_NEAR(out[5], 2.0, 1e-9);
    // order 3 with a0 = 2 (normalisation): pure delay of 3 samples times 2 => y = x[-3]
    IIRFilter dl({0.0, 0.0, 0.0, 2.0, 2.0, 0.0, 0.0, 0.0});
    dl.initialize(48000, 8, {"L"});
    std::vector<float> in2 = {1, 2, 3, 4, 5, 6, 7, 8}, out2(8);
    ip = in2.data();
    op = out2.data();
    dl.process(&op, &ip, 8);
    CHECK_NEAR(out2[3], 1.0, 1e-9);
    CHECK_NEAR(out2[7], 5.0, 1e-9);
}
