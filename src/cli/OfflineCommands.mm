// Offline commands: process, bench, gen, analyze.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

#include "audio/AudioFile.h"
#include "cli/Commands.h"
#include "engine/FilterEngine.h"
#include "engine/Log.h"
#include "engine/filters/ConvolutionFilter.h"
#include "daemon/Paths.h"

namespace eqapo::cli {

std::string optionValue(const Args& args, const std::string& name, const std::string& fallback) {
    for (size_t i = 0; i + 1 < args.size(); i++)
        if (args[i] == name) return args[i + 1];
    return fallback;
}

bool hasFlag(const Args& args, const std::string& name) {
    for (const std::string& a : args)
        if (a == name) return true;
    return false;
}

std::vector<std::string> positional(const Args& args) {
    std::vector<std::string> out;
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i].size() > 2 && args[i][0] == '-' && args[i][1] == '-') {
            // options that take a value
            static const char* valued[] = {"--config", "--rate", "--frames", "--channels", "--seconds", "--tone",
                                           "--delay", "--buffer", "--device", "--out"};
            bool takesValue = false;
            for (const char* v : valued)
                if (args[i] == v) takesValue = true;
            if (takesValue) i++;
            continue;
        }
        out.push_back(args[i]);
    }
    return out;
}

namespace {
ImpulseResponse loadImpulse(const std::string& path, std::string& error) {
    AudioData data = readAudioFile(path, error);
    ImpulseResponse ir;
    ir.sampleRate = data.sampleRate;
    ir.channels = std::move(data.channels);
    return ir;
}
}  // namespace

void installImpulseLoader() { setImpulseLoader(&loadImpulse); }

int cmdProcess(const Args& args) {
    std::vector<std::string> pos = positional(args);
    if (pos.size() < 2) {
        std::fprintf(stderr, "usage: eqapo process <in> <out.wav> [--config FILE] [--frames N] [--device NAME]\n");
        return 2;
    }
    std::string configPath = optionValue(args, "--config", Paths::configFile());
    unsigned frames = static_cast<unsigned>(std::strtoul(optionValue(args, "--frames", "512").c_str(), nullptr, 10));
    std::string error;
    AudioData in = readAudioFile(pos[0], error);
    if (in.channels.empty()) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    installImpulseLoader();
    FilterEngine engine;
    engine.setDeviceInfo(optionValue(args, "--device", "Benchmark"), "File output");
    engine.initialize(static_cast<float>(in.sampleRate), static_cast<unsigned>(in.channels.size()),
                      static_cast<unsigned>(in.channels.size()), frames);
    if (!engine.loadConfig(configPath)) std::fprintf(stderr, "warning: could not open %s\n", configPath.c_str());
    for (const std::string& line : engine.describeLoaded()) std::fprintf(stderr, "  %s\n", line.c_str());

    AudioData out;
    out.sampleRate = in.sampleRate;
    out.channels.assign(in.channels.size(), std::vector<float>(in.frames()));
    std::vector<float*> ip(in.channels.size()), op(in.channels.size());
    for (size_t p = 0; p < in.frames(); p += frames) {
        unsigned n = static_cast<unsigned>(std::min<size_t>(frames, in.frames() - p));
        for (size_t c = 0; c < in.channels.size(); c++) {
            ip[c] = in.channels[c].data() + p;
            op[c] = out.channels[c].data() + p;
        }
        engine.process(op.data(), ip.data(), n);
    }
    if (!writeWavFloat(pos[1], out, error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    std::printf("processed %zu frames x %zu channels at %g Hz with %zu filters -> %s\n", in.frames(), in.channels.size(),
                in.sampleRate, engine.loadedFilterCount(), pos[1].c_str());
    return 0;
}

int cmdBench(const Args& args) {
    std::string configPath = optionValue(args, "--config", Paths::configFile());
    double rate = std::strtod(optionValue(args, "--rate", "48000").c_str(), nullptr);
    unsigned frames = static_cast<unsigned>(std::strtoul(optionValue(args, "--frames", "512").c_str(), nullptr, 10));
    unsigned channels = static_cast<unsigned>(std::strtoul(optionValue(args, "--channels", "2").c_str(), nullptr, 10));
    double seconds = std::strtod(optionValue(args, "--seconds", "20").c_str(), nullptr);
    installImpulseLoader();
    FilterEngine engine;
    engine.setDeviceInfo("Benchmark", "File output");
    engine.initialize(static_cast<float>(rate), channels, channels, frames);
    if (!engine.loadConfig(configPath)) {
        std::fprintf(stderr, "error: could not open %s\n", configPath.c_str());
        return 1;
    }
    std::printf("config: %s (%zu filters, loaded in %.2f ms)\n", configPath.c_str(), engine.loadedFilterCount(),
                engine.lastLoadMilliseconds());
    for (const std::string& line : engine.describeLoaded()) std::printf("  %s\n", line.c_str());

    std::mt19937 rng(1);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    std::vector<std::vector<float>> in(channels, std::vector<float>(frames)), out(channels, std::vector<float>(frames));
    for (auto& ch : in)
        for (auto& v : ch) v = dist(rng);
    std::vector<float*> ip(channels), op(channels);
    for (unsigned c = 0; c < channels; c++) {
        ip[c] = in[c].data();
        op[c] = out[c].data();
    }
    size_t blocks = static_cast<size_t>(seconds * rate / frames);
    for (size_t i = 0; i < 50; i++) engine.process(op.data(), ip.data(), frames);  // warm-up
    auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < blocks; i++) engine.process(op.data(), ip.data(), frames);
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    double perBlockUs = elapsed / blocks * 1e6;
    double blockDurationUs = frames / rate * 1e6;
    std::printf("%zu blocks of %u frames x %u ch at %g Hz: %.2f us/block (block = %.0f us) => %.3f%% of one core\n", blocks,
                frames, channels, rate, perBlockUs, blockDurationUs, 100.0 * perBlockUs / blockDurationUs);
    return 0;
}

int cmdGen(const Args& args) {
    std::vector<std::string> pos = positional(args);
    if (pos.empty()) {
        std::fprintf(stderr, "usage: eqapo gen <out.wav> [--rate HZ] [--seconds S] [--channels N] [--tone HZ:AMP ...]\n");
        return 2;
    }
    double rate = std::strtod(optionValue(args, "--rate", "48000").c_str(), nullptr);
    double seconds = std::strtod(optionValue(args, "--seconds", "2").c_str(), nullptr);
    unsigned channels = static_cast<unsigned>(std::strtoul(optionValue(args, "--channels", "2").c_str(), nullptr, 10));
    std::vector<std::pair<double, double>> tones;
    for (size_t i = 0; i + 1 < args.size(); i++) {
        if (args[i] != "--tone") continue;
        std::string spec = args[i + 1];
        size_t colon = spec.find(':');
        double f = std::strtod(spec.substr(0, colon).c_str(), nullptr);
        double a = colon == std::string::npos ? 0.25 : std::strtod(spec.substr(colon + 1).c_str(), nullptr);
        tones.push_back({f, a});
    }
    if (tones.empty()) tones = {{100.0, 0.25}, {1000.0, 0.25}};
    AudioData data;
    data.sampleRate = rate;
    size_t frames = static_cast<size_t>(rate * seconds);
    data.channels.assign(channels, std::vector<float>(frames, 0.0f));
    for (size_t i = 0; i < frames; i++) {
        double v = 0;
        for (auto& t : tones) v += t.second * std::sin(2 * M_PI * t.first * i / rate);
        for (unsigned c = 0; c < channels; c++) data.channels[c][i] = static_cast<float>(v);
    }
    std::string error;
    if (!writeWavFloat(pos[0], data, error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    return 0;
}

int cmdAnalyze(const Args& args) {
    std::vector<std::string> pos = positional(args);
    if (pos.size() < 2) {
        std::fprintf(stderr, "usage: eqapo analyze <file> FREQ [FREQ ...]   (prints dBFS of each tone per channel)\n");
        return 2;
    }
    std::string error;
    AudioData data = readAudioFile(pos[0], error);
    if (data.channels.empty()) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    // skip the first 10% to avoid filter transients
    size_t start = data.frames() / 10;
    for (size_t c = 0; c < data.channels.size(); c++) {
        std::printf("channel %zu:", c + 1);
        for (size_t i = 1; i < pos.size(); i++) {
            double freq = std::strtod(pos[i].c_str(), nullptr);
            const float* x = data.channels[c].data() + start;
            size_t n = data.frames() - start;
            double w = 2.0 * M_PI * freq / data.sampleRate;
            double coeff = 2.0 * std::cos(w), s1 = 0, s2 = 0;
            for (size_t k = 0; k < n; k++) {
                double s0 = x[k] + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
            double mag = 2.0 * std::sqrt(re * re + im * im) / n;
            std::printf("  %g Hz: %.2f dBFS", freq, 20 * std::log10(std::max(mag, 1e-12)));
        }
        std::printf("\n");
    }
    return 0;
}

}  // namespace eqapo::cli
