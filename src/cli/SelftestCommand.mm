// `eqapo selftest`: live end-to-end measurement of the processing pipeline.
//
// 1. Starts the full pipeline in this process with a known test configuration
//    (+6 dB at 100 Hz, deep notch at 1 kHz).
// 2. Plays a 100 Hz + 1 kHz tone with `afplay` (a separate process, so it is tapped).
// 3. Captures what *this* process sends to the device with a second, non-muting
//    process tap and measures both tones. Pass: 1 kHz down > 20 dB, 100 Hz up 5-7 dB.
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#import <Foundation/Foundation.h>

#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#include "audio/AudioFile.h"
#include "cli/Commands.h"
#include "daemon/AudioPipeline.h"
#include "daemon/CoreAudioUtil.h"
#include "daemon/Paths.h"
#include "engine/FilterEngine.h"
#include "engine/Log.h"

extern char** environ;

namespace eqapo::cli {

void installImpulseLoader();

namespace {

// Captures the output of one process (ours) through a non-muting tap in a tap-only aggregate.
class ProcessCapture {
public:
    bool start(AudioObjectID device, pid_t pid, std::string& error) {
        @autoreleasepool {
            AudioObjectID proc = ca::processObjectForPID(pid);
            if (!proc) {
                error = "no audio process object for pid";
                return false;
            }
            NSString* uid = [NSString stringWithUTF8String:ca::deviceUID(device).c_str()];
            CATapDescription* desc = [[CATapDescription alloc] initWithProcesses:@[@(proc)] andDeviceUID:uid withStream:0];
            desc.name = @"eqapo selftest tap";
            desc.privateTap = YES;
            desc.muteBehavior = CATapUnmuted;
            OSStatus st = AudioHardwareCreateProcessTap(desc, &tap_);
            if (st != noErr) {
                error = "selftest tap failed: " + ca::statusString(st);
                return false;
            }
            CFStringRef tapUID = nullptr;
            AudioObjectPropertyAddress a{kAudioTapPropertyUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
            UInt32 size = sizeof(tapUID);
            AudioObjectGetPropertyData(tap_, &a, 0, nullptr, &size, &tapUID);
            NSDictionary* agg = @{
                @kAudioAggregateDeviceUIDKey : [NSString stringWithFormat:@"com.eqapo.selftest.%d", getpid()],
                @kAudioAggregateDeviceNameKey : @"eqapo selftest",
                @kAudioAggregateDeviceIsPrivateKey : @YES,
                @kAudioAggregateDeviceTapAutoStartKey : @YES,
                @kAudioAggregateDeviceTapListKey : @[ @{@kAudioSubTapUIDKey : (__bridge_transfer NSString*)tapUID} ],
            };
            st = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)agg, &agg_);
            if (st != noErr) {
                error = "selftest aggregate failed: " + ca::statusString(st);
                return false;
            }
            sampleRate_ = ca::nominalSampleRate(agg_);
            channels_ = ca::channelCount(agg_, kAudioObjectPropertyScopeInput);
            if (channels_ == 0) channels_ = 2;
            ProcessCapture* self = this;
            st = AudioDeviceCreateIOProcIDWithBlock(&proc_, agg_, nullptr,
                                                    ^(const AudioTimeStamp*, const AudioBufferList* in, const AudioTimeStamp*,
                                                      AudioBufferList*, const AudioTimeStamp*) {
                                                      self->onInput(in);
                                                    });
            if (st != noErr) {
                error = "selftest IOProc failed: " + ca::statusString(st);
                return false;
            }
            st = AudioDeviceStart(agg_, proc_);
            if (st != noErr) {
                error = "selftest start failed: " + ca::statusString(st);
                return false;
            }
            return true;
        }
    }
    void stop() {
        if (proc_ && agg_) {
            AudioDeviceStop(agg_, proc_);
            AudioDeviceDestroyIOProcID(agg_, proc_);
            proc_ = nullptr;
        }
        if (agg_) AudioHardwareDestroyAggregateDevice(agg_), agg_ = 0;
        if (tap_) AudioHardwareDestroyProcessTap(tap_), tap_ = 0;
    }
    std::vector<float> firstChannel() {
        std::lock_guard<std::mutex> lock(mutex_);
        return captured_;
    }
    double sampleRate() const { return sampleRate_; }

private:
    void onInput(const AudioBufferList* in) {
        if (!in || in->mNumberBuffers == 0) return;
        const AudioBuffer& b = in->mBuffers[0];
        const float* src = static_cast<const float*>(b.mData);
        unsigned n = b.mNumberChannels ? b.mNumberChannels : 1;
        unsigned frames = b.mDataByteSize / (n * sizeof(float));
        std::lock_guard<std::mutex> lock(mutex_);  // selftest only; not a production path
        for (unsigned i = 0; i < frames; i++) captured_.push_back(src[i * n]);
    }
    AudioObjectID tap_ = 0, agg_ = 0;
    AudioDeviceIOProcID proc_ = nullptr;
    double sampleRate_ = 0;
    unsigned channels_ = 0;
    std::mutex mutex_;
    std::vector<float> captured_;
};

double toneDb(const std::vector<float>& x, size_t from, double freq, double sr) {
    if (x.size() <= from + 100) return -200;
    size_t n = x.size() - from;
    double w = 2.0 * M_PI * freq / sr, coeff = 2.0 * std::cos(w), s1 = 0, s2 = 0;
    for (size_t i = from; i < x.size(); i++) {
        double s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
    double mag = 2.0 * std::sqrt(re * re + im * im) / n;
    return 20 * std::log10(std::max(mag, 1e-12));
}

pid_t spawn(const std::vector<std::string>& argv) {
    std::vector<char*> av;
    for (const std::string& a : argv) av.push_back(const_cast<char*>(a.c_str()));
    av.push_back(nullptr);
    pid_t pid = 0;
    if (posix_spawnp(&pid, av[0], nullptr, nullptr, av.data(), environ) != 0) return -1;
    return pid;
}
}  // namespace

namespace {
// Tap-free variant: the pipeline feeds a generated tone into the engine inside the real device
// IOProc and records what it writes to the device. Verifies device I/O, real-time processing and
// the filter response without needing the System Audio Recording permission.
int syntheticSelftest(AudioObjectID device, const std::string& cfg, double seconds) {
    AudioPipeline pipeline;
    AudioPipeline::Format fmt;
    std::string error;
    if (!pipeline.prepare(device, 0, fmt, error, /*withTap=*/false)) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }
    FilterEngine engine;
    engine.setDeviceInfo(ca::deviceName(device), ca::deviceUID(device));
    engine.initialize(static_cast<float>(fmt.sampleRate), fmt.inputChannels, fmt.outputChannels, fmt.bufferFrames);
    engine.loadConfig(cfg);
    const double amp = 0.2;
    pipeline.setSyntheticSource(100.0, amp, 1000.0, amp);
    pipeline.enableOutputCapture(static_cast<size_t>(fmt.sampleRate * (seconds + 1)));
    if (!pipeline.startIO(engine, error)) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }
    engine.setAudioRunning(true);
    std::printf("pipeline (synthetic source, no tap): %g Hz, %u out channels, %u frames, %zu filters\n", fmt.sampleRate,
                fmt.outputChannels, fmt.bufferFrames, engine.loadedFilterCount());
    std::printf("playing a generated 100 Hz + 1 kHz tone through the equalizer for %.0f s ...\n", seconds);
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(seconds * 1000)));
    std::vector<float> captured = pipeline.capturedOutput();
    pipeline.stop();
    AudioPipeline::Stats st = pipeline.stats();
    double sr = fmt.sampleRate;
    size_t skip = static_cast<size_t>(sr * 1.0);
    double in = 20 * std::log10(amp);
    double out100 = toneDb(captured, skip, 100, sr), out1k = toneDb(captured, skip, 1000, sr);
    std::printf("captured %.1f s of device output; %llu blocks, %llu overloads, avg load %.2f%% (max %.0f us/block)\n",
                captured.size() / sr, st.callbacks, st.overloads, st.avgLoadPercent, st.maxMicros);
    std::printf("  100 Hz: source %.1f dBFS -> output %.1f dBFS  (%+.1f dB, expected +6)\n", in, out100, out100 - in);
    std::printf("  1 kHz : source %.1f dBFS -> output %.1f dBFS  (%+.1f dB, expected < -20)\n", in, out1k, out1k - in);
    bool pass = captured.size() > skip + sr && (out100 - in) > 5.0 && (out100 - in) < 7.0 && (out1k - in) < -20.0 &&
                st.overloads == 0;
    std::printf("%s\n", pass ? "PASS: real-time pipeline on the device applies the configured EQ" : "FAIL");
    return pass ? 0 : 1;
}
}  // namespace

int cmdSelftest(const Args& args) {
    double seconds = std::strtod(optionValue(args, "--seconds", "4").c_str(), nullptr);
    bool synthetic = hasFlag(args, "--synthetic");
    std::string pidText = Paths::readFile(Paths::pidFile());
    int daemonPid = std::atoi(pidText.c_str());
    if (daemonPid > 0 && kill(daemonPid, 0) == 0 && !hasFlag(args, "--force")) {
        std::fprintf(stderr,
                     "A daemon is running (pid %d). Stop it first (`eqapo stop`) so the selftest can own the device, "
                     "or pass --force.\n",
                     daemonPid);
        return 1;
    }
    setLogSink([](LogLevel level, const std::string& m) {
        if (level != LogLevel::Trace) std::fprintf(stderr, "  [%s] %s\n", level == LogLevel::Error ? "error" : "info", m.c_str());
    });
    installImpulseLoader();

    AudioObjectID device = ca::defaultOutputDevice();
    if (!device) {
        std::fprintf(stderr, "no default output device\n");
        return 1;
    }
    std::printf("selftest on \"%s\" (%s)\n", ca::deviceName(device).c_str(),
                ca::transportTypeName(ca::transportType(device)).c_str());

    // test configuration
    std::string dir = "/tmp/eqapo-selftest-" + std::to_string(getpid());
    Paths::ensureDirectory(dir);
    std::string cfg = dir + "/config.txt";
    Paths::writeFile(cfg, "Filter: ON PK Fc 100 Hz Gain 6 dB Q 2\nFilter: ON NO Fc 1000 Hz Q 30\nFilter: ON NO Fc 1000 Hz Q 10\n");
    if (synthetic) {
        int rc = syntheticSelftest(device, cfg, seconds);
        unlink(cfg.c_str());
        rmdir(dir.c_str());
        return rc;
    }

    // pipeline
    AudioPipeline pipeline;
    AudioPipeline::Format fmt;
    std::string error;
    if (!pipeline.prepare(device, 0, fmt, error)) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }
    FilterEngine engine;
    engine.setDeviceInfo(ca::deviceName(device), ca::deviceUID(device));
    engine.initialize(static_cast<float>(fmt.sampleRate), fmt.inputChannels, fmt.outputChannels, fmt.bufferFrames);
    engine.loadConfig(cfg);
    if (!pipeline.startIO(engine, error)) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }
    engine.setAudioRunning(true);
    std::printf("pipeline: %g Hz, %u in / %u out, %u frames, %zu filters\n", fmt.sampleRate, fmt.inputChannels,
                fmt.outputChannels, fmt.bufferFrames, engine.loadedFilterCount());

    // capture of our own output
    ProcessCapture capture;
    if (!capture.start(device, getpid(), error)) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        pipeline.stop();
        return 1;
    }

    // tone file and player
    std::string wav = dir + "/tone.wav";
    AudioData tone;
    tone.sampleRate = fmt.sampleRate;
    size_t frames = static_cast<size_t>(fmt.sampleRate * (seconds + 1));
    tone.channels.assign(2, std::vector<float>(frames));
    const float amp = 0.2f;
    for (size_t i = 0; i < frames; i++) {
        float v = amp * static_cast<float>(std::sin(2 * M_PI * 100 * i / fmt.sampleRate) + std::sin(2 * M_PI * 1000 * i / fmt.sampleRate));
        tone.channels[0][i] = tone.channels[1][i] = v;
    }
    writeWavFloat(wav, tone, error);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    pid_t player = spawn({"/usr/bin/afplay", wav});
    if (player <= 0) {
        std::fprintf(stderr, "FAIL: could not start afplay\n");
        capture.stop();
        pipeline.stop();
        return 1;
    }
    std::printf("playing 100 Hz + 1 kHz test tone for %.0f s through the equalizer ...\n", seconds);
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(seconds * 1000)));
    kill(player, SIGTERM);
    int status = 0;
    waitpid(player, &status, 0);

    std::vector<float> captured = capture.firstChannel();
    double sr = capture.sampleRate() > 0 ? capture.sampleRate() : fmt.sampleRate;
    capture.stop();
    pipeline.stop();
    AudioPipeline::Stats st = pipeline.stats();

    size_t skip = static_cast<size_t>(sr * 1.0);  // ignore first second (startup, filter transient)
    double in100 = 20 * std::log10(amp), in1k = 20 * std::log10(amp);
    double out100 = toneDb(captured, skip, 100, sr), out1k = toneDb(captured, skip, 1000, sr);
    double d100 = out100 - in100, d1k = out1k - in1k;
    std::printf("captured %.1f s of our output; %llu blocks, %llu overloads, avg load %.2f%% (max %.0f us/block)\n",
                captured.size() / sr, st.callbacks, st.overloads, st.avgLoadPercent, st.maxMicros);
    std::printf("  100 Hz: source %.1f dBFS -> output %.1f dBFS  (%+.1f dB, expected +6)\n", in100, out100, d100);
    std::printf("  1 kHz : source %.1f dBFS -> output %.1f dBFS  (%+.1f dB, expected < -20)\n", in1k, out1k, d1k);
    bool pass = captured.size() > skip + sr && d100 > 5.0 && d100 < 7.0 && d1k < -20.0;
    std::printf("%s\n", pass ? "PASS: the equalizer changes the audio as configured" : "FAIL");
    if (!pass && captured.size() <= skip + sr) std::printf("  (too little audio captured; is the output device muted or asleep?)\n");
    unlink(wav.c_str());
    unlink(cfg.c_str());
    rmdir(dir.c_str());
    return pass ? 0 : 1;
}

}  // namespace eqapo::cli
