#include "Test.h"
#include "TestUtil.h"
#include "engine/filters/BiQuad.h"
#include "engine/filters/BiQuadFilter.h"

using namespace eqapo;

namespace {
BiQuadFilter makeMono(BiQuad::Type t, double gain, double fc, double q, bool isBwOrS, bool corner, double sr) {
    BiQuadFilter f(t, gain, fc, q, isBwOrS, corner);
    f.initialize(static_cast<float>(sr), 512, {"L"});
    return f;
}
}  // namespace

TEST(biquad_measured_gain_matches_gainAt_all_types) {
    const double sr = 48000;
    struct Case {
        BiQuad::Type type;
        double gain, fc, q;
        bool isBwOrS;
    } cases[] = {
        {BiQuad::PEAKING, 6.0, 1000, 1.0, false},
        {BiQuad::PEAKING, -10.0, 50, 2.5, false},
        {BiQuad::PEAKING, 3.0, 100, 0.167, true},  // BW Oct
        {BiQuad::LOW_PASS, 0, 8000, M_SQRT1_2, false},
        {BiQuad::HIGH_PASS, 0, 30, 0.5, false},
        {BiQuad::BAND_PASS, 0, 1000, 0.1, false},
        {BiQuad::NOTCH, 0, 800, 30.0, false},
        {BiQuad::ALL_PASS, 0, 900, 0.707, false},
        {BiQuad::LOW_SHELF, 5.0, 300, 0.9, true},
        {BiQuad::HIGH_SHELF, -3.0, 1000, 0.9, true},
        {BiQuad::LOW_SHELF, 5.0, 300, 0.6473, false},
        {BiQuad::HIGH_SHELF, -6.0, 100, 0.4272, false},
    };
    const double probes[] = {40, 100, 300, 1000, 3000, 10000};
    for (const Case& c : cases) {
        BiQuad bq(c.type, c.gain, c.fc, sr, c.q, c.isBwOrS);
        for (double f : probes) {
            BiQuadFilter filt = makeMono(c.type, c.gain, c.fc, c.q, c.isBwOrS, false, sr);
            double measured = testutil::measureGainDb(filt, f, sr);
            double expected = bq.gainAt(f, sr);
            if (expected < -60) continue;  // deep notch: measurement noise floor
            CHECK_NEAR(measured, expected, 0.05);
        }
    }
}

TEST(biquad_known_values) {
    const double sr = 48000;
    BiQuad pk(BiQuad::PEAKING, 6.0, 1000, sr, 1.0, false);
    CHECK_NEAR(pk.gainAt(1000, sr), 6.0, 1e-6);
    BiQuad lp(BiQuad::LOW_PASS, 0, 1000, sr, M_SQRT1_2, false);
    CHECK_NEAR(lp.gainAt(1000, sr), -3.0103, 1e-3);
    CHECK_NEAR(lp.gainAt(10, sr), 0.0, 1e-3);
    BiQuad hp(BiQuad::HIGH_PASS, 0, 1000, sr, M_SQRT1_2, false);
    CHECK_NEAR(hp.gainAt(1000, sr), -3.0103, 1e-3);
    BiQuad no(BiQuad::NOTCH, 0, 1000, sr, 30.0, false);
    CHECK(no.gainAt(1000, sr) < -80);
    CHECK_NEAR(no.gainAt(100, sr), 0.0, 0.01);
    BiQuad ap(BiQuad::ALL_PASS, 0, 1000, sr, 0.707, false);
    CHECK_NEAR(ap.gainAt(500, sr), 0.0, 1e-9);
    BiQuad ls(BiQuad::LOW_SHELF, 5.0, 300, sr, 0.9, true);
    CHECK_NEAR(ls.gainAt(10, sr), 5.0, 1e-3);
    CHECK_NEAR(ls.gainAt(20000, sr), 0.0, 0.01);
    CHECK_NEAR(ls.gainAt(300, sr), 2.5, 0.05);  // shelf midpoint at centre frequency
    BiQuad hs(BiQuad::HIGH_SHELF, -3.0, 1000, sr, 0.9, true);
    CHECK_NEAR(hs.gainAt(20000, sr), -3.0, 0.02);
    CHECK_NEAR(hs.gainAt(10, sr), 0.0, 1e-3);
    // explicit coefficient check from the Equalizer APO wiki example: LP Fc 3000 Hz @ 48 kHz
    BiQuad lp3k(BiQuad::LOW_PASS, 0, 3000, sr, M_SQRT1_2, false);
    CHECK_NEAR(lp3k.b0() * 1.2706, 0.0380602, 1e-6);
    CHECK_NEAR(lp3k.b1() * 1.2706, 0.0761205, 1e-6);
    CHECK_NEAR(lp3k.a1() * 1.2706, -1.84776, 1e-5);
    CHECK_NEAR(lp3k.a2() * 1.2706, 0.729402, 2e-6);  // wiki value is rounded
}

TEST(biquadfilter_corner_frequency_shelf) {
    const double sr = 48000;
    // LS 12dB Fc 2000 Hz Gain -5 dB: S = 12/12 = 1, corner frequency form
    BiQuadFilter ls = makeMono(BiQuad::LOW_SHELF, -5.0, 2000, 1.0, true, true, sr);
    double factor = std::pow(10.0, 5.0 / 80.0 / 1.0);
    CHECK_NEAR(ls.effectiveFreq(), 2000 * factor, 1e-9);
    // HS with Q given (HS Fc 100 Gain -6 Q 0.4272): S derived from Q
    BiQuadFilter hs = makeMono(BiQuad::HIGH_SHELF, -6.0, 100, 0.4272, false, true, sr);
    double a = std::pow(10.0, -6.0 / 40.0);
    double s = 1.0 / ((1.0 / (0.4272 * 0.4272) - 2.0) / (a + 1.0 / a) + 1.0);
    CHECK_NEAR(hs.effectiveFreq(), 100 / std::pow(10.0, 6.0 / 80.0 / s), 1e-9);
    // LSC (centre frequency form) is not shifted
    BiQuadFilter lsc = makeMono(BiQuad::LOW_SHELF, -5.0, 2000, 1.0, true, false, sr);
    CHECK_NEAR(lsc.effectiveFreq(), 2000, 1e-9);
    // response of the shifted filter equals a BiQuad built at the shifted frequency
    BiQuad ref(BiQuad::LOW_SHELF, -5.0, 2000 * factor, sr, 1.0, true);
    CHECK_NEAR(testutil::measureGainDb(ls, 500, sr), ref.gainAt(500, sr), 0.05);
}

TEST(biquadfilter_stereo_independent_state) {
    BiQuadFilter f(BiQuad::PEAKING, 6.0, 1000, 1.0, false, false);
    f.initialize(48000, 64, {"L", "R"});
    std::vector<float> l(64, 0.0f), r(64, 0.0f), ol(64), orr(64);
    l[0] = 1.0f;  // impulse on L only
    float* in[2] = {l.data(), r.data()};
    float* out[2] = {ol.data(), orr.data()};
    f.process(out, in, 64);
    CHECK(std::fabs(ol[0]) > 0.5f);
    for (float v : orr) CHECK(v == 0.0f);
}
