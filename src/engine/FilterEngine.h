#pragma once
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "engine/FilterConfiguration.h"
#include "engine/IFilterFactory.h"
#include "engine/expr/Expression.h"

namespace eqapo {

// Loads Equalizer APO configuration files into FilterConfigurations and runs the
// active one on the audio thread, crossfading to a newly loaded one.
//
// Threads: initialize()/loadConfig()/collectGarbage() on one control thread;
// process() on the audio thread. The two never block each other: configs are
// handed over through atomic pointers.
class FilterEngine {
public:
    FilterEngine();
    ~FilterEngine();
    FilterEngine(const FilterEngine&) = delete;
    FilterEngine& operator=(const FilterEngine&) = delete;

    // Device identity used by `Device:` lines (matched against "<name> <uid>").
    void setDeviceInfo(const std::string& name, const std::string& uid);
    // Must be called while the audio thread is not running. Drops all loaded configurations.
    void initialize(float sampleRate, unsigned inputChannelCount, unsigned outputChannelCount, unsigned maxFrameCount);

    // Parses `configFilePath` (plus includes) and publishes the result. Returns false when
    // the file could not be opened (an empty configuration is published in that case).
    bool loadConfig(const std::string& configFilePath);
    // Parses one file into the configuration being built (used by Include).
    void loadConfigFile(const std::string& path);

    // Tells loadConfig whether process() is being called; when false, new configurations are
    // swapped in immediately instead of crossfaded.
    void setAudioRunning(bool running);
    bool transitionPending() const { return nextConfig_.load(std::memory_order_acquire) != nullptr; }
    // Deletes a configuration retired by the audio thread. Control thread only.
    void collectGarbage();

    // Audio thread. `input` has inputChannelCount planar channels, `output` outputChannelCount.
    void process(float** output, float** input, unsigned frameCount);

    // --- information for factories
    float sampleRate() const { return sampleRate_; }
    unsigned inputChannelCount() const { return inputChannelCount_; }
    unsigned outputChannelCount() const { return outputChannelCount_; }
    unsigned maxFrameCount() const { return maxFrameCount_; }
    const std::string& deviceName() const { return deviceName_; }
    const std::string& deviceUid() const { return deviceUid_; }
    std::string deviceString() const { return deviceName_ + " " + deviceUid_; }
    // Expression evaluator shared by the If/Eval/inline factories (variables reset per load).
    ExpressionEvaluator& parser() { return evaluator_; }
    // Channel selection during parsing.
    const Channels& currentChannelNames() const { return currentChannelNames_; }
    const Channels& allChannelNames() const { return allChannelNames_; }

    // --- status (control thread)
    std::vector<std::string> describeLoaded() const;  // description of the most recently loaded configuration
    size_t loadedFilterCount() const { return loadedDescription_.size(); }
    double lastLoadMilliseconds() const { return lastLoadMs_; }
    const std::vector<std::string>& lastLoadErrors() const { return lastLoadErrors_; }

private:
    void addFilters(FilterList filters);
    void publish(std::unique_ptr<FilterConfiguration> config);
    void deleteAllConfigs();

    std::vector<std::unique_ptr<IFilterFactory>> factories_;
    ExpressionEvaluator evaluator_;

    std::string deviceName_, deviceUid_;
    float sampleRate_ = 48000;
    unsigned inputChannelCount_ = 2;
    unsigned outputChannelCount_ = 2;
    unsigned maxFrameCount_ = 512;
    unsigned transitionLength_ = 480;

    // parse state (control thread)
    std::vector<FilterInfo> filterInfos_;
    Channels currentChannelNames_;
    Channels allChannelNames_;
    std::vector<std::string> loadedDescription_;
    std::vector<std::string> lastLoadErrors_;
    double lastLoadMs_ = 0;
    std::mutex loadMutex_;

    // shared with audio thread
    std::atomic<FilterConfiguration*> currentConfig_{nullptr};
    std::atomic<FilterConfiguration*> nextConfig_{nullptr};
    std::atomic<FilterConfiguration*> previousConfig_{nullptr};
    std::atomic<bool> audioRunning_{false};
    unsigned transitionCounter_ = 0;  // audio thread only
};

}  // namespace eqapo
