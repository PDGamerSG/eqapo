#include "daemon/AudioPipeline.h"

#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#import <Foundation/Foundation.h>

#include <mach/mach_time.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

#include "engine/FilterEngine.h"
#include "engine/Log.h"

namespace eqapo {

namespace {
inline void enableFlushToZero() {
#if defined(__aarch64__)
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    if (!(fpcr & (1ull << 24))) {
        fpcr |= (1ull << 24);
        __asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
    }
#elif defined(__x86_64__)
    unsigned int csr = __builtin_ia32_stmxcsr();
    if ((csr & 0x8040) != 0x8040) __builtin_ia32_ldmxcsr(csr | 0x8040);
#endif
}

double machToMicros(uint64_t delta) {
    static mach_timebase_info_data_t info = {0, 0};
    if (info.denom == 0) mach_timebase_info(&info);
    return static_cast<double>(delta) * info.numer / info.denom / 1000.0;
}
}  // namespace

AudioPipeline::~AudioPipeline() { stop(); }

void AudioPipeline::setSyntheticSource(double freqA, double ampA, double freqB, double ampB) {
    synthetic_ = true;
    synthFreq_[0] = freqA;
    synthAmp_[0] = ampA;
    synthFreq_[1] = freqB;
    synthAmp_[1] = ampB;
    synthPhase_[0] = synthPhase_[1] = 0;
}

void AudioPipeline::enableOutputCapture(size_t maxFrames) {
    capture_.assign(maxFrames, 0.0f);
    captureFill_ = 0;
}

std::vector<float> AudioPipeline::capturedOutput() const {
    size_t n = std::min(captureFill_.load(), capture_.size());
    return std::vector<float>(capture_.begin(), capture_.begin() + static_cast<long>(n));
}

bool AudioPipeline::prepare(AudioObjectID device, unsigned bufferFrames, Format& format, std::string& error, bool withTap) {
    stop();
    deviceID_ = device;
    @autoreleasepool {
        std::string uid = ca::deviceUID(device);
        if (uid.empty()) {
            error = "device has no UID";
            return false;
        }
        NSString* deviceUID = [NSString stringWithUTF8String:uid.c_str()];

        NSString* tapUID = nil;
        AudioStreamBasicDescription tapFormat{};
        if (withTap) {
        // --- tap: every process except us, muted while we read it
        AudioObjectID ownProcess = ca::processObjectForPID(getpid());
        NSArray<NSNumber*>* exclude = ownProcess ? @[@(ownProcess)] : @[];
        CATapDescription* desc = [[CATapDescription alloc] initExcludingProcesses:exclude andDeviceUID:deviceUID withStream:0];
        desc.name = @"eqapo system tap";
        desc.privateTap = YES;
        desc.muteBehavior = CATapMutedWhenTapped;
        if (!ownProcess) {
            if (@available(macOS 26.0, *)) {
                desc.bundleIDs = @[@"com.eqapo.daemon"];
            } else {
                error = "could not resolve own audio process object (needed to avoid a feedback loop)";
                return false;
            }
        }
        OSStatus st = AudioHardwareCreateProcessTap(desc, &tapID_);
        if (st != noErr || tapID_ == 0) {
            tapID_ = 0;
            error = "AudioHardwareCreateProcessTap failed: " + ca::statusString(st) +
                    (st == kAudioHardwareIllegalOperationError || st == kAudioHardwareNotRunningError || st == 560947818
                         ? " (grant eqapo 'System Audio Recording Only' in System Settings > Privacy & Security)"
                         : "");
            return false;
        }

        // tap UID + format
        CFStringRef tapUIDRef = nullptr;
        AudioObjectPropertyAddress tapUIDAddr{kAudioTapPropertyUID, kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
        UInt32 size = sizeof(tapUIDRef);
        st = AudioObjectGetPropertyData(tapID_, &tapUIDAddr, 0, nullptr, &size, &tapUIDRef);
        if (st != noErr || !tapUIDRef) {
            error = "cannot read tap UID: " + ca::statusString(st);
            teardown();
            return false;
        }
        tapUID = (__bridge_transfer NSString*)tapUIDRef;

        AudioObjectPropertyAddress tapFmtAddr{kAudioTapPropertyFormat, kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
        size = sizeof(tapFormat);
        AudioObjectGetPropertyData(tapID_, &tapFmtAddr, 0, nullptr, &size, &tapFormat);
        }

        // --- aggregate: output device (clock) + tap
        NSMutableDictionary* aggDesc = [@{
            @kAudioAggregateDeviceUIDKey : [NSString stringWithFormat:@"com.eqapo.aggregate.%d", getpid()],
            @kAudioAggregateDeviceNameKey : @"eqapo",
            @kAudioAggregateDeviceMainSubDeviceKey : deviceUID,
            @kAudioAggregateDeviceIsPrivateKey : @YES,
            @kAudioAggregateDeviceIsStackedKey : @NO,
            @kAudioAggregateDeviceTapAutoStartKey : @YES,
            @kAudioAggregateDeviceSubDeviceListKey : @[ @{@kAudioSubDeviceUIDKey : deviceUID} ],
        } mutableCopy];
        if (tapUID)
            aggDesc[@kAudioAggregateDeviceTapListKey] = @[ @{@kAudioSubTapUIDKey : tapUID, @kAudioSubTapDriftCompensationKey : @NO} ];
        OSStatus st = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggDesc, &aggregateID_);
        if (st != noErr || aggregateID_ == 0) {
            aggregateID_ = 0;
            error = "AudioHardwareCreateAggregateDevice failed: " + ca::statusString(st);
            teardown();
            return false;
        }

        if (bufferFrames > 0 && !ca::setBufferFrameSize(aggregateID_, bufferFrames))
            logMessage(eqapo::format("could not set buffer size %u on aggregate device", bufferFrames));

        format.sampleRate = ca::nominalSampleRate(aggregateID_);
        if (format.sampleRate <= 0) format.sampleRate = ca::nominalSampleRate(device);
        format.bufferFrames = ca::bufferFrameSize(aggregateID_);
        format.inputChannels = ca::channelCount(aggregateID_, kAudioObjectPropertyScopeInput);
        format.outputChannels = ca::channelCount(aggregateID_, kAudioObjectPropertyScopeOutput);
        if (format.inputChannels == 0) format.inputChannels = tapFormat.mChannelsPerFrame;
        if (format.inputChannels == 0) format.inputChannels = format.outputChannels;
        std::string inDesc, outDesc;
        bool inFloat = withTap ? ca::isFloat32(aggregateID_, kAudioObjectPropertyScopeInput, inDesc) : true;
        if (!withTap) inDesc = "none (synthetic)";
        bool outFloat = ca::isFloat32(aggregateID_, kAudioObjectPropertyScopeOutput, outDesc);
        format.inputFormat = inDesc;
        format.outputFormat = outDesc;
        if (format.outputChannels == 0 || format.bufferFrames == 0 || format.sampleRate <= 0) {
            error = eqapo::format("unusable aggregate format (rate %g, out ch %u, frames %u)", format.sampleRate,
                                  format.outputChannels, format.bufferFrames);
            teardown();
            return false;
        }
        if (!inFloat || !outFloat) {
            error = "streams are not 32-bit float (input: " + inDesc + "; output: " + outDesc + ")";
            teardown();
            return false;
        }
        format_ = format;
        return true;
    }
}

bool AudioPipeline::startIO(FilterEngine& engine, std::string& error) {
    if (aggregateID_ == 0) {
        error = "pipeline not prepared";
        return false;
    }
    engine_ = &engine;
    unsigned maxChannels = std::max(format_.inputChannels, format_.outputChannels);
    inPlanar_.assign(maxChannels, std::vector<float>(format_.bufferFrames * 2, 0.0f));
    outPlanar_.assign(maxChannels, std::vector<float>(format_.bufferFrames * 2, 0.0f));
    inPtrs_.resize(maxChannels);
    outPtrs_.resize(maxChannels);
    for (unsigned c = 0; c < maxChannels; c++) {
        inPtrs_[c] = inPlanar_[c].data();
        outPtrs_[c] = outPlanar_[c].data();
    }
    blockMicros_ = format_.bufferFrames / format_.sampleRate * 1e6;
    callbacks_ = overloads_ = frameMismatches_ = 0;
    maxMicros_ = 0;
    inputPeak_ = outputPeak_ = 0;

    AudioObjectPropertyAddress overloadAddr{kAudioDeviceProcessorOverload, kAudioObjectPropertyScopeGlobal,
                                            kAudioObjectPropertyElementMain};
    overloadListener_ = ca::PropertyListener(aggregateID_, overloadAddr, nullptr, [this] { overloads_++; });

    AudioPipeline* self = this;
    OSStatus st = AudioDeviceCreateIOProcIDWithBlock(
        &procID_, aggregateID_, nullptr,
        ^(const AudioTimeStamp*, const AudioBufferList* inInputData, const AudioTimeStamp*, AudioBufferList* outOutputData,
          const AudioTimeStamp*) {
          self->ioCallback(inInputData, outOutputData);
        });
    if (st != noErr) {
        error = "AudioDeviceCreateIOProcIDWithBlock failed: " + ca::statusString(st);
        procID_ = nullptr;
        return false;
    }
    st = AudioDeviceStart(aggregateID_, procID_);
    if (st != noErr) {
        error = "AudioDeviceStart failed: " + ca::statusString(st);
        AudioDeviceDestroyIOProcID(aggregateID_, procID_);
        procID_ = nullptr;
        return false;
    }
    running_ = true;
    return true;
}

void AudioPipeline::ioCallback(const AudioBufferList* input, AudioBufferList* output) {
    uint64_t t0 = mach_absolute_time();
    enableFlushToZero();

    if (!output || output->mNumberBuffers == 0) return;
    const unsigned outCh0 = output->mBuffers[0].mNumberChannels ? output->mBuffers[0].mNumberChannels : 1;
    unsigned frames = output->mBuffers[0].mDataByteSize / (outCh0 * sizeof(float));
    const unsigned capacity = static_cast<unsigned>(inPlanar_.empty() ? 0 : inPlanar_[0].size());
    if (frames > capacity) {
        frameMismatches_++;
        frames = capacity;
    }
    if (frames == 0) return;

    // deinterleave tap input into planar buffers
    unsigned ch = 0;
    const unsigned maxCh = static_cast<unsigned>(inPlanar_.size());
    if (synthetic_) {
        const double twoPi = 2.0 * M_PI, sr = format_.sampleRate;
        for (unsigned i = 0; i < frames; i++) {
            double v = synthAmp_[0] * std::sin(synthPhase_[0]) + synthAmp_[1] * std::sin(synthPhase_[1]);
            synthPhase_[0] += twoPi * synthFreq_[0] / sr;
            synthPhase_[1] += twoPi * synthFreq_[1] / sr;
            if (synthPhase_[0] > twoPi) synthPhase_[0] -= twoPi;
            if (synthPhase_[1] > twoPi) synthPhase_[1] -= twoPi;
            for (unsigned c = 0; c < maxCh; c++) inPtrs_[c][i] = static_cast<float>(v);
        }
        ch = maxCh;
    } else if (input) {
        for (UInt32 b = 0; b < input->mNumberBuffers && ch < maxCh; b++) {
            const AudioBuffer& buf = input->mBuffers[b];
            const float* src = static_cast<const float*>(buf.mData);
            unsigned n = buf.mNumberChannels;
            unsigned avail = buf.mDataByteSize / (n * sizeof(float));
            unsigned count = std::min(frames, avail);
            if (!src) {
                for (unsigned c = 0; c < n && ch < maxCh; c++, ch++) std::memset(inPtrs_[ch], 0, frames * sizeof(float));
                continue;
            }
            if (n == 1) {
                std::memcpy(inPtrs_[ch], src, count * sizeof(float));
                if (count < frames) std::memset(inPtrs_[ch] + count, 0, (frames - count) * sizeof(float));
                ch++;
            } else {
                for (unsigned c = 0; c < n && ch < maxCh; c++, ch++) {
                    float* dst = inPtrs_[ch];
                    for (unsigned i = 0; i < count; i++) dst[i] = src[i * n + c];
                    if (count < frames) std::memset(dst + count, 0, (frames - count) * sizeof(float));
                }
            }
        }
    }
    for (; ch < format_.inputChannels && ch < maxCh; ch++) std::memset(inPtrs_[ch], 0, frames * sizeof(float));

    // level meters (cheap: one channel, every 8th sample)
    float inPeak = inputPeak_.load(std::memory_order_relaxed);
    for (unsigned i = 0; i < frames; i += 8) inPeak = std::max(inPeak, std::fabs(inPtrs_[0][i]));
    inputPeak_.store(inPeak, std::memory_order_relaxed);

    engine_->process(outPtrs_.data(), inPtrs_.data(), frames);

    float outPeak = outputPeak_.load(std::memory_order_relaxed);
    for (unsigned i = 0; i < frames; i += 8) outPeak = std::max(outPeak, std::fabs(outPtrs_[0][i]));
    outputPeak_.store(outPeak, std::memory_order_relaxed);
    if (!capture_.empty()) {
        size_t fill = captureFill_.load(std::memory_order_relaxed);
        size_t n = std::min<size_t>(frames, capture_.size() - fill);
        if (n > 0) {
            std::memcpy(capture_.data() + fill, outPtrs_[0], n * sizeof(float));
            captureFill_.store(fill + n, std::memory_order_release);
        }
    }

    // interleave into the device output buffers
    ch = 0;
    for (UInt32 b = 0; b < output->mNumberBuffers; b++) {
        AudioBuffer& buf = output->mBuffers[b];
        float* dst = static_cast<float*>(buf.mData);
        unsigned n = buf.mNumberChannels;
        if (!dst || n == 0) continue;
        unsigned avail = buf.mDataByteSize / (n * sizeof(float));
        unsigned count = std::min(frames, avail);
        if (n == 1) {
            if (ch < format_.outputChannels && ch < maxCh) std::memcpy(dst, outPtrs_[ch], count * sizeof(float));
            else std::memset(dst, 0, count * sizeof(float));
            ch++;
        } else {
            for (unsigned c = 0; c < n; c++, ch++) {
                if (ch < format_.outputChannels && ch < maxCh) {
                    const float* src = outPtrs_[ch];
                    for (unsigned i = 0; i < count; i++) dst[i * n + c] = src[i];
                } else {
                    for (unsigned i = 0; i < count; i++) dst[i * n + c] = 0.0f;
                }
            }
        }
    }

    double micros = machToMicros(mach_absolute_time() - t0);
    lastMicros_.store(micros, std::memory_order_relaxed);
    if (micros > maxMicros_.load(std::memory_order_relaxed)) maxMicros_.store(micros, std::memory_order_relaxed);
    double load = blockMicros_ > 0 ? micros / blockMicros_ * 100.0 : 0;
    avgLoad_.store(avgLoad_.load(std::memory_order_relaxed) * 0.99 + load * 0.01, std::memory_order_relaxed);
    callbacks_.fetch_add(1, std::memory_order_relaxed);
}

void AudioPipeline::stop() {
    if (procID_ && aggregateID_) {
        AudioDeviceStop(aggregateID_, procID_);
        AudioDeviceDestroyIOProcID(aggregateID_, procID_);
    }
    procID_ = nullptr;
    running_ = false;
    overloadListener_.reset();
    teardown();
}

void AudioPipeline::teardown() {
    if (aggregateID_) {
        AudioHardwareDestroyAggregateDevice(aggregateID_);
        aggregateID_ = 0;
    }
    if (tapID_) {
        AudioHardwareDestroyProcessTap(tapID_);
        tapID_ = 0;
    }
    engine_ = nullptr;
}

AudioPipeline::Stats AudioPipeline::stats() const {
    Stats s;
    s.callbacks = callbacks_.load();
    s.overloads = overloads_.load();
    s.frameMismatches = frameMismatches_.load();
    s.lastMicros = lastMicros_.load();
    s.maxMicros = maxMicros_.load();
    s.avgLoadPercent = avgLoad_.load();
    s.inputPeak = inputPeak_.load();
    s.outputPeak = outputPeak_.load();
    return s;
}

}  // namespace eqapo
