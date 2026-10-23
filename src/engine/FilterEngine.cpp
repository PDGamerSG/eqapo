#include "engine/FilterEngine.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>

#include "engine/ChannelHelper.h"
#include "engine/Log.h"
#include "engine/StringHelper.h"
#include "engine/factories/Factories.h"

namespace eqapo {

FilterEngine::FilterEngine() {
    // Same order as Equalizer APO so gating/precedence rules match.
    factories_.push_back(std::make_unique<DeviceFilterFactory>());
    factories_.push_back(std::make_unique<IncludeFilterFactory>());
    factories_.push_back(std::make_unique<StageFilterFactory>());
    factories_.push_back(std::make_unique<ChannelFilterFactory>());
    factories_.push_back(std::make_unique<IIRFilterFactory>());
    factories_.push_back(std::make_unique<BiQuadFilterFactory>());
    factories_.push_back(std::make_unique<PreampFilterFactory>());
    factories_.push_back(std::make_unique<DelayFilterFactory>());
    factories_.push_back(std::make_unique<CopyFilterFactory>());
    factories_.push_back(std::make_unique<ConvolutionFilterFactory>());
    factories_.push_back(std::make_unique<GraphicEQFilterFactory>());
}

FilterEngine::~FilterEngine() { deleteAllConfigs(); }

void FilterEngine::setDeviceInfo(const std::string& name, const std::string& uid) {
    deviceName_ = name;
    deviceUid_ = uid;
}

void FilterEngine::deleteAllConfigs() {
    delete currentConfig_.exchange(nullptr);
    delete nextConfig_.exchange(nullptr);
    delete previousConfig_.exchange(nullptr);
}

void FilterEngine::initialize(float sampleRate, unsigned inputChannelCount, unsigned outputChannelCount,
                              unsigned maxFrameCount) {
    std::lock_guard<std::mutex> lock(loadMutex_);
    deleteAllConfigs();
    sampleRate_ = sampleRate;
    inputChannelCount_ = inputChannelCount;
    outputChannelCount_ = outputChannelCount;
    maxFrameCount_ = maxFrameCount;
    transitionCounter_ = 0;
    transitionLength_ = static_cast<unsigned>(sampleRate / 100);
    for (auto& f : factories_) f->initialize(this);
    traceMessage(format("%u channels for this device: %s", std::max(inputChannelCount, outputChannelCount),
                        StringHelper::join(ChannelHelper::getChannelNames(std::max(inputChannelCount, outputChannelCount)), " ")
                            .c_str()));
}

void FilterEngine::setAudioRunning(bool running) { audioRunning_.store(running, std::memory_order_release); }

void FilterEngine::collectGarbage() { delete previousConfig_.exchange(nullptr); }

bool FilterEngine::loadConfig(const std::string& configFilePath) {
    std::lock_guard<std::mutex> lock(loadMutex_);
    auto start = std::chrono::steady_clock::now();

    // Wait for a previous transition to finish so at most one configuration is pending.
    if (audioRunning_.load(std::memory_order_acquire)) {
        auto deadline = start + std::chrono::seconds(3);
        while (nextConfig_.load(std::memory_order_acquire) != nullptr && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (nextConfig_.load(std::memory_order_acquire) != nullptr) {
            logMessage("Audio thread did not pick up the previous configuration; skipping reload");
            return false;
        }
    }
    collectGarbage();

    lastLoadErrors_.clear();
    allChannelNames_ = ChannelHelper::getChannelNames(std::max(inputChannelCount_, outputChannelCount_));
    currentChannelNames_ = allChannelNames_;
    filterInfos_.clear();

    for (auto& f : factories_) addFilters(f->startOfConfiguration());
    bool opened = std::ifstream(configFilePath).good();
    loadConfigFile(configFilePath);
    for (auto& f : factories_) addFilters(f->endOfConfiguration());

    auto config = std::make_unique<FilterConfiguration>(std::move(filterInfos_), inputChannelCount_, outputChannelCount_,
                                                        static_cast<unsigned>(allChannelNames_.size()), maxFrameCount_);
    filterInfos_.clear();
    loadedDescription_ = config->describe();
    publish(std::move(config));

    lastLoadMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    traceMessage(format("Finished loading configuration after %.3f milliseconds", lastLoadMs_));
    return opened;
}

void FilterEngine::publish(std::unique_ptr<FilterConfiguration> config) {
    FilterConfiguration* raw = config.release();
    if (!audioRunning_.load(std::memory_order_acquire)) {
        // No audio thread: replace immediately.
        delete nextConfig_.exchange(nullptr);
        delete currentConfig_.exchange(raw);
        return;
    }
    if (currentConfig_.load(std::memory_order_acquire) == nullptr) currentConfig_.store(raw, std::memory_order_release);
    else nextConfig_.store(raw, std::memory_order_release);
}

void FilterEngine::loadConfigFile(const std::string& path) {
    traceMessage("Loading configuration from " + path);
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::string msg = "Error while reading configuration file " + path + ": " + std::strerror(errno);
        logMessage(msg);
        lastLoadErrors_.push_back(msg);
        return;
    }
    std::stringstream content;
    content << file.rdbuf();
    std::string text = content.str();
    if (text.size() >= 3 && text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);  // UTF-8 BOM

    Channels savedChannelNames = currentChannelNames_;
    for (auto& f : factories_) addFilters(f->startOfFile(path));

    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string key = StringHelper::trim(line.substr(0, pos));  // allow indentation
        std::string value = line.substr(pos + 1);
        for (auto& f : factories_) {
            FilterList newFilters;
            try {
                newFilters = f->createFilter(path, key, value);
            } catch (const std::exception& e) {
                logMessage(e.what());
                lastLoadErrors_.push_back(e.what());
            }
            if (key.empty()) break;
            if (!newFilters.empty()) {
                addFilters(std::move(newFilters));
                break;
            }
        }
    }

    for (auto& f : factories_) addFilters(f->endOfFile(path));
    currentChannelNames_ = savedChannelNames;  // restore channels selected in outer configuration file
}

void FilterEngine::addFilters(FilterList filters) {
    for (auto& filter : filters) {
        FilterInfo info;
        info.inPlace = filter->getInPlace();
        Channels savedChannelNames = currentChannelNames_;
        if (filter->getAllChannels()) currentChannelNames_ = allChannelNames_;

        for (const std::string& name : currentChannelNames_) {
            auto pos = std::find(allChannelNames_.begin(), allChannelNames_.end(), name);
            info.inChannels.push_back(static_cast<size_t>(pos - allChannelNames_.begin()));
        }

        Channels newChannelNames = filter->initialize(sampleRate_, maxFrameCount_, currentChannelNames_);

        for (const std::string& name : newChannelNames) {
            auto pos = std::find(allChannelNames_.begin(), allChannelNames_.end(), name);
            if (pos == allChannelNames_.end()) {
                info.outChannels.push_back(allChannelNames_.size());
                allChannelNames_.push_back(name);
            } else {
                info.outChannels.push_back(static_cast<size_t>(pos - allChannelNames_.begin()));
            }
        }

        info.filter = std::move(filter);
        bool selectChannels = info.filter->getSelectChannels();
        filterInfos_.push_back(std::move(info));
        currentChannelNames_ = selectChannels ? newChannelNames : savedChannelNames;
    }
}

void FilterEngine::process(float** output, float** input, unsigned frameCount) {
    FilterConfiguration* current = currentConfig_.load(std::memory_order_acquire);
    FilterConfiguration* next = nextConfig_.load(std::memory_order_acquire);
    if (current == nullptr || (current->isEmpty() && next == nullptr)) {
        // avoid copying cost if no processing will happen anyway
        unsigned channels = std::min(inputChannelCount_, outputChannelCount_);
        for (unsigned c = 0; c < channels; c++)
            if (output[c] != input[c]) std::memcpy(output[c], input[c], frameCount * sizeof(float));
        for (unsigned c = channels; c < outputChannelCount_; c++) std::memset(output[c], 0, frameCount * sizeof(float));
        return;
    }

    unsigned done = 0;
    while (done < frameCount) {
        unsigned n = std::min(frameCount - done, maxFrameCount_);
        float* in[64];
        float* out[64];
        for (unsigned c = 0; c < inputChannelCount_ && c < 64; c++) in[c] = input[c] + done;
        for (unsigned c = 0; c < outputChannelCount_ && c < 64; c++) out[c] = output[c] + done;

        current->read(in, n);
        current->process(n);
        if (next) {
            next->read(in, n);
            next->process(n);
            transitionCounter_ = current->doTransition(*next, n, transitionCounter_, transitionLength_);
        }
        current->write(out, n);

        if (next && transitionCounter_ >= transitionLength_) {
            previousConfig_.store(current, std::memory_order_release);
            currentConfig_.store(next, std::memory_order_release);
            nextConfig_.store(nullptr, std::memory_order_release);
            transitionCounter_ = 0;
            current = next;
            next = nullptr;
        }
        done += n;
    }
}

std::vector<std::string> FilterEngine::describeLoaded() const { return loadedDescription_; }

}  // namespace eqapo
