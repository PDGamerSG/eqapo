#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "Test.h"
#include "TestUtil.h"
#include "engine/FilterEngine.h"
#include "engine/factories/Factories.h"

using namespace eqapo;

namespace {
std::string tempDir() {
    static std::string dir;
    if (dir.empty()) {
        char buf[] = "/tmp/eqapo_test_XXXXXX";
        dir = mkdtemp(buf);
    }
    return dir;
}
std::string writeFile(const std::string& name, const std::string& content) {
    std::string path = tempDir() + "/" + name;
    std::ofstream(path, std::ios::binary) << content;
    return path;
}
// Runs `frames` frames of planar audio through the engine in blocks.
void run(FilterEngine& engine, std::vector<std::vector<float>>& in, std::vector<std::vector<float>>& out, unsigned block) {
    size_t total = in[0].size();
    std::vector<float*> ip(in.size()), op(out.size());
    for (size_t pos = 0; pos < total; pos += block) {
        unsigned n = static_cast<unsigned>(std::min<size_t>(block, total - pos));
        for (size_t c = 0; c < in.size(); c++) ip[c] = in[c].data() + pos;
        for (size_t c = 0; c < out.size(); c++) op[c] = out[c].data() + pos;
        engine.process(op.data(), ip.data(), n);
    }
}
}  // namespace

TEST(factory_getFreq_thousands_rule) {
    CHECK_NEAR(BiQuadFilterFactory::getFreq("1.000"), 1000, 1e-9);     // REW thousands separator
    CHECK_NEAR(BiQuadFilterFactory::getFreq("11.300"), 11300, 1e-9);
    CHECK_NEAR(BiQuadFilterFactory::getFreq("22.0"), 22.0, 1e-9);
    CHECK_NEAR(BiQuadFilterFactory::getFreq("1000.0"), 1000.0, 1e-9);  // '.' not four from the end
    CHECK_NEAR(BiQuadFilterFactory::getFreq("1.5e3"), 1500, 1e-9);     // exponent: no thousands rule
    CHECK_NEAR(BiQuadFilterFactory::getFreq("1\xC2\xA0" "000"), 1000, 1e-9);  // NBSP separator
    CHECK_NEAR(BiQuadFilterFactory::getFreq("abc"), -1.0, 1e-9);
}

TEST(device_matching) {
    std::string dev = "NIRVANAA 751ANC 14-9F-3C-11-22-33:output";
    CHECK(DeviceFilterFactory::matchDevice(dev, "nirvanaa"));
    CHECK(DeviceFilterFactory::matchDevice(dev, "NIRVANAA 751ANC"));
    CHECK(DeviceFilterFactory::matchDevice(dev, " Speakers; nirvanaa output "));
    CHECK(!DeviceFilterFactory::matchDevice(dev, "Speakers"));
    CHECK(!DeviceFilterFactory::matchDevice(dev, "NIRVANAA Speakers"));
    CHECK(DeviceFilterFactory::matchDevice(dev, "all"));
    CHECK(DeviceFilterFactory::matchDevice(dev, "Speakers; ALL"));
}

TEST(engine_parses_pc_equalizer_preset) {
    std::string cfg =
        "Filter Settings file\r\n\r\nDate: 2017-08-16 21:42:46\r\n\r\nNote: [build] Bass\r\n\r\nEqualiser: FBQ2496\r\n"
        "Preamp:    0.0 dB\r\n"
        "Filter  1: ON  PEQ      Fc   22.0 Hz  Gain   0.00 dB  BW Oct 02.600\r\n"
        "Filter  2: ON  PEQ      Fc   31.0 Hz  Gain   5.50 dB  BW Oct 02.600\r\n"
        "Filter 12: ON  PEQ      Fc  1,000 Hz  Gain  -2.50 dB  BW Oct 02.600\r\n"
        "Filter 19: ON  PEQ      Fc 11,300 Hz  Gain   0.00 dB  BW Oct 02.600\r\n"
        "Filter 15: ON  None\r\n"
        "Filter 16: OFF PK Fc 100 Hz Gain 3 dB Q 1\r\n"
        "#Include: pce_delay.txt\r\n"
        "Include: pce_chnl.txt\r\n";
    std::string path = writeFile("bass.txt", "\xEF\xBB\xBF" + cfg);
    FilterEngine engine;
    engine.initialize(48000, 2, 2, 512);
    CHECK(engine.loadConfig(path));
    std::vector<std::string> desc = engine.describeLoaded();
    CHECK_EQ(desc.size(), size_t{5});
    if (desc.size() == 5) {
        CHECK_EQ(desc[0], std::string("Preamp 0 dB"));
        CHECK_EQ(desc[1], std::string("PK Fc 22 Hz Gain 0 dB BW 2.6"));
        CHECK_EQ(desc[2], std::string("PK Fc 31 Hz Gain 5.5 dB BW 2.6"));
        CHECK_EQ(desc[3], std::string("PK Fc 1000 Hz Gain -2.5 dB BW 2.6"));
        CHECK_EQ(desc[4], std::string("PK Fc 11300 Hz Gain 0 dB BW 2.6"));
    }
    CHECK_EQ(engine.lastLoadErrors().size(), size_t{1});  // missing pce_chnl.txt is reported, not fatal
}

TEST(engine_filter_parsing_variants) {
    std::string cfg =
        "Filter: ON LS 6dB Fc 50,0 Hz Gain 7,2 dB\n"
        "Filter: ON LSC 10.8 dB Fc 300 Hz Gain 5.0 dB\n"
        "Filter: ON HSC Fc 100 Hz Gain -6.0 dB Q 0.4272\n"
        "Filter: ON NO Fc 800 Hz\n"
        "Filter: ON LP Fc 8.000 Hz\n"
        "Filter: ON PK Fc 100 Hz Gain 3 dB\n"          // missing Q -> ignored
        "Filter: ON AP Fc 900 Hz Q 0.707\n"
        "Filter: ON XX Fc 900 Hz\n"                    // invalid type -> ignored
        "Filter: ON IIR Order 2 Coefficients 0.0380602 0.0761205 0.0380602 1.2706 -1.84776 0.729402\n"
        "Filter: ON IIR Order 2 Coefficients 1 2 3\n"  // wrong count -> ignored
        "  Filter: ON HP Fc 30 Hz\n";                  // indentation allowed
    std::string path = writeFile("variants.txt", cfg);
    FilterEngine engine;
    engine.initialize(48000, 2, 2, 512);
    engine.loadConfig(path);
    std::vector<std::string> d = engine.describeLoaded();
    CHECK_EQ(d.size(), size_t{8});
    if (d.size() == 8) {
        CHECK_EQ(d[0], std::string("LS Fc 50 Hz Gain 7.2 dB S 0.5"));
        CHECK_EQ(d[1], std::string("LS Fc 300 Hz Gain 5 dB S 0.9"));
        CHECK_EQ(d[2], std::string("HS Fc 100 Hz Gain -6 dB Q 0.4272"));
        CHECK_EQ(d[3], std::string("NO Fc 800 Hz Gain 0 dB Q 30"));
        CHECK_EQ(d[4], std::string("LP Fc 8000 Hz Gain 0 dB Q 0.707107"));
        CHECK_EQ(d[5], std::string("AP Fc 900 Hz Gain 0 dB Q 0.707"));
        CHECK_EQ(d[6], std::string("IIR order 2"));
        CHECK_EQ(d[7], std::string("HP Fc 30 Hz Gain 0 dB Q 0.707107"));
    }
}

TEST(engine_channel_selection_and_preamp) {
    std::string path = writeFile("chan.txt", "Preamp: -6 dB\nChannel: L\nPreamp: -6 dB\nChannel: 2\nPreamp: +6 dB\n");
    FilterEngine engine;
    engine.initialize(48000, 2, 2, 64);
    engine.loadConfig(path);
    std::vector<std::vector<float>> in(2, std::vector<float>(128, 1.0f)), out(2, std::vector<float>(128));
    run(engine, in, out, 64);
    CHECK_NEAR(out[0][100], std::pow(10.0, -12.0 / 20.0), 1e-5);  // L: -6 -6
    CHECK_NEAR(out[1][100], 1.0, 1e-5);                           // R: -6 +6
}

TEST(engine_selective_delay_example) {
    // Equalizer APO's selective_delay.txt (shortened delay)
    std::string cfg =
        "Copy: L2=L R2=R\n"
        "Channel: L2 R2\n"
        "Delay: 10 samples\n"
        "Copy: L=0.5*L+0.5*L2 R=0.5*R+0.5*R2\n";
    std::string path = writeFile("seldelay.txt", cfg);
    FilterEngine engine;
    engine.initialize(48000, 2, 2, 32);
    engine.loadConfig(path);
    CHECK_EQ(engine.allChannelNames(), (Channels{"L", "R", "L2", "R2"}));
    std::vector<std::vector<float>> in(2, std::vector<float>(96, 0.0f)), out(2, std::vector<float>(96));
    for (size_t i = 0; i < 96; i++) {
        in[0][i] = static_cast<float>(i + 1);
        in[1][i] = static_cast<float>(-(int)(i + 1));
    }
    run(engine, in, out, 32);
    for (size_t i = 0; i < 96; i++) {
        float delayed = i < 10 ? 0.0f : in[0][i - 10];
        CHECK_NEAR(out[0][i], 0.5f * in[0][i] + 0.5f * delayed, 1e-5);
        float delayedR = i < 10 ? 0.0f : in[1][i - 10];
        CHECK_NEAR(out[1][i], 0.5f * in[1][i] + 0.5f * delayedR, 1e-5);
    }
}

TEST(engine_include_relative_and_channel_restore) {
    writeFile("inner.txt", "Channel: R\nPreamp: -20 dB\n");
    std::string path = writeFile("outer.txt", "Include: inner.txt\nPreamp: -6 dB\n");
    FilterEngine engine;
    engine.initialize(48000, 2, 2, 64);
    engine.loadConfig(path);
    std::vector<std::vector<float>> in(2, std::vector<float>(64, 1.0f)), out(2, std::vector<float>(64));
    run(engine, in, out, 64);
    // after the include, the selection is restored to all channels, so -6 dB applies to both
    CHECK_NEAR(out[0][10], std::pow(10.0, -6.0 / 20.0), 1e-5);
    CHECK_NEAR(out[1][10], std::pow(10.0, -26.0 / 20.0), 1e-5);
    // recursion is bounded
    std::string loop = writeFile("loop.txt", "Include: loop.txt\nPreamp: -1 dB\n");
    FilterEngine engine2;
    engine2.initialize(48000, 2, 2, 64);
    engine2.loadConfig(loop);
    CHECK_EQ(engine2.loadedFilterCount(), size_t{101});
}

TEST(engine_device_and_stage_gating) {
    std::string cfg =
        "Device: Speakers\n"
        "Preamp: -10 dB\n"
        "Device: Headset; Benchmark\n"
        "Preamp: -1 dB\n"
        "Device: all\n"
        "Stage: pre-mix\n"
        "Preamp: -100 dB\n"
        "Stage: pre-mix post-mix\n"
        "Preamp: -2 dB\n";
    std::string path = writeFile("gate.txt", cfg);
    FilterEngine engine;
    engine.setDeviceInfo("USB Headset", "AppleUSBAudioEngine:1234");
    engine.initialize(48000, 2, 2, 64);
    engine.loadConfig(path);
    std::vector<std::string> d = engine.describeLoaded();
    CHECK_EQ(d, (std::vector<std::string>{"Preamp -1 dB", "Preamp -2 dB"}));
}

TEST(engine_graphiceq_and_convolution_lines) {
    std::string cfg = "GraphicEQ: 1000 0; 25 6; 16000 -3\nConvolution: missing.wav\n";
    std::string path = writeFile("geq.txt", cfg);
    FilterEngine engine;
    engine.initialize(48000, 2, 2, 512);
    engine.loadConfig(path);
    std::vector<std::string> d = engine.describeLoaded();
    CHECK_EQ(d.size(), size_t{2});
    if (d.size() == 2) {
        CHECK_EQ(d[0], std::string("GraphicEQ 3 nodes"));
        CHECK_EQ(d[1], std::string("Convolution " + tempDir() + "/missing.wav (inactive)"));
    }
}

TEST(engine_reload_crossfades_without_discontinuity) {
    const double sr = 48000;
    std::string a = writeFile("a.txt", "Preamp: 0 dB\n");
    std::string b = writeFile("b.txt", "Preamp: -200 dB\n");
    FilterEngine engine;
    engine.initialize(static_cast<float>(sr), 1, 1, 64);
    engine.setAudioRunning(true);
    engine.loadConfig(a);
    CHECK(!engine.transitionPending());
    std::vector<std::vector<float>> in(1, std::vector<float>(64, 1.0f)), out(1, std::vector<float>(64));
    run(engine, in, out, 64);
    CHECK_NEAR(out[0][63], 1.0, 1e-6);

    engine.loadConfig(b);
    CHECK(engine.transitionPending());
    // transition is 10 ms = 480 frames; run 512 frames in blocks of 64 and check monotonic fade to 0
    std::vector<std::vector<float>> in2(1, std::vector<float>(512, 1.0f)), out2(1, std::vector<float>(512));
    run(engine, in2, out2, 64);
    bool monotonic = true;
    for (size_t i = 1; i < 480; i++)
        if (out2[0][i] > out2[0][i - 1] + 1e-6f) monotonic = false;
    CHECK(monotonic);
    CHECK_NEAR(out2[0][0], 1.0, 1e-6);
    CHECK_NEAR(out2[0][240], 0.5, 0.01);  // half way through the raised-cosine fade
    CHECK_NEAR(out2[0][500], 0.0, 1e-6);
    CHECK(!engine.transitionPending());
    engine.collectGarbage();

    // when audio is not running, loads swap immediately
    engine.setAudioRunning(false);
    engine.loadConfig(a);
    CHECK(!engine.transitionPending());
    run(engine, in, out, 64);
    CHECK_NEAR(out[0][0], 1.0, 1e-6);
}

TEST(engine_missing_config_is_passthrough) {
    FilterEngine engine;
    engine.initialize(48000, 2, 2, 64);
    CHECK(!engine.loadConfig(tempDir() + "/does_not_exist.txt"));
    std::vector<std::vector<float>> in(2, std::vector<float>(64, 0.25f)), out(2, std::vector<float>(64));
    run(engine, in, out, 64);
    CHECK_NEAR(out[1][5], 0.25, 1e-9);
    // larger frame counts than maxFrameCount are chunked
    std::string path = writeFile("p.txt", "Preamp: -6 dB\n");
    engine.loadConfig(path);
    std::vector<std::vector<float>> in2(2, std::vector<float>(200, 1.0f)), out2(2, std::vector<float>(200));
    run(engine, in2, out2, 200);
    CHECK_NEAR(out2[0][199], std::pow(10.0, -6.0 / 20.0), 1e-5);
}
