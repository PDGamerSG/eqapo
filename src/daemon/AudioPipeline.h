#pragma once
#include <CoreAudio/CoreAudio.h>

#include <atomic>
#include <string>
#include <vector>

#include "daemon/CoreAudioUtil.h"

namespace eqapo {

class FilterEngine;

// Process tap (all processes except ours, muted while tapped) + private aggregate
// device {output device, tap} + one IOProc that runs the FilterEngine.
class AudioPipeline {
public:
    struct Format {
        double sampleRate = 0;
        unsigned inputChannels = 0;   // tap channels
        unsigned outputChannels = 0;  // device channels
        unsigned bufferFrames = 0;
        std::string inputFormat, outputFormat;
    };
    struct Stats {
        uint64_t callbacks = 0;
        uint64_t overloads = 0;
        uint64_t frameMismatches = 0;
        double lastMicros = 0;
        double maxMicros = 0;
        double avgLoadPercent = 0;  // callback time / block duration
        double inputPeak = 0, outputPeak = 0;
    };

    AudioPipeline() = default;
    ~AudioPipeline();
    AudioPipeline(const AudioPipeline&) = delete;
    AudioPipeline& operator=(const AudioPipeline&) = delete;

    // Step 1: create tap + aggregate for `device`; fills `format`. bufferFrames 0 = device default.
    // withTap=false builds the aggregate without a tap (input is then silence or the synthetic source).
    bool prepare(AudioObjectID device, unsigned bufferFrames, Format& format, std::string& error, bool withTap = true);
    // Test support: replace the input with two generated tones and record output channel 0.
    void setSyntheticSource(double freqA, double ampA, double freqB, double ampB);
    void enableOutputCapture(size_t maxFrames);
    std::vector<float> capturedOutput() const;
    // Step 2: install the IOProc and start. The engine must already be initialised for `format`.
    bool startIO(FilterEngine& engine, std::string& error);
    void stop();
    bool running() const { return running_.load(); }
    Stats stats() const;
    AudioObjectID aggregateID() const { return aggregateID_; }
    AudioObjectID tapID() const { return tapID_; }
    AudioObjectID deviceID() const { return deviceID_; }

private:
    void ioCallback(const AudioBufferList* input, AudioBufferList* output);
    void teardown();

    AudioObjectID deviceID_ = 0;
    AudioObjectID tapID_ = 0;
    AudioObjectID aggregateID_ = 0;
    AudioDeviceIOProcID procID_ = nullptr;
    FilterEngine* engine_ = nullptr;
    Format format_;
    std::vector<std::vector<float>> inPlanar_, outPlanar_;
    std::vector<float*> inPtrs_, outPtrs_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> callbacks_{0}, overloads_{0}, frameMismatches_{0};
    std::atomic<double> lastMicros_{0}, maxMicros_{0}, avgLoad_{0};
    std::atomic<float> inputPeak_{0}, outputPeak_{0};
    double blockMicros_ = 0;
    ca::PropertyListener overloadListener_;
    // synthetic source / capture (selftest only)
    bool synthetic_ = false;
    double synthFreq_[2] = {0, 0}, synthAmp_[2] = {0, 0}, synthPhase_[2] = {0, 0};
    std::vector<float> capture_;
    std::atomic<size_t> captureFill_{0};
};

}  // namespace eqapo
